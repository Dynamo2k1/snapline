#pragma once

#include <QImage>
#include <QString>

namespace snapline::Clipboard {

void setImage(const QImage &image);
void setText(const QString &text);

} // namespace snapline::Clipboard
