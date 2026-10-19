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
    // A brief notification replaces the previous brief one, closes itself after a few seconds and is
    // not kept in the desktop's notification list.
    void notify(const QString &title, const QString &body, std::function<void()> onClick = {}, bool brief = false);

private slots:
    void onActionInvoked(uint id, const QString &actionKey);
    void onActivationToken(uint id, const QString &token);
    void onClosed(uint id, uint reason);

private:
    void close(uint id);
    bool serverParsesMarkup();

    QPointer<QSystemTrayIcon> m_fallback;
    int m_markup = -1; // unknown until asked
    QHash<uint, std::function<void()>> m_callbacks;
    QHash<uint, QString> m_tokens;
    uint m_briefId = 0;
};

} // namespace snapline
