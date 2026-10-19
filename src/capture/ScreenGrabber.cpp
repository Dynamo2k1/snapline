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
#include <QWindow>

#include <memory>

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

// Works on GNOME/mutter, where grim (wlroots-only) does not -- verified
// directly on this system (`flameshot full --raw` returns a real PNG on
// stdout, exit 0, no hang, where the raw portal call hangs indefinitely).
class FlameshotGrabber : public ScreenGrabber {
public:
    using ScreenGrabber::ScreenGrabber;
    QString name() const override { return QStringLiteral("flameshot"); }

    void grab(bool includeCursor) override
    {
        Q_UNUSED(includeCursor); // flameshot's CLI has no cursor-inclusion flag for `full`
        auto *process = new QProcess(this);
        connect(process, &QProcess::finished, this, [this, process](int code, QProcess::ExitStatus status) {
            process->deleteLater();
            const QByteArray data = process->readAllStandardOutput();
            if (status != QProcess::NormalExit || code != 0) {
                emit failed(tr("flameshot failed: %1").arg(QString::fromLocal8Bit(process->readAllStandardError()).trimmed()));
                return;
            }
            const Capture capture = Capture::fromDesktopImage(QImage::fromData(data, "PNG"), screenLayout());
            if (capture.isNull())
                emit failed(tr("flameshot returned no image."));
            else
                emit finished(capture);
        });
        connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError e) {
            if (e != QProcess::FailedToStart)
                return;
            process->deleteLater();
            emit failed(tr("Could not start flameshot. Install it or choose another capture backend."));
        });
        process->start(QStringLiteral("flameshot"), {QStringLiteral("full"), QStringLiteral("--raw")});
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

        // The portal request has no built-in deadline. On some desktop
        // implementations (observed: xdg-desktop-portal-gnome mishandling an
        // empty parent_window -- this app has no visible window at the point
        // it requests a screenshot, so it always passes one -- as an internal
        // Wayland protocol error instead of a normal failure), the request
        // never emits either `error` or `finished`: it just hangs forever,
        // and neither handler below ever runs. A caller-side timeout is the
        // only way to recover from a portal that silently drops the request
        // rather than reporting one. 5s is generous for a working portal
        // (interactive=false, so no consent dialog is expected here) while
        // still being well short of "the user thinks the hotkey did nothing."
        auto done = std::make_shared<bool>(false);
        auto *timeout = new QTimer(this);
        timeout->setSingleShot(true);
        connect(timeout, &QTimer::timeout, this, [this, request, done, includeCursor]() {
            if (*done)
                return;
            *done = true;
            request->disconnect(this);
            request->deleteLater();
            fallBackToSubprocessGrabber(includeCursor, tr("the screenshot portal did not respond"));
        });
        if (!m_userMayBeAsked)
            timeout->start(5000);

        connect(request, &PortalRequest::error, this, [this, request, done, timeout, includeCursor](const QString &message) {
            if (*done)
                return;
            *done = true;
            timeout->deleteLater();
            request->deleteLater();
            fallBackToSubprocessGrabber(includeCursor, message);
        });
        connect(request, &PortalRequest::finished, this, [this, request, done, timeout](uint response, const QVariantMap &results) {
            if (*done)
                return;
            *done = true;
            timeout->deleteLater();
            request->deleteLater();
            if (response == 1) {
                emit failed(QString());
                return;
            }
            if (response == 2) {
                // GNOME has no stored answer for this app and only asks a focused app, so it refused.
                emit permissionDenied();
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
    // Tries grim first (wlroots compositors), then flameshot (works on GNOME/mutter,
    // where grim does not -- verified directly on this system), before giving up.
    void fallBackToSubprocessGrabber(bool includeCursor, const QString &portalMessage)
    {
        if (!QStandardPaths::findExecutable(QStringLiteral("grim")).isEmpty()) {
            fallBackTo(new GrimGrabber(this), includeCursor);
            return;
        }
        if (!QStandardPaths::findExecutable(QStringLiteral("flameshot")).isEmpty()) {
            fallBackTo(new FlameshotGrabber(this), includeCursor);
            return;
        }
        emit failed(portalMessage.isEmpty()
                         ? tr("The screenshot portal is not available, and neither grim nor flameshot is installed.")
                         : tr("The screenshot portal is not available (%1), and neither grim nor flameshot is installed.").arg(portalMessage));
    }

    void fallBackTo(ScreenGrabber *grabber, bool includeCursor)
    {
        connect(grabber, &ScreenGrabber::finished, this, [this, grabber](const Capture &c) {
            grabber->deleteLater();
            emit finished(c);
        });
        connect(grabber, &ScreenGrabber::failed, this, [this, grabber](const QString &m) {
            grabber->deleteLater();
            emit failed(m);
        });
        grabber->grab(includeCursor);
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
