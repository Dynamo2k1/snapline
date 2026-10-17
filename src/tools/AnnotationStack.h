#pragma once

#include "tools/Annotation.h"

#include <QVector>

namespace snapline {

// The annotations of one capture, bottom to top, with snapshot-based undo/redo.
class AnnotationStack {
public:
    void push(AnnotationPtr annotation);
    bool undo();
    bool redo();
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    void clear();
    int size() const { return m_items.size(); }
    const QVector<AnnotationPtr> &items() const { return m_items; }
    int indexOf(const AnnotationPtr &annotation) const { return m_items.indexOf(annotation); }

    // Edits that are not recorded on their own; pair them with checkpoint() taken beforehand.
    void replaceAt(int index, AnnotationPtr annotation);
    void removeAt(int index);
    // Records state as the version to return to on undo.
    void checkpoint(const QVector<AnnotationPtr> &state);

    // Paints everything except skip (the item being edited, if any).
    void paint(QPainter &painter, const AnnotationPtr &skip = nullptr) const;

private:
    QVector<AnnotationPtr> m_items;
    QVector<QVector<AnnotationPtr>> m_undo;
    QVector<QVector<AnnotationPtr>> m_redo;
};

} // namespace snapline
