#pragma once

#include "core/Hotkeys.h"

#include <QStringList>

namespace snapline::GnomeShortcut {

// True on GNOME outside a sandbox, where hotkeys are installed as GNOME custom keyboard shortcuts.
bool isGnome();

// The ids of enabled bindings whose keys GNOME's own screenshot tool currently uses.
QStringList conflicts(const QList<HotkeyBinding> &bindings);

// Installs every enabled binding as a GNOME shortcut running `program <command>` and removes disabled
// ones. Keys that GNOME's screenshot tool uses are taken from it when replaceScreenshotKeys is set (and
// handed back once Snapline stops using them); otherwise those bindings are skipped and listed in
// skipped. Returns false with error set if gsettings fails.
bool apply(const QList<HotkeyBinding> &bindings, const QString &program, bool replaceScreenshotKeys,
           QStringList *skipped, QString *error);

// While a hotkey field is recording, GNOME must not act on the keys pressed, so its screenshot keys and
// Snapline's own shortcuts are switched off. resume() puts them back, also after a crash at startup.
void suspend();
void resume();

} // namespace snapline::GnomeShortcut
