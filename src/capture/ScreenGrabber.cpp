#include "capture/ScreenGrabber.h"

#include "app/PortalRequest.h"

#include <QCursor>
#include <QFile>
#include <QGuiApplication>
#include <QPainter>
#include <QPixmap>
#include <QProcess>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#ifdef SNAPLINE_HAVE_X11
#include <QtGui/qguiapplication_platform.h>
#include <xcb/xfixes.h>
#include <cstdlib>
#endif

namespace snapline {

namespace {

#ifdef SNAPLINE_HAVE_X11
void drawX11Cursor(Capture &capture)
{
    auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    if (!x11)
        return;
    xcb_connection_t *conn = x11->connection();
    free(xcb_xfixes_query_version_reply(conn, xcb_xfixes_query_version(conn, 4, 0), nullptr));
    xcb_xfixes_get_cursor_image_reply_t *reply =
        xcb_xfixes_get_cursor_image_reply(conn, xcb_xfixes_get_cursor_image(conn), nullptr);
    if (!reply)
        return;

    const QPoint hotspot(reply->xhot, reply->yhot);
    const QImage cursor = QImage(reinterpret_cast<const uchar *>(xcb_xfixes_get_cursor_image_cursor_image(reply)),
                                 reply->width, reply->height, QImage::Format_ARGB32_Premultiplied)
                              .copy();
    free(reply);

    // Position from Qt (logical), hotspot and bitmap from XFixes (native pixels).
    const QPoint logical = QCursor::pos();
    for (ScreenImage &s : capture.screens) {
        if (!s.geometry.contains(logical))
            continue;
        const QPointF native = QPointF(logical - s.geometry.topLeft()) * s.dpr();
        QPainter painter(&s.image);
        painter.drawImage(native.toPoint() - hotspot, cursor);
    }
}
#endif

class QtGrabber : public ScreenGrabber {
public:
    using ScreenGrabber::ScreenGrabber;
    QString name() const override { return QStringLiteral("qt"); }

    void grab(bool includeCursor) override
    {
        Capture capture;
        for (QScreen *screen : QGuiApplication::screens()) {
            const QPixmap pixmap = screen->grabWindow(0);
            if (pixmap.isNull())
                continue;
            capture.screens.push_back({screen->name(), screen->geometry(), pixmap.toImage()});
        }
#ifdef SNAPLINE_HAVE_X11
        if (includeCursor && !isWayland())
            drawX11Cursor(capture);
#else
        Q_UNUSED(includeCursor)
#endif
        QTimer::singleShot(0, this, [this, capture]() {
            if (capture.isNull())
                emit failed(tr("Could not read the screen contents."));
            else
                emit finished(capture);
        });
    }
};

class GrimGrabber : public ScreenGrabber {
public:
    using ScreenGrabber::ScreenGrabber;
    QString name() const override { return QStringLiteral("grim"); }

    void grab(bool includeCursor) override
    {
        auto *process = new QProcess(this);
        QStringList args;
        if (includeCursor)
            args << QStringLiteral("-c");
        args << QStringLiteral("-t") << QStringLiteral("png") << QStringLiteral("-");
        connect(process, &QProcess::finished, this, [this, process](int code, QProcess::ExitStatus status) {
            process->deleteLater();
            const QByteArray data = process->readAllStandardOutput();
            if (status != QProcess::NormalExit || code != 0) {
                emit failed(tr("grim failed: %1").arg(QString::fromLocal8Bit(process->readAllStandardError()).trimmed()));
                return;
            }
            const Capture capture = Capture::fromDesktopImage(QImage::fromData(data, "PNG"), screenLayout());
            if (capture.isNull())
                emit failed(tr("grim returned no image."));
            else
                emit finished(capture);
        });
        connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError e) {
            if (e != QProcess::FailedToStart)
                return;
            process->deleteLater();
            emit failed(tr("Could not start grim. Install it or choose another capture backend."));
        });
        process->start(QStringLiteral("grim"), args);
    }
};

class PortalGrabber : public ScreenGrabber {
public:
    using ScreenGrabber::ScreenGrabber;
    QString name() const override { return QStringLiteral("portal"); }

    void grab(bool includeCursor) override
    {
        const QVariantMap options{{QStringLiteral("interactive"), false}, {QStringLiteral("modal"), false}};
        auto *request = PortalRequest::call(QStringLiteral("org.freedesktop.portal.Screenshot"),
                                            QStringLiteral("Screenshot"), {QString(), options}, this);
        connect(request, &PortalRequest::error, this, [this, request, includeCursor](const QString &message) {
            request->deleteLater();
            if (!QStandardPaths::findExecutable(QStringLiteral("grim")).isEmpty()) {
                fallBackToGrim(includeCursor);
                return;
            }
            emit failed(tr("The screenshot portal is not available: %1").arg(message));
        });
        connect(request, &PortalRequest::finished, this, [this, request](uint response, const QVariantMap &results) {
            request->deleteLater();
            if (response == 1) {
                emit failed(QString());
                return;
            }
            const QString path = QUrl(results.value(QStringLiteral("uri")).toString()).toLocalFile();
            if (response != 0 || path.isEmpty()) {
                emit failed(tr("The desktop refused the screenshot request."));
                return;
            }
            const QImage image(path);
            QFile::remove(path); // the portal wrote a fresh file just for us
            const Capture capture = Capture::fromDesktopImage(image, screenLayout());
            if (capture.isNull())
                emit failed(tr("The screenshot portal returned no image."));
            else
                emit finished(capture);
        });
    }

private:
    void fallBackToGrim(bool includeCursor)
    {
        auto *grim = new GrimGrabber(this);
        connect(grim, &ScreenGrabber::finished, this, [this, grim](const Capture &c) {
            grim->deleteLater();
            emit finished(c);
        });
        connect(grim, &ScreenGrabber::failed, this, [this, grim](const QString &m) {
            grim->deleteLater();
            emit failed(m);
        });
        grim->grab(includeCursor);
    }
};

bool isWlrootsDesktop()
{
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP").toLower();
    for (const char *name : {"sway", "hyprland", "river", "wlroots", "labwc", "niri", "wayfire"}) {
        if (desktop.contains(QLatin1String(name)))
            return true;
    }
    return false;
}

} // namespace

bool ScreenGrabber::isWayland()
{
    return QGuiApplication::platformName().startsWith(QLatin1String("wayland"));
}

QVector<QPair<QString, QRect>> ScreenGrabber::screenLayout()
{
    QVector<QPair<QString, QRect>> layout;
    for (QScreen *screen : QGuiApplication::screens())
        layout.push_back({screen->name(), screen->geometry()});
    return layout;
}

ScreenGrabber *ScreenGrabber::create(const QString &backend, QObject *parent)
{
    QString chosen = backend;
    if (chosen.isEmpty() || chosen == QLatin1String("auto")) {
        if (!isWayland())
            chosen = QStringLiteral("qt");
        else if (isWlrootsDesktop() && !QStandardPaths::findExecutable(QStringLiteral("grim")).isEmpty())
            chosen = QStringLiteral("grim");
        else
            chosen = QStringLiteral("portal");
    }
    if (chosen == QLatin1String("grim"))
        return new GrimGrabber(parent);
    if (chosen == QLatin1String("portal"))
        return new PortalGrabber(parent);
    return new QtGrabber(parent);
}

} // namespace snapline
