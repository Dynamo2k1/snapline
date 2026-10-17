#pragma once

#include "capture/Capture.h"
#include "tools/AnnotationStack.h"

#include <QObject>

namespace snapline {

// State shared by all overlay windows of one capture: frozen screens, selection, tool and annotations.
// All coordinates are logical virtual-desktop coordinates.
class CaptureSession : public QObject {
    Q_OBJECT

public:
    CaptureSession(Capture capture, QObject *parent);

    const Capture &capture() const { return m_capture; }
    QRect virtualGeometry() const { return m_virtual; }

    QRect selection() const { return m_selection; }
    bool hasSelection() const { return !m_selection.isEmpty(); }
    void setSelection(const QRect &rect);

    Tool tool() const { return m_tool; }
    void setTool(Tool tool);
    QColor color() const { return m_style.color; }
    void setColor(const QColor &color);
    qreal strokeWidth() const { return m_style.width; }
    void setStrokeWidth(qreal width);
    Style style() const { return m_style; }

    const AnnotationStack &annotations() const { return m_annotations; }
    const AnnotationPtr &draft() const { return m_draft; }
    void beginDraft(const QPointF &start);
    void updateDraft(const QPointF &point, bool constrain);
    void commitDraft();
    void addAnnotation(AnnotationPtr annotation);
    void undo();
    void redo();
    bool canUndo() const { return m_annotations.canUndo(); }
    bool canRedo() const { return m_annotations.canRedo(); }

    qreal resultDpr() const;
    QSize resultPixelSize() const;
    QImage result() const;

signals:
    void changed();

private:
    Capture m_capture;
    QRect m_virtual;
    QRect m_selection;
    Tool m_tool = Tool::None;
    Style m_style;
    AnnotationStack m_annotations;
    AnnotationPtr m_draft;
};

} // namespace snapline
