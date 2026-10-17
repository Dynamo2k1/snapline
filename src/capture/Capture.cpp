#include "capture/Capture.h"

#include <QPainter>
#include <cmath>

namespace snapline {

QRect Capture::virtualGeometry() const
{
    QRect united;
    for (const ScreenImage &s : screens)
        united = united.united(s.geometry);
    return united;
}

qreal Capture::maxDprIn(const QRect &logical) const
{
    qreal best = 0;
    for (const ScreenImage &s : screens) {
        if (s.geometry.intersects(logical))
            best = std::max(best, s.dpr());
    }
    return best > 0 ? best : 1.0;
}

QImage Capture::render(const QRect &logical, qreal dpr) const
{
    if (logical.isEmpty())
        return {};
    const QSize pixels(std::lround(logical.width() * dpr), std::lround(logical.height() * dpr));
    QImage out(pixels, QImage::Format_RGB32);
    out.fill(Qt::black);
    out.setDevicePixelRatio(dpr);

    QPainter painter(&out);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    for (const ScreenImage &s : screens) {
        const QRect part = s.geometry.intersected(logical);
        if (part.isEmpty())
            continue;
        const qreal sdpr = s.dpr();
        const QRectF source((part.x() - s.geometry.x()) * sdpr, (part.y() - s.geometry.y()) * sdpr,
                            part.width() * sdpr, part.height() * sdpr);
        const QRectF target(part.topLeft() - logical.topLeft(), part.size());
        painter.drawImage(target, s.image, source);
    }
    return out;
}

Capture Capture::fromDesktopImage(const QImage &desktop, const QVector<QPair<QString, QRect>> &screens)
{
    Capture capture;
    if (desktop.isNull() || screens.isEmpty())
        return capture;

    QRect virt;
    for (const auto &s : screens)
        virt = virt.united(s.second);

    const qreal sx = qreal(desktop.width()) / virt.width();
    const qreal sy = qreal(desktop.height()) / virt.height();
    for (const auto &s : screens) {
        const QRect &g = s.second;
        const QRect source(std::lround((g.x() - virt.x()) * sx), std::lround((g.y() - virt.y()) * sy),
                           std::lround(g.width() * sx), std::lround(g.height() * sy));
        capture.screens.push_back({s.first, g, desktop.copy(source)});
    }
    return capture;
}

} // namespace snapline
