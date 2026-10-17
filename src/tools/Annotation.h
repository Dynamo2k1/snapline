#pragma once

#include <QColor>
#include <QImage>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <memory>
#include <optional>

class QPainter;

namespace snapline {

enum class Tool { None, Pen, Line, Arrow, Rect, Circle, Marker, Text, Blur };

struct Style {
    QColor color = QColor(255, 0, 0);
    qreal width = 3;
};

struct TextContent {
    QPointF topLeft;
    QString text;
    QColor color;
    int pixelSize = 0;
};

class Annotation;
using AnnotationPtr = std::shared_ptr<Annotation>;

// Annotations live in logical virtual-desktop coordinates. Once committed they are treated as
// immutable: edits produce a modified copy so undo snapshots keep the old version.
class Annotation {
public:
    virtual ~Annotation() = default;
    virtual void paint(QPainter &painter) const = 0;
    virtual void update(const QPointF &point, bool constrain) { Q_UNUSED(point) Q_UNUSED(constrain) }
    virtual bool isEmpty() const = 0;

    virtual QRectF bounds() const = 0;
    // True if a click at point (within tolerance pixels) lands on what is drawn.
    virtual bool hitTest(const QPointF &point, qreal tolerance) const = 0;
    virtual AnnotationPtr translated(const QPointF &offset) const = 0;
    // A copy with a new colour and stroke width, or null if the annotation has no style (blur).
    virtual AnnotationPtr restyled(const Style &style) const { Q_UNUSED(style) return nullptr; }
    virtual std::optional<TextContent> textContent() const { return std::nullopt; }

    // Annotations that need the underlying pixels (blur) report the region here; the session
    // renders it, including the annotations below, and passes the result to applyBase().
    virtual QRectF baseRect() const { return {}; }
    virtual void applyBase(const QImage &base) { Q_UNUSED(base) }
};

AnnotationPtr beginAnnotation(Tool tool, const QPointF &start, const Style &style);
AnnotationPtr makeTextAnnotation(const QPointF &topLeft, const QString &text, const QColor &color, int pixelSize);

int textPixelSize(qreal strokeWidth);
QPolygonF arrowHead(const QPointF &from, const QPointF &to, qreal strokeWidth);
QImage pixelate(const QImage &source, int blockPixels);

} // namespace snapline
