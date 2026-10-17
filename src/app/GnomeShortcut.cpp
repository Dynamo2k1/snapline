#include "app/GnomeShortcut.h"

#include "core/Hotkeys.h"

#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringList>

namespace snapline::GnomeShortcut {

namespace {

const QString kMediaKeys = QStringLiteral("org.gnome.settings-daemon.plugins.media-keys");
const QString kPath = QStringLiteral("/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/snapline/");

bool gsettings(const QStringList &args, QString *output, QString *error)
{
    QProcess process;
    process.start(QStringLiteral("gsettings"), args);
    if (!process.waitForFinished(5000) || process.exitCode() != 0) {
        if (error)
            *error = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        return false;
    }
    if (output)
        *output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    return true;
}

QString quote(const QString &value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\")).replace(QLatin1Char('\''), QStringLiteral("\\'"));
    return QLatin1Char('\'') + escaped + QLatin1Char('\'');
}

} // namespace

bool isGnome()
{
    return qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains(QLatin1String("GNOME"), Qt::CaseInsensitive) &&
           !QStandardPaths::findExecutable(QStringLiteral("gsettings")).isEmpty();
}

bool install(const QKeySequence &sequence, const QString &command, bool releasePrintKey, QString *error)
{
    const QString accel = gtkAccelerator(sequence);
    if (accel.isEmpty()) {
        *error = QObject::tr("This key combination cannot be used as a GNOME shortcut.");
        return false;
    }

    QString current;
    if (!gsettings({QStringLiteral("get"), kMediaKeys, QStringLiteral("custom-keybindings")}, &current, error))
        return false;
    QStringList paths;
    static const QRegularExpression quoted(QStringLiteral("'([^']*)'"));
    for (auto it = quoted.globalMatch(current); it.hasNext();)
        paths << it.next().captured(1);
    if (!paths.contains(kPath)) {
        paths << kPath;
        QStringList items;
        for (const QString &p : paths)
            items << quote(p);
        if (!gsettings({QStringLiteral("set"), kMediaKeys, QStringLiteral("custom-keybindings"),
                        QLatin1Char('[') + items.join(QStringLiteral(", ")) + QLatin1Char(']')},
                       nullptr, error))
            return false;
    }

    const QString schema = kMediaKeys + QStringLiteral(".custom-keybinding:") + kPath;
    if (!gsettings({QStringLiteral("set"), schema, QStringLiteral("name"), QStringLiteral("Snapline")}, nullptr, error) ||
        !gsettings({QStringLiteral("set"), schema, QStringLiteral("command"), command}, nullptr, error) ||
        !gsettings({QStringLiteral("set"), schema, QStringLiteral("binding"), accel}, nullptr, error))
        return false;

    if (releasePrintKey && accel == QLatin1String("Print")) {
        gsettings({QStringLiteral("set"), QStringLiteral("org.gnome.shell.keybindings"),
                   QStringLiteral("show-screenshot-ui"), QStringLiteral("[]")},
                  nullptr, nullptr);
    }
    return true;
}

} // namespace snapline::GnomeShortcut
