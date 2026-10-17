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

    // Editing existing annotations. Colour and width changes also apply to the selected one.
    AnnotationPtr annotationAt(const QPointF &point) const;
    AnnotationPtr selectedAnnotation() const { return m_selected; }
    void selectAnnotation(const AnnotationPtr &annotation);
    void beginMove();
    void moveSelected(const QPointF &offsetFromStart);
    void endMove();
    void nudgeSelected(const QPointF &offset);
    void deleteSelected();
    // Replaces old with replacement (removes it if replacement is null or empty), as one undo step.
    void replaceAnnotation(const AnnotationPtr &old, AnnotationPtr replacement);
    // Hidden while its text is being edited in place.
    AnnotationPtr editingAnnotation() const { return m_editing; }
    void setEditingAnnotation(const AnnotationPtr &annotation);

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
    void restyleSelected();
    void rebakeBlurs();

    Capture m_capture;
    QRect m_virtual;
    QRect m_selection;
    Tool m_tool = Tool::None;
    Style m_style;
    AnnotationStack m_annotations;
    AnnotationPtr m_draft;
    AnnotationPtr m_selected;
    AnnotationPtr m_editing;
    AnnotationPtr m_moveOriginal;
    QVector<AnnotationPtr> m_moveBase;
    bool m_moved = false;
};

} // namespace snapline
