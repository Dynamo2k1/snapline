#include "app/GnomeShortcut.h"

#include <QFileInfo>
#include <QHash>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>

namespace snapline::GnomeShortcut {

namespace {

const QString kMediaKeys = QStringLiteral("org.gnome.settings-daemon.plugins.media-keys");
const QString kShellKeys = QStringLiteral("org.gnome.shell.keybindings");
const QString kCustomList = QStringLiteral("custom-keybindings");
const QString kPathPrefix = QStringLiteral("/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/");
// GNOME's screenshot tool keys (Print, Shift+Print and Alt+Print by default).
const QStringList kScreenshotKeys = {QStringLiteral("show-screenshot-ui"), QStringLiteral("screenshot"),
                                     QStringLiteral("screenshot-window")};

struct Binding {
    QString id;
    QString path;
    QString command;
    QString name;
};

const QList<Binding> &bindings()
{
    static const QList<Binding> list = {
        {QStringLiteral("main"), kPathPrefix + QStringLiteral("snapline/"), QStringLiteral("gui"),
         QObject::tr("Snapline: take a screenshot")},
        {QStringLiteral("save_full"), kPathPrefix + QStringLiteral("snapline-save-full/"), QStringLiteral("full"),
         QObject::tr("Snapline: save the full screen")},
        {QStringLiteral("upload_full"), kPathPrefix + QStringLiteral("snapline-upload-full/"),
         QStringLiteral("full-upload"), QObject::tr("Snapline: upload the full screen")},
    };
    return list;
}

QString customSchema(const QString &path)
{
    return kMediaKeys + QStringLiteral(".custom-keybinding:") + path;
}

bool gsettings(const QStringList &args, QString *output = nullptr, QString *error = nullptr)
{
    QProcess process;
    process.start(QStringLiteral("gsettings"), args);
    if (!process.waitForFinished(5000) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error)
            *error = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        return false;
    }
    if (output)
        *output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    return true;
}

QString get(const QString &schema, const QString &key)
{
    QString value;
    gsettings({QStringLiteral("get"), schema, key}, &value);
    return value;
}

QStringList parseList(const QString &value)
{
    QStringList items;
    static const QRegularExpression quoted(QStringLiteral("'((?:[^'\\\\]|\\\\.)*)'"));
    for (auto it = quoted.globalMatch(value); it.hasNext();)
        items << it.next().captured(1).replace(QStringLiteral("\\'"), QStringLiteral("'"));
    return items;
}

QString quote(const QString &value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\")).replace(QLatin1Char('\''), QStringLiteral("\\'"));
    return QLatin1Char('\'') + escaped + QLatin1Char('\'');
}

QString formatList(const QStringList &items)
{
    QStringList quoted;
    for (const QString &item : items)
        quoted << quote(item);
    return QLatin1Char('[') + quoted.join(QStringLiteral(", ")) + QLatin1Char(']');
}

// GTK accelerators can spell modifiers several ways ("<Primary>", "<Ctrl>", "<Control>"); compare
// a canonical form.
QString canonical(const QString &accel)
{
    static const QRegularExpression modifier(QStringLiteral("<([^>]+)>"));
    QStringList mods;
    for (auto it = modifier.globalMatch(accel); it.hasNext();) {
        const QString m = it.next().captured(1).toLower();
        if (m == QLatin1String("primary") || m == QLatin1String("control") || m == QLatin1String("ctrl"))
            mods << QStringLiteral("ctrl");
        else if (m == QLatin1String("alt") || m == QLatin1String("mod1"))
            mods << QStringLiteral("alt");
        else if (m == QLatin1String("super") || m == QLatin1String("meta") || m == QLatin1String("mod4"))
            mods << QStringLiteral("super");
        else
            mods << m;
    }
    mods.sort();
    mods.removeDuplicates();
    QString key = accel;
    key.remove(modifier);
    return mods.join(QLatin1Char('+')) + QLatin1Char('+') + key.toLower();
}

QHash<QString, QString> enabledAccels(const QList<HotkeyBinding> &list)
{
    QHash<QString, QString> accels; // canonical accel -> binding id
    for (const HotkeyBinding &b : list) {
        const QString accel = b.enabled ? gtkAccelerator(b.sequence) : QString();
        if (!accel.isEmpty())
            accels.insert(canonical(accel), b.id);
    }
    return accels;
}

void setCustomList(const QStringList &paths)
{
    gsettings({QStringLiteral("set"), kMediaKeys, kCustomList, formatList(paths)});
}

} // namespace

bool isGnome()
{
    static const bool gnome =
        qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains(QLatin1String("GNOME"), Qt::CaseInsensitive) &&
        !QFileInfo::exists(QStringLiteral("/.flatpak-info")) &&
        !QStandardPaths::findExecutable(QStringLiteral("gsettings")).isEmpty();
    return gnome;
}

QStringList conflicts(const QList<HotkeyBinding> &list)
{
    const QHash<QString, QString> used = enabledAccels(list);
    QStringList ids;
    for (const QString &key : kScreenshotKeys) {
        for (const QString &accel : parseList(get(kShellKeys, key))) {
            const QString id = used.value(canonical(accel));
            if (!id.isEmpty() && !ids.contains(id))
                ids << id;
        }
    }
    return ids;
}

bool apply(const QList<HotkeyBinding> &list, const QString &program, bool replaceScreenshotKeys,
           QStringList *skipped, QString *error)
{
    resume(); // never write over values saved by a suspend()
    QSettings settings;
    const QHash<QString, QString> used = enabledAccels(list);
    QSet<QString> blocked; // binding ids whose key GNOME keeps

    // Hand back screenshot keys we took earlier and no longer use; take the ones we now need.
    QStringList released = settings.value(QStringLiteral("gnome/released_keys")).toStringList();
    for (const QString &key : kScreenshotKeys) {
        QStringList accels = parseList(get(kShellKeys, key));
        const QStringList before = accels;
        for (auto it = released.begin(); it != released.end();) {
            const QString owner = it->section(QLatin1Char('\t'), 0, 0);
            const QString accel = it->section(QLatin1Char('\t'), 1);
            if (owner == key && !used.contains(canonical(accel))) {
                if (!accels.contains(accel))
                    accels << accel;
                it = released.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = accels.begin(); it != accels.end();) {
            const QString id = used.value(canonical(*it));
            if (id.isEmpty()) {
                ++it;
            } else if (replaceScreenshotKeys) {
                released << key + QLatin1Char('\t') + *it;
                it = accels.erase(it);
            } else {
                blocked.insert(id);
                ++it;
            }
        }
        if (accels != before)
            gsettings({QStringLiteral("set"), kShellKeys, key, formatList(accels)});
    }
    settings.setValue(QStringLiteral("gnome/released_keys"), released);

    QString quotedProgram = program;
    if (quotedProgram.contains(QLatin1Char(' ')))
        quotedProgram = QLatin1Char('"') + quotedProgram + QLatin1Char('"');

    QStringList paths = parseList(get(kMediaKeys, kCustomList));
    const QStringList pathsBefore = paths;
    for (const Binding &b : bindings()) {
        HotkeyBinding hotkey{b.id, {}, false};
        for (const HotkeyBinding &h : list) {
            if (h.id == b.id)
                hotkey = h;
        }
        const QString accel = hotkey.enabled ? gtkAccelerator(hotkey.sequence) : QString();
        if (accel.isEmpty() || blocked.contains(b.id)) {
            if (blocked.contains(b.id) && skipped)
                *skipped << b.id;
            paths.removeAll(b.path);
            continue;
        }
        const QString schema = customSchema(b.path);
        if (!gsettings({QStringLiteral("set"), schema, QStringLiteral("name"), quote(b.name)}, nullptr, error) ||
            !gsettings({QStringLiteral("set"), schema, QStringLiteral("command"),
                        quote(quotedProgram + QLatin1Char(' ') + b.command)},
                       nullptr, error) ||
            !gsettings({QStringLiteral("set"), schema, QStringLiteral("binding"), quote(accel)}, nullptr, error))
            return false;
        if (!paths.contains(b.path))
            paths << b.path;
    }
    if (paths != pathsBefore)
        setCustomList(paths);
    return true;
}

void suspend()
{
    QSettings settings;
    if (settings.contains(QStringLiteral("gnome/suspended")))
        return;
    QStringList saved; // "schema \t key \t raw value"
    for (const QString &key : kScreenshotKeys)
        saved << kShellKeys + QLatin1Char('\t') + key + QLatin1Char('\t') + get(kShellKeys, key);
    for (const Binding &b : bindings()) {
        const QString schema = customSchema(b.path);
        saved << schema + QStringLiteral("\tbinding\t") + get(schema, QStringLiteral("binding"));
    }
    settings.setValue(QStringLiteral("gnome/suspended"), saved);
    settings.sync();

    for (const QString &key : kScreenshotKeys)
        gsettings({QStringLiteral("set"), kShellKeys, key, QStringLiteral("[]")});
    for (const Binding &b : bindings())
        gsettings({QStringLiteral("set"), customSchema(b.path), QStringLiteral("binding"), QStringLiteral("''")});
}

void resume()
{
    if (!isGnome())
        return;
    QSettings settings;
    const QStringList saved = settings.value(QStringLiteral("gnome/suspended")).toStringList();
    if (saved.isEmpty())
        return;
    for (const QString &entry : saved) {
        const QStringList parts = entry.split(QLatin1Char('\t'));
        if (parts.size() == 3 && !parts[2].isEmpty())
            gsettings({QStringLiteral("set"), parts[0], parts[1], parts[2]});
    }
    settings.remove(QStringLiteral("gnome/suspended"));
}

} // namespace snapline::GnomeShortcut
