#include "overlay/CaptureController.h"

#include "overlay/CaptureSession.h"
#include "overlay/OverlayWindow.h"

#include <QCursor>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>

namespace snapline {

CaptureController::CaptureController(const Capture &capture, const QRect &initialSelection, QObject *parent)
    : QObject(parent), m_session(new CaptureSession(capture, this))
{
    if (!initialSelection.isEmpty() && capture.virtualGeometry().contains(initialSelection))
        m_session->setSelection(initialSelection);
}

void CaptureController::show()
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    const auto &shots = m_session->capture().screens;
    OverlayWindow *underCursor = nullptr;

    for (int i = 0; i < shots.size(); ++i) {
        QScreen *screen = nullptr;
        for (QScreen *s : screens) {
            if (s->name() == shots[i].name) {
                screen = s;
                break;
            }
        }
        if (!screen)
            continue;
        auto *window = new OverlayWindow(m_session, i, screen);
        connect(window, &OverlayWindow::action, this, &CaptureController::onAction);
        connect(window, &OverlayWindow::textEditingStarted, this, [this](OverlayWindow *editing) {
            for (const auto &w : m_windows) {
                if (w && w != editing)
                    w->commitText();
            }
        });
        m_windows << window;
        window->showFullScreen();
        if (shots[i].geometry.contains(QCursor::pos()))
            underCursor = window;
    }

    if (m_windows.isEmpty()) {
        onAction(Action::Close);
        return;
    }
    OverlayWindow *focus = underCursor ? underCursor : m_windows.first().data();
    focus->raise();
    focus->activateWindow();
    focus->setFocus();
}

void CaptureController::onAction(Action action)
{
    if (m_done)
        return;

    switch (action) {
    case Action::Undo:
        m_session->undo();
        return;
    case Action::Redo:
        m_session->redo();
        return;
    case Action::SelectFullScreen: {
        QRect target = m_session->virtualGeometry();
        for (const ScreenImage &s : m_session->capture().screens) {
            if (s.geometry.contains(QCursor::pos()))
                target = s.geometry;
        }
        m_session->setSelection(target);
        return;
    }
    default:
        break;
    }

    for (const auto &w : m_windows) {
        if (w)
            w->commitText();
    }

    const QRect selection = m_session->selection();
    if (action != Action::Close && selection.isEmpty())
        return; // nothing selected yet; ignore like the reference does

    m_done = true;
    const QImage image = action == Action::Close ? QImage() : m_session->result();

    if (action == Action::Copy) {
        emit finished(action, image, selection);
        closeWindows();
    } else {
        closeWindows();
        // Let the overlays disappear before dialogs (save, print, upload) show up.
        QTimer::singleShot(0, this, [this, action, image, selection]() { emit finished(action, image, selection); });
    }
}

void CaptureController::closeWindows()
{
    for (const auto &w : m_windows) {
        if (w)
            w->close();
    }
    m_windows.clear();
}

} // namespace snapline
