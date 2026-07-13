#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>

#include <cstdint>

class QLocalSocket;

namespace loupe::app::worker {

class WorkerClient final : public QObject {
    Q_OBJECT

public:
    explicit WorkerClient(QObject* parent = nullptr);

    [[nodiscard]] bool connectToServer(const QString& serverName, int timeoutMs = 3'000);
    [[nodiscard]] std::uint64_t openFile(const QString& path);
    void cancel(std::uint64_t requestId);

signals:
    void snapshotReady(quint64 requestId, const QByteArray& snapshotJson);
    void requestFailed(quint64 requestId, const QString& code, const QString& message, bool recoverable);
    void requestCanceled(quint64 requestId);
    void protocolError(const QString& message);
    void connectionLost();

private slots:
    void readEvents();

private:
    void writeCommand(const QByteArray& command);

    QLocalSocket* socket_{};
    QByteArray inputBuffer_;
    std::uint64_t nextRequestId_{1};
};

} // namespace loupe::app::worker
