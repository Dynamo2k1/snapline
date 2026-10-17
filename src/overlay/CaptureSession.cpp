#include "overlay/CaptureSession.h"

#include <QPainter>
#include <cmath>

namespace snapline {

CaptureSession::CaptureSession(Capture capture, QObject *parent)
    : QObject(parent), m_capture(std::move(capture)), m_virtual(m_capture.virtualGeometry())
{
}

void CaptureSession::setSelection(const QRect &rect)
{
    const QRect clamped = rect.normalized().intersected(m_virtual);
    if (clamped == m_selection)
        return;
    m_selection = clamped;
    emit changed();
}

void CaptureSession::setTool(Tool tool)
{
    if (m_tool == tool)
        return;
    m_tool = tool;
    emit changed();
}

void CaptureSession::setColor(const QColor &color)
{
    m_style.color = color;
    restyleSelected();
    emit changed();
}

void CaptureSession::setStrokeWidth(qreal width)
{
    m_style.width = qBound<qreal>(1, width, 30);
    restyleSelected();
    emit changed();
}

void CaptureSession::beginDraft(const QPointF &start)
{
    m_draft = beginAnnotation(m_tool, start, m_style);
    emit changed();
}

void CaptureSession::updateDraft(const QPointF &point, bool constrain)
{
    if (!m_draft)
        return;
    m_draft->update(point, constrain);
    emit changed();
}

void CaptureSession::commitDraft()
{
    if (!m_draft)
        return;
    AnnotationPtr draft = std::move(m_draft);
    m_draft.reset();
    addAnnotation(std::move(draft));
}

void CaptureSession::addAnnotation(AnnotationPtr annotation)
{
    if (annotation && !annotation->isEmpty()) {
        m_annotations.push(std::move(annotation));
        rebakeBlurs();
    }
    emit changed();
}

AnnotationPtr CaptureSession::annotationAt(const QPointF &point) const
{
    const auto &items = m_annotations.items();
    for (auto it = items.crbegin(); it != items.crend(); ++it) {
        if (*it != m_editing && (*it)->hitTest(point, 4))
            return *it;
    }
    return nullptr;
}

void CaptureSession::selectAnnotation(const AnnotationPtr &annotation)
{
    if (m_selected == annotation)
        return;
    m_selected = annotation;
    emit changed();
}

void CaptureSession::beginMove()
{
    m_moveBase = m_annotations.items();
    m_moveOriginal = m_selected;
    m_moved = false;
}

void CaptureSession::moveSelected(const QPointF &offsetFromStart)
{
    const int index = m_annotations.indexOf(m_selected);
    if (!m_moveOriginal || index < 0)
        return;
    m_selected = m_moveOriginal->translated(offsetFromStart);
    m_annotations.replaceAt(index, m_selected);
    m_moved = true;
    emit changed();
}

void CaptureSession::endMove()
{
    if (m_moved) {
        m_annotations.checkpoint(m_moveBase);
        rebakeBlurs();
        emit changed();
    }
    m_moveBase.clear();
    m_moveOriginal.reset();
    m_moved = false;
}

void CaptureSession::nudgeSelected(const QPointF &offset)
{
    if (m_selected)
        replaceAnnotation(m_selected, m_selected->translated(offset));
}

void CaptureSession::deleteSelected()
{
    if (m_selected)
        replaceAnnotation(m_selected, nullptr);
}

void CaptureSession::replaceAnnotation(const AnnotationPtr &old, AnnotationPtr replacement)
{
    const int index = m_annotations.indexOf(old);
    if (index < 0)
        return;
    m_annotations.checkpoint(m_annotations.items());
    if (replacement && !replacement->isEmpty()) {
        if (m_selected == old)
            m_selected = replacement;
        m_annotations.replaceAt(index, std::move(replacement));
    } else {
        if (m_selected == old)
            m_selected.reset();
        m_annotations.removeAt(index);
    }
    rebakeBlurs();
    emit changed();
}

void CaptureSession::setEditingAnnotation(const AnnotationPtr &annotation)
{
    m_editing = annotation;
    emit changed();
}

void CaptureSession::restyleSelected()
{
    if (!m_selected)
        return;
    if (AnnotationPtr restyled = m_selected->restyled(m_style))
        replaceAnnotation(m_selected, std::move(restyled));
}

void CaptureSession::rebakeBlurs()
{
    // Pixelate what is underneath each blur, including earlier annotations, like the final image shows.
    const QVector<AnnotationPtr> items = m_annotations.items();
    for (int i = 0; i < items.size(); ++i) {
        const QRect base = items[i]->baseRect().toAlignedRect().intersected(m_virtual);
        if (base.isEmpty())
            continue;
        QImage pixels = m_capture.render(base, resultDpr());
        QPainter painter(&pixels);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.translate(-base.topLeft());
        for (int j = 0; j < i; ++j)
            items[j]->paint(painter);
        painter.end();
        items[i]->applyBase(pixels);
    }
}

void CaptureSession::undo()
{
    if (!m_annotations.undo())
        return;
    if (m_annotations.indexOf(m_selected) < 0)
        m_selected.reset();
    rebakeBlurs();
    emit changed();
}

void CaptureSession::redo()
{
    if (!m_annotations.redo())
        return;
    if (m_annotations.indexOf(m_selected) < 0)
        m_selected.reset();
    rebakeBlurs();
    emit changed();
}

qreal CaptureSession::resultDpr() const
{
    return m_capture.maxDprIn(m_selection.isEmpty() ? m_virtual : m_selection);
}

QSize CaptureSession::resultPixelSize() const
{
    const qreal dpr = resultDpr();
    return QSize(std::lround(m_selection.width() * dpr), std::lround(m_selection.height() * dpr));
}

QImage CaptureSession::result() const
{
    if (m_selection.isEmpty())
        return {};
    QImage image = m_capture.render(m_selection, resultDpr());
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.translate(-m_selection.topLeft());
    m_annotations.paint(painter);
    painter.end();
    image.setDevicePixelRatio(1.0);
    return image;
}

} // namespace snapline
