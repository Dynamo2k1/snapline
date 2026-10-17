#pragma once

#include <QDateTime>
#include <QString>

namespace snapline {

// Expands %Y %m %d %H %M %S (date/time), %n (counter) and %% in a file name template.
QString expandFileNameTemplate(const QString &tpl, const QDateTime &time, int counter);

// dir/baseName.ext, with _1, _2, ... appended if that file already exists.
QString uniqueFilePath(const QString &dir, const QString &baseName, const QString &extension);

} // namespace snapline
