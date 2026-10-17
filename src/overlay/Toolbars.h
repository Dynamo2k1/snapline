#pragma once

#include "overlay/Actions.h"
#include "tools/Annotation.h"

#include <QFrame>
#include <utility>
#include <vector>

class QToolButton;

namespace snapline {

class CaptureSession;

class ToolsBar : public QFrame {
    Q_OBJECT

public:
    ToolsBar(CaptureSession *session, QWidget *parent);
    void sync();

signals:
    void action(snapline::Action action);

private:
    void pickColor();

    CaptureSession *m_session;
    std::vector<std::pair<Tool, QToolButton *>> m_tools;
    QToolButton *m_color = nullptr;
    QToolButton *m_undo = nullptr;
    QToolButton *m_redo = nullptr;
};

class ActionsBar : public QFrame {
    Q_OBJECT

public:
    explicit ActionsBar(QWidget *parent);

signals:
    void action(snapline::Action action);
};

} // namespace snapline
