#pragma once

#include <QByteArray>
#include <QString>

namespace snapline {

// Resolves paths like "data.link" or "files[0].url" in a JSON body.
// An empty path returns the trimmed body itself (servers that answer with a bare URL).
QString extractJsonPath(const QByteArray &body, const QString &path, bool *ok = nullptr);

} // namespace snapline
