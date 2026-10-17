#pragma once

#include "capture/Capture.h"
#include "overlay/Actions.h"

#include <QImage>
#include <QList>
#include <QObject>
#include <QPointer>

namespace snapline {

class CaptureSession;
class OverlayWindow;

// Runs one interactive capture: shows an overlay per monitor and reports the user's final action.
class CaptureController : public QObject {
    Q_OBJECT

public:
    CaptureController(const Capture &capture, const QRect &initialSelection, QObject *parent);
    void show();

signals:
    // Emitted once. Close means cancelled (image is null). For Copy it fires while the overlay is
    // still visible, so the clipboard is set from a focused surface (required on Wayland).
    void finished(snapline::Action action, const QImage &image, const QRect &selection);

private:
    void onAction(Action action);
    void closeWindows();

    CaptureSession *m_session;
    QList<QPointer<OverlayWindow>> m_windows;
    bool m_done = false;
};

} // namespace snapline
