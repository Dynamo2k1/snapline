#pragma once

#include "core/Hotkeys.h"

#include <QObject>
#include <memory>

namespace snapline {

class HotkeyBackend;

// Global hotkeys: GNOME custom keyboard shortcuts on GNOME, XGrabKey on other X11 desktops and the
// GlobalShortcuts portal on other Wayland desktops.
class HotkeyManager : public QObject {
    Q_OBJECT

public:
    explicit HotkeyManager(QObject *parent = nullptr);
    ~HotkeyManager() override;

    void setBindings(const QList<HotkeyBinding> &bindings);
    // Whether keys the desktop's own screenshot tool uses (GNOME's Print) may be taken over.
    void setReplaceDesktopKeys(bool replace);
    // Switches the hotkeys off while a hotkey field in Settings records a new key.
    void setSuspended(bool suspended);
    QString backendName() const;

signals:
    void activated(const QString &id);
    void registrationFailed(const QString &id, const QString &reason);

private:
    std::unique_ptr<HotkeyBackend> m_backend;
};

class HotkeyBackend : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;
    virtual QString name() const = 0;
    virtual void setBindings(const QList<HotkeyBinding> &bindings) = 0;
    virtual void setReplaceDesktopKeys(bool replace) { Q_UNUSED(replace) }
    virtual void setSuspended(bool suspended) { Q_UNUSED(suspended) }

signals:
    void activated(const QString &id);
    void registrationFailed(const QString &id, const QString &reason);
};

} // namespace snapline
