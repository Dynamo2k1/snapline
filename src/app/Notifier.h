#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <functional>

class QSystemTrayIcon;

namespace snapline {

// Desktop notifications via org.freedesktop.Notifications; click runs the optional callback.
class Notifier : public QObject {
    Q_OBJECT

public:
    explicit Notifier(QSystemTrayIcon *fallback, QObject *parent = nullptr);
    void notify(const QString &title, const QString &body, std::function<void()> onClick = {});

private slots:
    void onActionInvoked(uint id, const QString &actionKey);
    void onClosed(uint id, uint reason);

private:
    bool serverParsesMarkup();

    QPointer<QSystemTrayIcon> m_fallback;
    int m_markup = -1; // unknown until asked
    QHash<uint, std::function<void()>> m_callbacks;
};

} // namespace snapline
