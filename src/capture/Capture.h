#pragma once

#include <QImage>
#include <QPair>
#include <QRect>
#include <QString>
#include <QVector>

namespace snapline {

// One monitor: its logical geometry on the virtual desktop and its pixels at native resolution.
struct ScreenImage {
    QString name;
    QRect geometry;
    QImage image;

    qreal dpr() const { return geometry.width() > 0 ? qreal(image.width()) / geometry.width() : 1.0; }
};

struct Capture {
    QVector<ScreenImage> screens;

    bool isNull() const { return screens.isEmpty(); }
    QRect virtualGeometry() const;
    qreal maxDprIn(const QRect &logical) const;

    // Renders a logical region of the virtual desktop at the given device pixel ratio.
    QImage render(const QRect &logical, qreal dpr) const;

    // Splits one image of the whole virtual desktop (portal, grim) into per-screen images.
    static Capture fromDesktopImage(const QImage &desktop, const QVector<QPair<QString, QRect>> &screens);
};

} // namespace snapline
