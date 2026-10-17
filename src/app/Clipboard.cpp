#include "app/Clipboard.h"

#include "capture/ScreenGrabber.h"

#include <QBuffer>
#include <QClipboard>
#include <QGuiApplication>
#include <QProcess>
#include <QStandardPaths>

namespace snapline::Clipboard {

namespace {

// On Wayland a client may only set the selection while it has keyboard focus, and the overlay is gone
// by the time some actions finish. wl-copy serves the data from its own process, so prefer it there.
bool viaWlCopy(const QByteArray &data, const QString &mimeType)
{
    if (!ScreenGrabber::isWayland())
        return false;
    const QString wlCopy = QStandardPaths::findExecutable(QStringLiteral("wl-copy"));
    if (wlCopy.isEmpty())
        return false;
    QProcess process;
    // wl-copy forks a server that outlives this call; it must not hold our pipes open.
    process.setStandardOutputFile(QProcess::nullDevice());
    process.setStandardErrorFile(QProcess::nullDevice());
    process.start(wlCopy, {QStringLiteral("--type"), mimeType});
    if (!process.waitForStarted(2000))
        return false;
    process.write(data);
    process.closeWriteChannel();
    return process.waitForFinished(5000) && process.exitCode() == 0;
}

} // namespace

void setImage(const QImage &image)
{
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    if (!viaWlCopy(png, QStringLiteral("image/png")))
        QGuiApplication::clipboard()->setImage(image);
}

void setText(const QString &text)
{
    if (!viaWlCopy(text.toUtf8(), QStringLiteral("text/plain;charset=utf-8")))
        QGuiApplication::clipboard()->setText(text);
}

} // namespace snapline::Clipboard
