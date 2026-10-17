#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace snapline {

// One xdg-desktop-portal method call that answers through an org.freedesktop.portal.Request object.
// The last argument must be the options map; a handle_token is injected into it.
class PortalRequest : public QObject {
    Q_OBJECT

public:
    static PortalRequest *call(const QString &interface, const QString &method, QVariantList args, QObject *parent);

signals:
    // response: 0 = success, 1 = cancelled by the user, 2 = other error.
    void finished(uint response, const QVariantMap &results);
    void error(const QString &message);

private slots:
    void onResponse(uint response, const QVariantMap &results);

private:
    explicit PortalRequest(QObject *parent);
    void subscribe(const QString &path);
    void unsubscribe();

    QString m_path;
    bool m_done = false;
};

} // namespace snapline
