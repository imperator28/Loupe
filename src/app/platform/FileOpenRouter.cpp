#include "app/platform/FileOpenRouter.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QFileOpenEvent>
#include <QLocalServer>
#include <QLocalSocket>
#include <QWindow>

#include <utility>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace loupe::app::platform {
namespace {

constexpr int kForwardTimeoutMs = 2'000;

bool isStepFile(const QString& path)
{
    const auto suffix = QFileInfo(path).suffix().toLower();
    return suffix == QStringLiteral("step") || suffix == QStringLiteral("stp");
}

} // namespace

FileOpenRouter::FileOpenRouter(QObject* parent)
    : QObject(parent)
{
    QCoreApplication::instance()->installEventFilter(this);
}

FileOpenRouter::~FileOpenRouter() = default;

QStringList FileOpenRouter::stepFilesIn(const QStringList& arguments)
{
    QStringList files;
    for (qsizetype index = 1; index < arguments.size(); ++index) {
        const auto& argument = arguments.at(index);
        if (argument.startsWith(QLatin1Char('-'))) continue;
        if (isStepFile(argument)) files.append(QFileInfo(argument).absoluteFilePath());
    }
    return files;
}

// One server per user, so two people logged in to one machine never share a window.
QString FileOpenRouter::serverName()
{
    const auto user = QCryptographicHash::hash(QDir::homePath().toUtf8(), QCryptographicHash::Sha1).toHex().left(12);
    return QStringLiteral("loupe-open-%1").arg(QString::fromLatin1(user));
}

bool FileOpenRouter::forwardToRunningInstance(const QStringList& files)
{
    if (files.isEmpty()) return false;
    QLocalSocket socket;
    socket.connectToServer(serverName());
    if (!socket.waitForConnected(kForwardTimeoutMs)) return false;
#ifdef Q_OS_WIN
    // Windows only lets a process that already has the foreground give it away: without
    // this the running copy opens the file but cannot come to the front.
    AllowSetForegroundWindow(ASFW_ANY);
#endif
    socket.write(files.join(QLatin1Char('\n')).toUtf8() + '\n');
    // Not checked: once flush() has written everything there is nothing left to wait for,
    // and waitForBytesWritten reports that as false. The reply below is the real check.
    socket.flush();
    socket.waitForBytesWritten(kForwardTimeoutMs);
    // Wait for the reply so the files are known to have arrived before this copy exits.
    return socket.waitForReadyRead(kForwardTimeoutMs) && socket.readAll().startsWith("ok");
}

void FileOpenRouter::listenForOtherInstances()
{
    if (server_) return;
    // Ask first, never listen to find out: with access options set, Qt on Unix creates the
    // socket elsewhere and renames it into place, silently replacing a running copy's, and
    // on Windows two servers can share a pipe name. A copy that answers owns the name; a
    // name nobody answers on was left by a copy that crashed and is safe to clear.
    {
        QLocalSocket probe;
        probe.connectToServer(serverName());
        if (probe.waitForConnected(500)) return;
    }
    QLocalServer::removeServer(serverName());
    server_ = new QLocalServer(this);
    server_->setSocketOptions(QLocalServer::UserAccessOption);
    if (!server_->listen(serverName())) return;
    connect(server_, &QLocalServer::newConnection, this, [this] {
        while (auto* socket = server_->nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            const auto receive = [this, socket] {
                if (!socket->canReadLine()) return;
                QStringList files;
                while (socket->canReadLine()) {
                    const auto line = QString::fromUtf8(socket->readLine()).trimmed();
                    if (!line.isEmpty()) files.append(line);
                }
                socket->write("ok\n");
                socket->flush();
                open(files);
            };
            connect(socket, &QLocalSocket::readyRead, this, receive);
            // The files may already have arrived before the connection was handed over.
            receive();
        }
    });
}

void FileOpenRouter::setWindow(QObject* window)
{
    window_ = window;
    const auto pending = std::exchange(pending_, {});
    for (const auto& file : pending) deliver(file);
}

void FileOpenRouter::setWindowProvider(std::function<QObject*()> provider)
{
    windowProvider_ = std::move(provider);
}

void FileOpenRouter::open(const QStringList& files)
{
    for (const auto& file : files)
        if (isStepFile(file)) deliver(QUrl::fromLocalFile(file));
}

bool FileOpenRouter::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::FileOpen) {
        const auto* fileOpen = static_cast<QFileOpenEvent*>(event);
        const auto url = fileOpen->url().isValid() ? fileOpen->url() : QUrl::fromLocalFile(fileOpen->file());
        if (isStepFile(url.toLocalFile())) {
            deliver(url);
            return true;
        }
    }
    return QObject::eventFilter(watched, event);
}

void FileOpenRouter::deliver(const QUrl& file)
{
    QObject* target = windowProvider_ ? windowProvider_() : nullptr;
    if (!target) target = window_;
    if (!target) {
        pending_.append(file);
        return;
    }
    QMetaObject::invokeMethod(target, "openStepFile", Q_ARG(QVariant, QVariant::fromValue(file)));
    if (auto* window = qobject_cast<QWindow*>(target)) {
        if (window->windowState() & Qt::WindowMinimized) window->showNormal();
        window->raise();
        window->requestActivate();
    }
}

} // namespace loupe::app::platform
