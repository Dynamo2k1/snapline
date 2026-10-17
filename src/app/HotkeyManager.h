#pragma once

#include "core/Hotkeys.h"

#include <QObject>
#include <memory>

namespace snapline {

class HotkeyBackend;

// Global hotkeys: XGrabKey on X11, the GlobalShortcuts portal on Wayland.
class HotkeyManager : public QObject {
    Q_OBJECT

public:
    explicit HotkeyManager(QObject *parent = nullptr);
    ~HotkeyManager() override;

    void setBindings(const QList<HotkeyBinding> &bindings);
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

signals:
    void activated(const QString &id);
    void registrationFailed(const QString &id, const QString &reason);
};

} // namespace snapline
