#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>

namespace snapline {

struct EncodeOptions {
    QByteArray format = "png"; // png | jpg
    int jpegQuality = 90;
    bool optimizePng = false;  // runs optipng when installed
};

QByteArray encodeImage(const QImage &image, const EncodeOptions &options);
bool writeImage(const QImage &image, const QString &path, const EncodeOptions &options, QString *error);
bool hasOptiPng();

} // namespace snapline
