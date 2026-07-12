#include "worker/WorkerServer.h"

#include <QCoreApplication>
#include <QUuid>

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QString serverName;
    const auto arguments = application.arguments();
    const auto option = arguments.indexOf(QStringLiteral("--server-name"));
    if (option >= 0 && option + 1 < arguments.size()) {
        serverName = arguments.at(option + 1);
    }
    if (serverName.isEmpty()) {
        serverName = QStringLiteral("loupe-%1-%2").arg(qEnvironmentVariable("USERNAME"), QUuid::createUuid().toString(QUuid::WithoutBraces));
    }

    loupe::worker::WorkerServer server;
    if (!server.listen(serverName)) {
        return 2;
    }
    return application.exec();
}
