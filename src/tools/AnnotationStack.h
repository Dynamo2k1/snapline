#pragma once

#include "tools/Annotation.h"

#include <QVector>

namespace snapline {

class AnnotationStack {
public:
    void push(AnnotationPtr annotation);
    bool undo();
    bool redo();
    bool canUndo() const { return !m_items.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    void clear();
    int size() const { return m_items.size(); }
    void paint(QPainter &painter) const;

private:
    QVector<AnnotationPtr> m_items;
    QVector<AnnotationPtr> m_redo;
};

} // namespace snapline
