#include "core/FileNameTemplate.h"

#include <QDir>
#include <QFileInfo>

namespace snapline {

QString expandFileNameTemplate(const QString &tpl, const QDateTime &time, int counter)
{
    QString out;
    for (int i = 0; i < tpl.size(); ++i) {
        const QChar c = tpl.at(i);
        if (c != QLatin1Char('%') || i + 1 >= tpl.size()) {
            out += c;
            continue;
        }
        const QChar spec = tpl.at(++i);
        switch (spec.unicode()) {
        case 'Y': out += time.toString(QStringLiteral("yyyy")); break;
        case 'm': out += time.toString(QStringLiteral("MM")); break;
        case 'd': out += time.toString(QStringLiteral("dd")); break;
        case 'H': out += time.toString(QStringLiteral("HH")); break;
        case 'M': out += time.toString(QStringLiteral("mm")); break;
        case 'S': out += time.toString(QStringLiteral("ss")); break;
        case 'n': out += QString::number(counter); break;
        case '%': out += QLatin1Char('%'); break;
        default:
            out += QLatin1Char('%');
            out += spec;
        }
    }
    out.replace(QLatin1Char('/'), QLatin1Char('_'));
    out = out.trimmed();
    return out.isEmpty() ? QStringLiteral("Screenshot") : out;
}

QString uniqueFilePath(const QString &dir, const QString &baseName, const QString &extension)
{
    const QDir d(dir);
    QString candidate = d.filePath(baseName + QLatin1Char('.') + extension);
    for (int n = 1; QFileInfo::exists(candidate); ++n)
        candidate = d.filePath(QStringLiteral("%1_%2.%3").arg(baseName).arg(n).arg(extension));
    return candidate;
}

} // namespace snapline
