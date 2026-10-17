#pragma once

#include <QObject>

class QLocalServer;

namespace snapline {

// Forwards command-line commands to the already running instance over a local socket.
class SingleInstance : public QObject {
    Q_OBJECT

public:
    explicit SingleInstance(QObject *parent = nullptr);
    bool sendToRunning(const QString &command);
    bool listen();

signals:
    void commandReceived(const QString &command);

private:
    QString m_name;
    QLocalServer *m_server = nullptr;
};

} // namespace snapline
