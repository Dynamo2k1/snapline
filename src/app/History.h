#pragma once

#include <QDateTime>
#include <QList>
#include <QUrl>

namespace snapline {

// Local list of uploaded links (replaces the reference app's online account gallery).
class History {
public:
    struct Entry {
        QDateTime time;
        QUrl url;
    };

    History();
    void add(const QUrl &url);
    QList<Entry> recent(int count) const;
    void clear();

private:
    void save() const;

    QString m_path;
    QList<Entry> m_entries; // newest first
};

} // namespace snapline
