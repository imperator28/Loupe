#include "protocol/ProtocolFrame.h"

#include <QCoreApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QProcess>
#include <QUuid>

namespace {

constexpr int kConnectTimeoutMs = 5'000;
constexpr int kImportTimeoutMs = 10 * 60 * 1'000;

QString workerPath()
{
    auto name = QStringLiteral("loupe-worker");
#ifdef Q_OS_WIN
    name += QStringLiteral(".exe");
#endif
    return QCoreApplication::applicationDirPath() + QLatin1Char('/') + name;
}

bool sendOpen(QLocalSocket& socket, const QString& path)
{
    const auto payload = QJsonDocument(QJsonObject{
        {QStringLiteral("version"), QJsonObject{{QStringLiteral("major"), 2}, {QStringLiteral("minor"), 0}}},
        {QStringLiteral("type"), QStringLiteral("openFile")},
        {QStringLiteral("requestId"), 1},
        {QStringLiteral("path"), path},
    }).toJson(QJsonDocument::Compact) + '\n';
    socket.write(loupe::protocol::encodeFrame(loupe::protocol::FrameType::ControlJson, payload));
    return socket.waitForBytesWritten(kConnectTimeoutMs);
}

std::optional<QJsonObject> benchmarkFile(const QString& sourcePath)
{
    const auto socketName = QStringLiteral("/tmp/loupe-benchmark-%1-%2")
        .arg(QCoreApplication::applicationPid())
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
    QProcess worker;
    worker.start(workerPath(), {QStringLiteral("--server-name"), socketName});
    if (!worker.waitForStarted(kConnectTimeoutMs)) return std::nullopt;

    QLocalSocket socket;
    QElapsedTimer connectTimer;
    connectTimer.start();
    while (connectTimer.elapsed() < kConnectTimeoutMs) {
        socket.connectToServer(socketName);
        if (socket.waitForConnected(100)) break;
        socket.abort();
    }
    if (socket.state() != QLocalSocket::ConnectedState || !sendOpen(socket, QFileInfo(sourcePath).absoluteFilePath())) {
        worker.kill();
        worker.waitForFinished(1'000);
        return std::nullopt;
    }

    loupe::protocol::FrameDecoder frames;
    QElapsedTimer importTimer;
    importTimer.start();
    std::optional<QJsonObject> result;
    while (importTimer.elapsed() < kImportTimeoutMs && socket.state() == QLocalSocket::ConnectedState) {
        if (!socket.waitForReadyRead(250)) continue;
        frames.append(socket.readAll());
        while (const auto frame = frames.take()) {
            if (frame->type != loupe::protocol::FrameType::ControlJson) continue;
            const auto event = QJsonDocument::fromJson(frame->payload).object();
            if (event.value(QStringLiteral("type")).toString() == QStringLiteral("importMetrics")) {
                result = event;
                result->insert(QStringLiteral("wallElapsedMs"), importTimer.elapsed());
                break;
            }
            if (event.value(QStringLiteral("type")).toString() == QStringLiteral("failed")) break;
        }
        if (result) break;
    }
    socket.disconnectFromServer();
    worker.terminate();
    if (!worker.waitForFinished(1'000)) {
        worker.kill();
        worker.waitForFinished(1'000);
    }
    return result;
}

std::optional<QJsonObject> readBudgets(const QString& path)
{
    if (path.isEmpty()) return QJsonObject{};
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return std::nullopt;
    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) return std::nullopt;
    return document.object().value(QStringLiteral("cases")).toObject();
}

bool applyBudget(QJsonObject& result, const QString& sourcePath, const QJsonObject& budgets)
{
    if (budgets.isEmpty()) return true;
    const auto sourceName = QFileInfo(sourcePath).fileName();
    const auto budget = budgets.value(sourceName).toObject();
    if (budget.isEmpty()) {
        result.insert(QStringLiteral("budgetPassed"), false);
        result.insert(QStringLiteral("budgetError"), QStringLiteral("No performance budget for %1").arg(sourceName));
        return false;
    }
    const auto firstLimit = budget.value(QStringLiteral("maxFirstGeometryMs")).toInt(-1);
    const auto finalLimit = budget.value(QStringLiteral("maxFinalReadyMs")).toInt(-1);
    const auto firstElapsed = result.value(QStringLiteral("firstGeometryMs")).toInt(-1);
    const auto finalElapsed = result.value(QStringLiteral("finalReadyMs")).toInt(-1);
    const auto passed = firstLimit > 0 && finalLimit > 0 && firstElapsed >= 0 && finalElapsed >= 0
        && firstElapsed <= firstLimit && finalElapsed <= finalLimit;
    result.insert(QStringLiteral("budgetPassed"), passed);
    result.insert(QStringLiteral("maxFirstGeometryMs"), firstLimit);
    result.insert(QStringLiteral("maxFinalReadyMs"), finalLimit);
    return passed;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Benchmark Loupe STEP import responsiveness"));
    parser.addHelpOption();
    const QCommandLineOption budgetFileOption{
        QStringLiteral("budget-file"), QStringLiteral("Fail when a case exceeds limits from JSON."),
        QStringLiteral("path")};
    parser.addOption(budgetFileOption);
    parser.addPositionalArgument(QStringLiteral("files"), QStringLiteral("STEP files to benchmark."),
                                 QStringLiteral("[files...]"));
    parser.process(application);
    const auto paths = parser.positionalArguments();
    if (paths.isEmpty()) return 2;
    const auto budgets = readBudgets(parser.value(budgetFileOption));
    if (!budgets) return 2;
    bool budgetsPassed = true;
    for (const auto& path : paths) {
        auto result = benchmarkFile(path);
        if (!result) return 1;
        budgetsPassed = applyBudget(*result, path, *budgets) && budgetsPassed;
        const auto line = QJsonDocument(*result).toJson(QJsonDocument::Compact);
        fwrite(line.constData(), 1, static_cast<std::size_t>(line.size()), stdout);
        fputc('\n', stdout);
        fflush(stdout);
    }
    return budgetsPassed ? 0 : 3;
}
