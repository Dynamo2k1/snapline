#include "tools/AnnotationStack.h"

namespace snapline {

void AnnotationStack::push(AnnotationPtr annotation)
{
    if (!annotation || annotation->isEmpty())
        return;
    m_items.push_back(std::move(annotation));
    m_redo.clear();
}

bool AnnotationStack::undo()
{
    if (m_items.isEmpty())
        return false;
    m_redo.push_back(m_items.takeLast());
    return true;
}

bool AnnotationStack::redo()
{
    if (m_redo.isEmpty())
        return false;
    m_items.push_back(m_redo.takeLast());
    return true;
}

void AnnotationStack::clear()
{
    m_items.clear();
    m_redo.clear();
}

void AnnotationStack::paint(QPainter &painter) const
{
    for (const AnnotationPtr &item : m_items)
        item->paint(painter);
}

} // namespace snapline
