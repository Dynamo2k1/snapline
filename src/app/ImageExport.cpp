#include "app/ImageExport.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace snapline {

bool hasOptiPng()
{
    return !QStandardPaths::findExecutable(QStringLiteral("optipng")).isEmpty();
}

namespace {

QByteArray optimize(const QByteArray &png)
{
    QTemporaryDir dir;
    if (!dir.isValid())
        return png;
    const QString path = dir.filePath(QStringLiteral("shot.png"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(png) != png.size())
        return png;
    file.close();

    QProcess process;
    process.start(QStringLiteral("optipng"), {QStringLiteral("-quiet"), QStringLiteral("-o2"), path});
    if (!process.waitForFinished(60000) || process.exitCode() != 0)
        return png;
    QFile result(path);
    if (!result.open(QIODevice::ReadOnly))
        return png;
    const QByteArray optimized = result.readAll();
    return optimized.isEmpty() ? png : optimized;
}

} // namespace

QByteArray encodeImage(const QImage &image, const EncodeOptions &options)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    const bool jpeg = options.format == "jpg" || options.format == "jpeg";
    if (jpeg)
        image.convertToFormat(QImage::Format_RGB32).save(&buffer, "JPG", options.jpegQuality);
    else
        image.save(&buffer, "PNG");
    buffer.close();
    if (!jpeg && options.optimizePng && hasOptiPng())
        bytes = optimize(bytes);
    return bytes;
}

bool writeImage(const QImage &image, const QString &path, const EncodeOptions &options, QString *error)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    const QByteArray bytes = encodeImage(image, options);
    QSaveFile file(path);
    if (bytes.isEmpty() || !file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

} // namespace snapline
