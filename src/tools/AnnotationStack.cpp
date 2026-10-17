#include "tools/AnnotationStack.h"

namespace snapline {

void AnnotationStack::push(AnnotationPtr annotation)
{
    if (!annotation || annotation->isEmpty())
        return;
    checkpoint(m_items);
    m_items.push_back(std::move(annotation));
}

bool AnnotationStack::undo()
{
    if (m_undo.isEmpty())
        return false;
    m_redo.push_back(m_items);
    m_items = m_undo.takeLast();
    return true;
}

bool AnnotationStack::redo()
{
    if (m_redo.isEmpty())
        return false;
    m_undo.push_back(m_items);
    m_items = m_redo.takeLast();
    return true;
}

void AnnotationStack::clear()
{
    m_items.clear();
    m_undo.clear();
    m_redo.clear();
}

void AnnotationStack::replaceAt(int index, AnnotationPtr annotation)
{
    if (index >= 0 && index < m_items.size() && annotation)
        m_items[index] = std::move(annotation);
}

void AnnotationStack::removeAt(int index)
{
    if (index >= 0 && index < m_items.size())
        m_items.removeAt(index);
}

void AnnotationStack::checkpoint(const QVector<AnnotationPtr> &state)
{
    m_undo.push_back(state);
    m_redo.clear();
}

void AnnotationStack::paint(QPainter &painter, const AnnotationPtr &skip) const
{
    for (const AnnotationPtr &item : m_items) {
        if (item != skip)
            item->paint(painter);
    }
}

} // namespace snapline
