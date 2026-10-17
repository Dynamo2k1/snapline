#include "app/SingleInstance.h"

#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>

namespace snapline {

SingleInstance::SingleInstance(QObject *parent) : QObject(parent)
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (dir.isEmpty())
        dir = QDir::tempPath();
    m_name = QDir(dir).filePath(QStringLiteral("snapline.sock"));
}

bool SingleInstance::sendToRunning(const QString &command)
{
    QLocalSocket socket;
    socket.connectToServer(m_name);
    if (!socket.waitForConnected(500))
        return false;
    socket.write(command.toUtf8() + '\n');
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
                while (socket->canReadLine()) {
                    const QString command = QString::fromUtf8(socket->readLine()).trimmed();
                    if (!command.isEmpty())
                        emit commandReceived(command);
                }
            });
        }
    });
    return true;
}

} // namespace snapline
