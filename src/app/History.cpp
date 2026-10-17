#include "app/History.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace snapline {

namespace {
constexpr int kMaxEntries = 200;
}

History::History()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_path = QDir(dir).filePath(QStringLiteral("history.json"));

    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonArray array = QJsonDocument::fromJson(file.readAll()).array();
    for (const QJsonValue &v : array) {
        const QJsonObject o = v.toObject();
        const QUrl url(o.value(QStringLiteral("url")).toString());
        if (url.isValid())
            m_entries.push_back({QDateTime::fromString(o.value(QStringLiteral("time")).toString(), Qt::ISODate), url});
    }
}

void History::add(const QUrl &url)
{
    m_entries.prepend({QDateTime::currentDateTime(), url});
    while (m_entries.size() > kMaxEntries)
        m_entries.removeLast();
    save();
}

QList<History::Entry> History::recent(int count) const
{
    return m_entries.mid(0, count);
}

void History::clear()
{
    m_entries.clear();
    save();
}

void History::save() const
{
    QJsonArray array;
    for (const Entry &e : m_entries)
        array.append(QJsonObject{{QStringLiteral("time"), e.time.toString(Qt::ISODate)},
                                 {QStringLiteral("url"), e.url.toString()}});
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile file(m_path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
        file.commit();
    }
}

} // namespace snapline
