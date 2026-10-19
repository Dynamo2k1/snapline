#include "app/Notifier.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
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
    bus.connect(kService, kPath, kService, QStringLiteral("ActivationToken"), this,
                SLOT(onActivationToken(uint, QString)));
}

void Notifier::notify(const QString &title, const QString &body, std::function<void()> onClick)
{
    QDBusMessage message = QDBusMessage::createMethodCall(kService, kPath, kService, QStringLiteral("Notify"));
    QStringList actions;
    if (onClick)
        actions << QStringLiteral("default") << tr("Open");
    const QVariantMap hints{{QStringLiteral("desktop-entry"), QGuiApplication::desktopFileName()}};
    // Bodies hold paths, links and server messages; "&" or "<" in them must not be read as markup.
    const QString shownBody = serverParsesMarkup() ? body.toHtmlEscaped() : body;
    message << QGuiApplication::applicationDisplayName() << uint(0) << QGuiApplication::desktopFileName() << title
            << shownBody << actions << hints << int(5000);

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

bool Notifier::serverParsesMarkup()
{
    if (m_markup < 0) {
        const QDBusMessage call =
            QDBusMessage::createMethodCall(kService, kPath, kService, QStringLiteral("GetCapabilities"));
        const QDBusReply<QStringList> reply = QDBusConnection::sessionBus().call(call, QDBus::Block, 1000);
        m_markup = reply.isValid() && reply.value().contains(QStringLiteral("body-markup")) ? 1 : 0;
    }
    return m_markup == 1;
}

void Notifier::onActionInvoked(uint id, const QString &actionKey)
{
    Q_UNUSED(actionKey)
    const auto callback = m_callbacks.take(id);
    // Sent just before the click; lets the callback's window take focus on Wayland.
    const QString token = m_tokens.take(id);
    if (callback && !token.isEmpty())
        qputenv("XDG_ACTIVATION_TOKEN", token.toUtf8());
    if (callback)
        callback();
}

void Notifier::onActivationToken(uint id, const QString &token)
{
    if (m_callbacks.contains(id))
        m_tokens.insert(id, token);
}

void Notifier::onClosed(uint id, uint reason)
{
    Q_UNUSED(reason)
    // Some servers send NotificationClosed before ActionInvoked; give the action a moment to arrive.
    QMetaObject::invokeMethod(this, [this, id]() {
        m_callbacks.remove(id);
        m_tokens.remove(id);
    }, Qt::QueuedConnection);
}

} // namespace snapline
