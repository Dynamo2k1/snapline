#pragma once

#include "overlay/Actions.h"

#include <QPointer>
#include <QWidget>

class QScreen;

namespace snapline {

class ActionsBar;
class CaptureSession;
class TextEditor;
class ToolsBar;

// Full-screen window on one monitor showing that monitor's part of the frozen capture.
class OverlayWindow : public QWidget {
    Q_OBJECT

public:
    OverlayWindow(CaptureSession *session, int screenIndex, QScreen *screen);
    ~OverlayWindow() override;

    QRect screenGeometry() const;
    void commitText();

signals:
    void action(snapline::Action action);
    void textEditingStarted(snapline::OverlayWindow *window);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void enterEvent(QEnterEvent *event) override;

private:
    enum class Drag { None, NewSelection, Move, Resize, Draw };

    QPoint toGlobal(const QPointF &local) const;
    int handleAt(const QPoint &global) const;
    void updateCursor(const QPoint &global);
    void updateToolbars();
    void beginText(const QPoint &global);
    void nudgeSelection(int dx, int dy);

    CaptureSession *m_session;
    int m_index;
    QPointer<QScreen> m_screen;
    ToolsBar *m_tools;
    ActionsBar *m_actions;
    QPointer<TextEditor> m_text;
    QPoint m_textGlobal;

    Drag m_drag = Drag::None;
    int m_handle = -1;
    QPoint m_pressGlobal;
    QRect m_startSelection;
    QPoint m_cursorGlobal;
};

} // namespace snapline
