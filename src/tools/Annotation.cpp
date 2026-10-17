#include "tools/Annotation.h"

#include <QFont>
#include <QFontMetricsF>
#include <QLineF>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

namespace snapline {

namespace {

QPen strokePen(const QColor &color, qreal width)
{
    return QPen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
}

QPointF snapTo45(const QPointF &from, const QPointF &to)
{
    QLineF line(from, to);
    line.setAngle(std::round(line.angle() / 45.0) * 45.0);
    return line.p2();
}

QRectF dragRect(const QPointF &a, QPointF b, bool square)
{
    if (square) {
        const qreal dx = b.x() - a.x();
        const qreal dy = b.y() - a.y();
        const qreal side = std::max(std::abs(dx), std::abs(dy));
        b = QPointF(a.x() + std::copysign(side, dx), a.y() + std::copysign(side, dy));
    }
    return QRectF(a, b).normalized();
}

bool strokeContains(const QPainterPath &path, qreal width, const QPointF &point, qreal tolerance)
{
    QPainterPathStroker stroker;
    stroker.setWidth(width + 2 * tolerance);
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    return stroker.createStroke(path).contains(point);
}

constexpr int kTextFlags = Qt::AlignLeft | Qt::AlignTop | Qt::TextDontClip;

class StrokeAnnotation : public Annotation {
public:
    StrokeAnnotation(const QPointF &start, const Style &style, bool marker)
        : m_style(style), m_marker(marker)
    {
        m_path.moveTo(start);
    }

    void update(const QPointF &point, bool) override { m_path.lineTo(point); }

    void paint(QPainter &p) const override
    {
        QColor color = m_style.color;
        qreal width = m_style.width;
        if (m_marker) {
            color.setAlpha(110);
            width = std::max<qreal>(width * 4, 12);
        }
        p.save();
        p.setPen(strokePen(color, width));
        p.setBrush(Qt::NoBrush);
        if (m_path.elementCount() == 1)
            p.drawPoint(m_path.currentPosition());
        else
            p.drawPath(m_path);
        p.restore();
    }

    bool isEmpty() const override { return false; }

    QRectF bounds() const override
    {
        const qreal pad = paintWidth() / 2;
        return m_path.boundingRect().adjusted(-pad, -pad, pad, pad);
    }

    bool hitTest(const QPointF &point, qreal tolerance) const override
    {
        if (m_path.elementCount() == 1)
            return QLineF(point, m_path.currentPosition()).length() <= paintWidth() / 2 + tolerance;
        return strokeContains(m_path, paintWidth(), point, tolerance);
    }

    AnnotationPtr translated(const QPointF &offset) const override
    {
        auto copy = std::make_shared<StrokeAnnotation>(*this);
        copy->m_path.translate(offset);
        return copy;
    }

    AnnotationPtr restyled(const Style &style) const override
    {
        auto copy = std::make_shared<StrokeAnnotation>(*this);
        copy->m_style = style;
        return copy;
    }

private:
    qreal paintWidth() const { return m_marker ? std::max<qreal>(m_style.width * 4, 12) : m_style.width; }

    Style m_style;
    bool m_marker;
    QPainterPath m_path;
};

class LineAnnotation : public Annotation {
public:
    LineAnnotation(const QPointF &start, const Style &style, bool arrow)
        : m_style(style), m_arrow(arrow), m_start(start), m_end(start) {}

    void update(const QPointF &point, bool constrain) override
    {
        m_end = constrain ? snapTo45(m_start, point) : point;
    }

    void paint(QPainter &p) const override
    {
        p.save();
        p.setPen(strokePen(m_style.color, m_style.width));
        if (m_arrow) {
            const QPolygonF head = arrowHead(m_start, m_end, m_style.width);
            // Stop the shaft at the head's base so the round cap does not poke through the tip.
            const QPointF base = (head[1] + head[2]) / 2;
            p.drawLine(m_start, base);
            p.setPen(Qt::NoPen);
            p.setBrush(m_style.color);
            p.drawPolygon(head);
        } else {
            p.drawLine(m_start, m_end);
        }
        p.restore();
    }

    bool isEmpty() const override { return QLineF(m_start, m_end).length() < 2; }

    QRectF bounds() const override
    {
        QRectF r = QRectF(m_start, m_end).normalized();
        if (m_arrow)
            r = r.united(arrowHead(m_start, m_end, m_style.width).boundingRect());
        const qreal pad = m_style.width / 2;
        return r.adjusted(-pad, -pad, pad, pad);
    }

    bool hitTest(const QPointF &point, qreal tolerance) const override
    {
        QPainterPath path(m_start);
        path.lineTo(m_end);
        if (strokeContains(path, m_style.width, point, tolerance))
            return true;
        return m_arrow && arrowHead(m_start, m_end, m_style.width).containsPoint(point, Qt::OddEvenFill);
    }

    AnnotationPtr translated(const QPointF &offset) const override
    {
        auto copy = std::make_shared<LineAnnotation>(*this);
        copy->m_start += offset;
        copy->m_end += offset;
        return copy;
    }

    AnnotationPtr restyled(const Style &style) const override
    {
        auto copy = std::make_shared<LineAnnotation>(*this);
        copy->m_style = style;
        return copy;
    }

private:
    Style m_style;
    bool m_arrow;
    QPointF m_start;
    QPointF m_end;
};

class ShapeAnnotation : public Annotation {
public:
    ShapeAnnotation(const QPointF &start, const Style &style, bool ellipse)
        : m_style(style), m_ellipse(ellipse), m_start(start), m_rect(start, start) {}

    void update(const QPointF &point, bool constrain) override { m_rect = dragRect(m_start, point, constrain); }

    void paint(QPainter &p) const override
    {
        p.save();
        p.setPen(strokePen(m_style.color, m_style.width));
        p.setBrush(Qt::NoBrush);
        if (m_ellipse)
            p.drawEllipse(m_rect);
        else
            p.drawRect(m_rect);
        p.restore();
    }

    bool isEmpty() const override { return m_rect.width() < 2 && m_rect.height() < 2; }

    QRectF bounds() const override
    {
        const qreal pad = m_style.width / 2;
        return m_rect.adjusted(-pad, -pad, pad, pad);
    }

    // Only the outline counts, so you can still draw inside a rectangle or circle.
    bool hitTest(const QPointF &point, qreal tolerance) const override
    {
        QPainterPath path;
        if (m_ellipse)
            path.addEllipse(m_rect);
        else
            path.addRect(m_rect);
        return strokeContains(path, m_style.width, point, tolerance);
    }

    AnnotationPtr translated(const QPointF &offset) const override
    {
        auto copy = std::make_shared<ShapeAnnotation>(*this);
        copy->m_start += offset;
        copy->m_rect.translate(offset);
        return copy;
    }

    AnnotationPtr restyled(const Style &style) const override
    {
        auto copy = std::make_shared<ShapeAnnotation>(*this);
        copy->m_style = style;
        return copy;
    }

private:
    Style m_style;
    bool m_ellipse;
    QPointF m_start;
    QRectF m_rect;
};

class BlurAnnotation : public Annotation {
public:
    explicit BlurAnnotation(const QPointF &start) : m_start(start), m_rect(start, start) {}

    void update(const QPointF &point, bool constrain) override { m_rect = dragRect(m_start, point, constrain); }

    void paint(QPainter &p) const override
    {
        p.save();
        if (m_patch.isNull()) {
            p.setPen(QPen(QColor(255, 255, 255, 200), 1, Qt::DashLine));
            p.setBrush(QColor(128, 128, 128, 60));
            p.drawRect(m_rect);
        } else {
            p.drawImage(m_rect, m_patch);
        }
        p.restore();
    }

    bool isEmpty() const override { return m_rect.width() < 3 || m_rect.height() < 3; }
    QRectF bounds() const override { return m_rect; }
    bool hitTest(const QPointF &point, qreal tolerance) const override
    {
        return m_rect.adjusted(-tolerance, -tolerance, tolerance, tolerance).contains(point);
    }

    // The moved copy keeps the old pixels until the session re-renders what is now underneath.
    AnnotationPtr translated(const QPointF &offset) const override
    {
        auto copy = std::make_shared<BlurAnnotation>(*this);
        copy->m_start += offset;
        copy->m_rect.translate(offset);
        return copy;
    }

    QRectF baseRect() const override { return m_rect; }
    void applyBase(const QImage &base) override
    {
        const int block = std::max(8, int(std::lround(12 * base.devicePixelRatio())));
        m_patch = pixelate(base, block);
    }

private:
    QPointF m_start;
    QRectF m_rect;
    QImage m_patch;
};

class TextAnnotation : public Annotation {
public:
    TextAnnotation(const QPointF &topLeft, const QString &text, const QColor &color, int pixelSize)
        : m_topLeft(topLeft), m_text(text), m_color(color), m_pixelSize(pixelSize) {}

    void paint(QPainter &p) const override
    {
        p.save();
        QFont font = p.font();
        font.setPixelSize(m_pixelSize);
        p.setFont(font);
        p.setPen(m_color);
        p.drawText(QRectF(m_topLeft, QSizeF(100000, 100000)), kTextFlags, m_text);
        p.restore();
    }

    bool isEmpty() const override { return m_text.trimmed().isEmpty(); }

    QRectF bounds() const override
    {
        QFont font;
        font.setPixelSize(m_pixelSize);
        return QFontMetricsF(font).boundingRect(QRectF(m_topLeft, QSizeF(100000, 100000)), kTextFlags, m_text);
    }

    bool hitTest(const QPointF &point, qreal tolerance) const override
    {
        return bounds().adjusted(-tolerance, -tolerance, tolerance, tolerance).contains(point);
    }

    AnnotationPtr translated(const QPointF &offset) const override
    {
        auto copy = std::make_shared<TextAnnotation>(*this);
        copy->m_topLeft += offset;
        return copy;
    }

    AnnotationPtr restyled(const Style &style) const override
    {
        auto copy = std::make_shared<TextAnnotation>(*this);
        copy->m_color = style.color;
        copy->m_pixelSize = textPixelSize(style.width);
        return copy;
    }

    std::optional<TextContent> textContent() const override
    {
        return TextContent{m_topLeft, m_text, m_color, m_pixelSize};
    }

private:
    QPointF m_topLeft;
    QString m_text;
    QColor m_color;
    int m_pixelSize;
};

} // namespace

AnnotationPtr beginAnnotation(Tool tool, const QPointF &start, const Style &style)
{
    switch (tool) {
    case Tool::Pen: return std::make_shared<StrokeAnnotation>(start, style, false);
    case Tool::Marker: return std::make_shared<StrokeAnnotation>(start, style, true);
    case Tool::Line: return std::make_shared<LineAnnotation>(start, style, false);
    case Tool::Arrow: return std::make_shared<LineAnnotation>(start, style, true);
    case Tool::Rect: return std::make_shared<ShapeAnnotation>(start, style, false);
    case Tool::Circle: return std::make_shared<ShapeAnnotation>(start, style, true);
    case Tool::Blur: return std::make_shared<BlurAnnotation>(start);
    case Tool::Text:
    case Tool::None: break;
    }
    return nullptr;
}

AnnotationPtr makeTextAnnotation(const QPointF &topLeft, const QString &text, const QColor &color, int pixelSize)
{
    return std::make_shared<TextAnnotation>(topLeft, text, color, pixelSize);
}

int textPixelSize(qreal strokeWidth)
{
    return int(12 + strokeWidth * 2);
}

QPolygonF arrowHead(const QPointF &from, const QPointF &to, qreal strokeWidth)
{
    const QLineF shaft(to, from);
    const qreal length = std::min(std::max<qreal>(10, strokeWidth * 4), std::max<qreal>(shaft.length(), 1));
    const QPointF dir = shaft.length() > 0 ? (from - to) / shaft.length() : QPointF(-1, 0);
    const QPointF base = to + dir * length;
    const QPointF perp(-dir.y(), dir.x());
    const qreal halfWidth = length * 0.5;
    return QPolygonF({to, base + perp * halfWidth, base - perp * halfWidth});
}

QImage pixelate(const QImage &source, int blockPixels)
{
    if (source.isNull() || blockPixels < 2)
        return source;
    const QSize small(std::max(1, source.width() / blockPixels), std::max(1, source.height() / blockPixels));
    QImage out = source.scaled(small, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                     .scaled(source.size(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
    out.setDevicePixelRatio(source.devicePixelRatio());
    return out;
}

} // namespace snapline
