#pragma once

#include "capture/Capture.h"

#include <QObject>

namespace snapline {

// Grabs every screen. Emits exactly one of finished() / failed() per grab() call, never synchronously.
// failed() with an empty message means the user cancelled (e.g. denied the portal dialog).
class ScreenGrabber : public QObject {
    Q_OBJECT

public:
    // backend: "auto" | "qt" | "portal" | "grim"
    static ScreenGrabber *create(const QString &backend, QObject *parent);
    static bool isWayland();

    virtual QString name() const = 0;
    virtual void grab(bool includeCursor) = 0;

signals:
    void finished(const snapline::Capture &capture);
    void failed(const QString &message);

protected:
    using QObject::QObject;
    static QVector<QPair<QString, QRect>> screenLayout();
};

} // namespace snapline
