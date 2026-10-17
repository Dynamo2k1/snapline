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
    emit changed();
}

void CaptureSession::setStrokeWidth(qreal width)
{
    m_style.width = qBound<qreal>(1, width, 30);
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
    if (!annotation || annotation->isEmpty()) {
        emit changed();
        return;
    }
    const QRect base = annotation->baseRect().toAlignedRect().intersected(m_virtual);
    if (!base.isEmpty()) {
        // Pixelate what is underneath, including earlier annotations, like the final image will show.
        QImage pixels = m_capture.render(base, resultDpr());
        QPainter painter(&pixels);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.translate(-base.topLeft());
        m_annotations.paint(painter);
        painter.end();
        annotation->applyBase(pixels);
    }
    m_annotations.push(std::move(annotation));
    emit changed();
}

void CaptureSession::undo()
{
    if (m_annotations.undo())
        emit changed();
}

void CaptureSession::redo()
{
    if (m_annotations.redo())
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
