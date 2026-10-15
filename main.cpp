#include "mainwindow.h"

#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>

#if defined(Q_OS_UNIX)
#include <unistd.h>
#endif

#ifndef APP_VERSION
#define APP_VERSION "unknown"
#endif

namespace {

// Per-user server name so different users on the same machine can each run
// their own instance without stealing each other's single-instance socket.
QString instanceServerName()
{
    QString name = QStringLiteral("ipchecker-instance");
#if defined(Q_OS_UNIX)
    name += QLatin1Char('-') + QString::number(getuid());
#endif
    return name;
}

bool notifyRunningInstance()
{
    QLocalSocket socket;
    socket.connectToServer(instanceServerName());
    if (!socket.waitForConnected(300)) {
        return false;
    }
    socket.write("show");
    socket.waitForBytesWritten(300);
    socket.disconnectFromServer();
    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("IP Inspector Pro"));
    app.setApplicationDisplayName(QStringLiteral("IP Inspector Pro"));
    app.setApplicationVersion(QStringLiteral(APP_VERSION));
    app.setOrganizationName(QStringLiteral("IPInspector"));

    // Another instance is already running: ask it to show its window.
    if (notifyRunningInstance()) {
        return 0;
    }

    MainWindow window;

    QLocalServer server;
    QLocalServer::removeServer(instanceServerName());
    if (server.listen(instanceServerName())) {
        QObject::connect(&server, &QLocalServer::newConnection, &window, [&server, &window]() {
            while (QLocalSocket *connection = server.nextPendingConnection()) {
                window.showWindow();
                connection->disconnectFromServer();
                connection->deleteLater();
            }
        });
    }

    window.show();
    return app.exec();
}
