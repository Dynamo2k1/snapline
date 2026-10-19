#include "app/SingleInstance.h"

#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>
#include <unistd.h>

namespace snapline {

SingleInstance::SingleInstance(QObject *parent) : QObject(parent)
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    // Without a private runtime dir, use a per-user name in /tmp so users cannot collide.
    const QString name = dir.isEmpty() ? QStringLiteral("snapline-%1.sock").arg(getuid()) : QStringLiteral("snapline.sock");
    if (dir.isEmpty())
        dir = QDir::tempPath();
    m_name = QDir(dir).filePath(name);
}

bool SingleInstance::sendToRunning(const QString &command, const QString &activationToken)
{
    QLocalSocket socket;
    socket.connectToServer(m_name);
    if (!socket.waitForConnected(500))
        return false;
    // "command<TAB>token": the token lets the running instance raise a window the user asked for.
    QByteArray line = command.toUtf8();
    const QString token = activationToken.simplified();
    if (!token.isEmpty() && !token.contains(QLatin1Char('\t')))
        line += '\t' + token.toUtf8();
    socket.write(line + '\n');
    socket.waitForBytesWritten(1000);
    socket.disconnectFromServer();
    return true;
}

bool SingleInstance::listen()
{
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server->listen(m_name)) {
        // A stale socket from a crashed instance; nobody answered in sendToRunning().
        QLocalServer::removeServer(m_name);
        if (!m_server->listen(m_name))
            return false;
    }
    connect(m_server, &QLocalServer::newConnection, this, [this]() {
        while (QLocalSocket *socket = m_server->nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
                // Commands are one short line; drop clients that send anything else.
                if (!socket->canReadLine() && socket->bytesAvailable() > 1024) {
                    socket->abort();
                    return;
                }
                while (socket->canReadLine()) {
                    const QString line = QString::fromUtf8(socket->readLine()).trimmed();
                    const QString command = line.section(QLatin1Char('\t'), 0, 0);
                    if (!command.isEmpty())
                        emit commandReceived(command, line.section(QLatin1Char('\t'), 1, 1));
                }
            });
        }
    });
    return true;
}

} // namespace snapline
