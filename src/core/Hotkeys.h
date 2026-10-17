#pragma once

#include <QKeySequence>
#include <QList>
#include <QString>

namespace snapline {

struct HotkeyBinding {
    QString id;
    QKeySequence sequence;
    bool enabled = false;
};

enum class HotkeyError { None, Invalid, Duplicate };

HotkeyError validateHotkeys(const QList<HotkeyBinding> &bindings, QString *offendingId = nullptr);

// XKB keysym name for a Qt key ("Print", "a", "F5"); empty if the key cannot be bound globally.
QString keysymName(int qtKey);

// XDG shortcut trigger as used by the GlobalShortcuts portal, e.g. "CTRL+SHIFT+Print".
QString portalTrigger(const QKeySequence &sequence);

// GTK accelerator as used by GNOME custom keybindings, e.g. "<Control><Shift>Print".
QString gtkAccelerator(const QKeySequence &sequence);

} // namespace snapline
