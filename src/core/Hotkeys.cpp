#include "core/Hotkeys.h"

#include <QHash>
#include <QSet>
#include <QStringList>

namespace snapline {

QString keysymName(int key)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return QString(QChar('a' + (key - Qt::Key_A)));
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
        return QString(QChar('0' + (key - Qt::Key_0)));
    if (key >= Qt::Key_F1 && key <= Qt::Key_F35)
        return QStringLiteral("F%1").arg(key - Qt::Key_F1 + 1);

    static const QHash<int, QString> named = {
        {Qt::Key_Print, QStringLiteral("Print")},
        {Qt::Key_SysReq, QStringLiteral("Sys_Req")},
        {Qt::Key_Space, QStringLiteral("space")},
        {Qt::Key_Return, QStringLiteral("Return")},
        {Qt::Key_Enter, QStringLiteral("KP_Enter")},
        {Qt::Key_Escape, QStringLiteral("Escape")},
        {Qt::Key_Tab, QStringLiteral("Tab")},
        {Qt::Key_Backspace, QStringLiteral("BackSpace")},
        {Qt::Key_Insert, QStringLiteral("Insert")},
        {Qt::Key_Delete, QStringLiteral("Delete")},
        {Qt::Key_Home, QStringLiteral("Home")},
        {Qt::Key_End, QStringLiteral("End")},
        {Qt::Key_PageUp, QStringLiteral("Page_Up")},
        {Qt::Key_PageDown, QStringLiteral("Page_Down")},
        {Qt::Key_Left, QStringLiteral("Left")},
        {Qt::Key_Right, QStringLiteral("Right")},
        {Qt::Key_Up, QStringLiteral("Up")},
        {Qt::Key_Down, QStringLiteral("Down")},
        {Qt::Key_Pause, QStringLiteral("Pause")},
        {Qt::Key_ScrollLock, QStringLiteral("Scroll_Lock")},
        {Qt::Key_Minus, QStringLiteral("minus")},
        {Qt::Key_Equal, QStringLiteral("equal")},
        {Qt::Key_Comma, QStringLiteral("comma")},
        {Qt::Key_Period, QStringLiteral("period")},
        {Qt::Key_Slash, QStringLiteral("slash")},
        {Qt::Key_Semicolon, QStringLiteral("semicolon")},
        {Qt::Key_Apostrophe, QStringLiteral("apostrophe")},
        {Qt::Key_BracketLeft, QStringLiteral("bracketleft")},
        {Qt::Key_BracketRight, QStringLiteral("bracketright")},
        {Qt::Key_Backslash, QStringLiteral("backslash")},
        {Qt::Key_QuoteLeft, QStringLiteral("grave")},
    };
    return named.value(key);
}

QString portalTrigger(const QKeySequence &sequence)
{
    if (sequence.count() != 1)
        return {};
    const QKeyCombination combo = sequence[0];
    const QString key = keysymName(combo.key());
    if (key.isEmpty())
        return {};

    QStringList parts;
    const auto mods = combo.keyboardModifiers();
    if (mods & Qt::ControlModifier)
        parts << QStringLiteral("CTRL");
    if (mods & Qt::AltModifier)
        parts << QStringLiteral("ALT");
    if (mods & Qt::ShiftModifier)
        parts << QStringLiteral("SHIFT");
    if (mods & Qt::MetaModifier)
        parts << QStringLiteral("LOGO");
    parts << key;
    return parts.join(QLatin1Char('+'));
}

QString gtkAccelerator(const QKeySequence &sequence)
{
    if (sequence.count() != 1)
        return {};
    const QKeyCombination combo = sequence[0];
    const QString key = keysymName(combo.key());
    if (key.isEmpty())
        return {};

    QString accel;
    const auto mods = combo.keyboardModifiers();
    if (mods & Qt::ControlModifier)
        accel += QStringLiteral("<Control>");
    if (mods & Qt::AltModifier)
        accel += QStringLiteral("<Alt>");
    if (mods & Qt::ShiftModifier)
        accel += QStringLiteral("<Shift>");
    if (mods & Qt::MetaModifier)
        accel += QStringLiteral("<Super>");
    return accel + key;
}

HotkeyError validateHotkeys(const QList<HotkeyBinding> &bindings, QString *offendingId)
{
    QSet<QString> seen;
    for (const HotkeyBinding &binding : bindings) {
        if (!binding.enabled)
            continue;
        const QString trigger = portalTrigger(binding.sequence);
        if (trigger.isEmpty()) {
            if (offendingId)
                *offendingId = binding.id;
            return HotkeyError::Invalid;
        }
        if (seen.contains(trigger)) {
            if (offendingId)
                *offendingId = binding.id;
            return HotkeyError::Duplicate;
        }
        seen.insert(trigger);
    }
    return HotkeyError::None;
}

} // namespace snapline
