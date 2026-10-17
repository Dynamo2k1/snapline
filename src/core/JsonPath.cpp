#include "core/JsonPath.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>

namespace snapline {

QString extractJsonPath(const QByteArray &body, const QString &path, bool *ok)
{
    auto fail = [ok]() {
        if (ok)
            *ok = false;
        return QString();
    };

    if (path.trimmed().isEmpty()) {
        const QString text = QString::fromUtf8(body).trimmed();
        if (ok)
            *ok = !text.isEmpty();
        return text;
    }

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &error);
    if (error.error != QJsonParseError::NoError)
        return fail();

    QJsonValue current = doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object());
    static const QRegularExpression token(QStringLiteral(R"(([^.\[\]]+)|\[(\d+)\])"));
    auto it = token.globalMatch(path.trimmed());
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        if (!match.captured(1).isEmpty()) {
            if (!current.isObject())
                return fail();
            current = current.toObject().value(match.captured(1));
        } else {
            if (!current.isArray())
                return fail();
            const QJsonArray array = current.toArray();
            const int index = match.captured(2).toInt();
            if (index >= array.size())
                return fail();
            current = array.at(index);
        }
        if (current.isUndefined() || current.isNull())
            return fail();
    }

    QString value;
    if (current.isString())
        value = current.toString();
    else if (current.isDouble())
        value = QString::number(current.toDouble(), 'g', 17);
    if (value.isEmpty())
        return fail();
    if (ok)
        *ok = true;
    return value;
}

} // namespace snapline
