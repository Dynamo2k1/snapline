#pragma once

#include "capture/Capture.h"

#include <QObject>

namespace snapline {

// Grabs every screen. Emits exactly one of finished() / failed() / permissionDenied() per grab() call,
// never synchronously.
// failed() with an empty message means the user cancelled (e.g. denied the portal dialog).
class ScreenGrabber : public QObject {
    Q_OBJECT

public:
    // backend: "auto" | "qt" | "portal" | "grim"
    static ScreenGrabber *create(const QString &backend, QObject *parent);
    static bool isWayland();

    virtual QString name() const = 0;
    virtual void grab(bool includeCursor) = 0;
    // Set while the desktop may show its own permission dialog, so a slow answer is not a hang.
    void setUserMayBeAsked(bool asked) { m_userMayBeAsked = asked; }

signals:
    void finished(const snapline::Capture &capture);
    void failed(const QString &message);
    // The desktop refused because Snapline has no screenshot permission yet. GNOME can only ask the user
    // while a Snapline window is focused, so the app has to show one and retry from there.
    void permissionDenied();

protected:
    using QObject::QObject;
    static QVector<QPair<QString, QRect>> screenLayout();

    bool m_userMayBeAsked = false;
};

} // namespace snapline
