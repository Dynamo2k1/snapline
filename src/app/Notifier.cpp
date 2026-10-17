#include "app/Notifier.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QGuiApplication>
#include <QSystemTrayIcon>

namespace snapline {

namespace {
const QString kService = QStringLiteral("org.freedesktop.Notifications");
const QString kPath = QStringLiteral("/org/freedesktop/Notifications");
} // namespace

Notifier::Notifier(QSystemTrayIcon *fallback, QObject *parent) : QObject(parent), m_fallback(fallback)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(kService, kPath, kService, QStringLiteral("ActionInvoked"), this,
                SLOT(onActionInvoked(uint, QString)));
    bus.connect(kService, kPath, kService, QStringLiteral("NotificationClosed"), this, SLOT(onClosed(uint, uint)));
}

void Notifier::notify(const QString &title, const QString &body, std::function<void()> onClick)
{
    QDBusMessage message = QDBusMessage::createMethodCall(kService, kPath, kService, QStringLiteral("Notify"));
    QStringList actions;
    if (onClick)
        actions << QStringLiteral("default") << tr("Open");
    const QVariantMap hints{{QStringLiteral("desktop-entry"), QGuiApplication::desktopFileName()}};
    message << QGuiApplication::applicationDisplayName() << uint(0) << QGuiApplication::desktopFileName() << title
            << body << actions << hints << int(5000);

    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, title, body, onClick](QDBusPendingCallWatcher *) {
                watcher->deleteLater();
                const QDBusPendingReply<uint> reply = *watcher;
                if (reply.isError()) {
                    if (m_fallback)
                        m_fallback->showMessage(title, body);
                    return;
                }
                if (onClick)
                    m_callbacks.insert(reply.value(), onClick);
            });
}

void Notifier::onActionInvoked(uint id, const QString &actionKey)
{
    Q_UNUSED(actionKey)
    const auto callback = m_callbacks.take(id);
    if (callback)
        callback();
}

void Notifier::onClosed(uint id, uint reason)
{
    Q_UNUSED(reason)
    // Some servers send NotificationClosed before ActionInvoked; give the action a moment to arrive.
    QMetaObject::invokeMethod(this, [this, id]() { m_callbacks.remove(id); }, Qt::QueuedConnection);
}

} // namespace snapline
