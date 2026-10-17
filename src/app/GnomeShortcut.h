#pragma once

#include <QKeySequence>
#include <QString>

namespace snapline::GnomeShortcut {

bool isGnome();

// Adds/updates a GNOME custom keyboard shortcut running `command`. If the shortcut is plain Print and
// releasePrintKey is set, GNOME's own screenshot UI is unbound from Print so ours receives it.
bool install(const QKeySequence &sequence, const QString &command, bool releasePrintKey, QString *error);

} // namespace snapline::GnomeShortcut
