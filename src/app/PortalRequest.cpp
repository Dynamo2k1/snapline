#include "app/PortalRequest.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QRandomGenerator>

namespace snapline {

namespace {
const QString kService = QStringLiteral("org.freedesktop.portal.Desktop");
const QString kPath = QStringLiteral("/org/freedesktop/portal/desktop");
const QString kRequestInterface = QStringLiteral("org.freedesktop.portal.Request");
} // namespace

PortalRequest::PortalRequest(QObject *parent) : QObject(parent) {}

PortalRequest *PortalRequest::call(const QString &interface, const QString &method, QVariantList args, QObject *parent)
{
    auto *request = new PortalRequest(parent);
    QDBusConnection bus = QDBusConnection::sessionBus();

    const QString token = QStringLiteral("snapline%1").arg(QRandomGenerator::global()->generate());
    QString sender = bus.baseService();
    sender.remove(0, 1); // leading ':'
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));

    // Subscribe before calling so a fast Response cannot be missed.
    request->subscribe(QStringLiteral("%1/request/%2/%3").arg(kPath, sender, token));

    QVariantMap options = args.isEmpty() ? QVariantMap() : args.takeLast().toMap();
    options.insert(QStringLiteral("handle_token"), token);
    args << options;

    QDBusMessage message = QDBusMessage::createMethodCall(kService, kPath, interface, method);
    message.setArguments(args);
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(message), request);
    connect(watcher, &QDBusPendingCallWatcher::finished, request, [request](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<QDBusObjectPath> reply = *w;
        if (reply.isError()) {
            request->m_done = true;
            request->unsubscribe();
            emit request->error(reply.error().message());
            return;
        }
        const QString actual = reply.value().path();
        if (!request->m_done && actual != request->m_path) {
            request->unsubscribe();
            request->subscribe(actual);
        }
    });
    return request;
}

void PortalRequest::subscribe(const QString &path)
{
    m_path = path;
    QDBusConnection::sessionBus().connect(kService, m_path, kRequestInterface, QStringLiteral("Response"), this,
                                          SLOT(onResponse(uint, QVariantMap)));
}

void PortalRequest::unsubscribe()
{
    QDBusConnection::sessionBus().disconnect(kService, m_path, kRequestInterface, QStringLiteral("Response"), this,
                                             SLOT(onResponse(uint, QVariantMap)));
}

void PortalRequest::onResponse(uint response, const QVariantMap &results)
{
    if (m_done)
        return;
    m_done = true;
    unsubscribe();
    emit finished(response, results);
}

} // namespace snapline
