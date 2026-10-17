#pragma once

#include <QColor>
#include <QImage>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <memory>

class QPainter;

namespace snapline {

enum class Tool { None, Pen, Line, Arrow, Rect, Circle, Marker, Text, Blur };

struct Style {
    QColor color = QColor(255, 0, 0);
    qreal width = 3;
};

// Annotations live in logical virtual-desktop coordinates.
class Annotation {
public:
    virtual ~Annotation() = default;
    virtual void paint(QPainter &painter) const = 0;
    virtual void update(const QPointF &point, bool constrain) { Q_UNUSED(point) Q_UNUSED(constrain) }
    virtual bool isEmpty() const = 0;

    // Annotations that need the underlying pixels (blur) report the region here; the session
    // renders it and passes the result to applyBase() once the drag is finished.
    virtual QRectF baseRect() const { return {}; }
    virtual void applyBase(const QImage &base) { Q_UNUSED(base) }
};

using AnnotationPtr = std::shared_ptr<Annotation>;

AnnotationPtr beginAnnotation(Tool tool, const QPointF &start, const Style &style);
AnnotationPtr makeTextAnnotation(const QPointF &topLeft, const QString &text, const QColor &color, int pixelSize);

int textPixelSize(qreal strokeWidth);
QPolygonF arrowHead(const QPointF &from, const QPointF &to, qreal strokeWidth);
QImage pixelate(const QImage &source, int blockPixels);

} // namespace snapline
