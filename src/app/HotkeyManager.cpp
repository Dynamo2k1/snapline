#include "app/HotkeyManager.h"

#include "app/PortalRequest.h"
#include "capture/ScreenGrabber.h"

#include <QAbstractNativeEventFilter>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDBusVariant>
#include <QGuiApplication>
#include <QRandomGenerator>
#include <QVector>

#ifdef SNAPLINE_HAVE_X11
#include <QtGui/qguiapplication_platform.h>
#include <X11/Xlib.h>
#include <xcb/xcb.h>
// Xlib macros that collide with Qt/moc identifiers.
#undef None
#undef Bool
#undef Status
#undef KeyPress
#undef KeyRelease
#undef FocusIn
#undef FocusOut
#undef CursorShape
#undef Unsorted
#undef Expose
#undef Always
#undef Success
#endif

struct PortalShortcut {
    QString id;
    QVariantMap properties;
};
Q_DECLARE_METATYPE(PortalShortcut)
using PortalShortcutList = QList<PortalShortcut>;
Q_DECLARE_METATYPE(PortalShortcutList)

QDBusArgument &operator<<(QDBusArgument &arg, const PortalShortcut &s)
{
    arg.beginStructure();
    arg << s.id << s.properties;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, PortalShortcut &s)
{
    arg.beginStructure();
    arg >> s.id >> s.properties;
    arg.endStructure();
    return arg;
}

namespace snapline {

namespace {

const QString kPortalService = QStringLiteral("org.freedesktop.portal.Desktop");
const QString kPortalPath = QStringLiteral("/org/freedesktop/portal/desktop");
const QString kShortcutsInterface = QStringLiteral("org.freedesktop.portal.GlobalShortcuts");

QString describe(const QString &id)
{
    if (id == QLatin1String("main"))
        return QObject::tr("Take a screenshot");
    if (id == QLatin1String("save_full"))
        return QObject::tr("Instantly save the full screen");
    if (id == QLatin1String("upload_full"))
        return QObject::tr("Instantly upload the full screen");
    return id;
}

class NullBackend : public HotkeyBackend {
public:
    NullBackend(const QString &reason, QObject *parent) : HotkeyBackend(parent), m_reason(reason) {}
    QString name() const override { return QStringLiteral("none"); }
    void setBindings(const QList<HotkeyBinding> &bindings) override
    {
        for (const HotkeyBinding &b : bindings) {
            if (b.enabled)
                emit registrationFailed(b.id, m_reason);
        }
    }

private:
    QString m_reason;
};

#ifdef SNAPLINE_HAVE_X11
bool g_grabFailed = false;

int onGrabError(Display *, XErrorEvent *event)
{
    if (event->error_code == BadAccess)
        g_grabFailed = true;
    return 0;
}

class X11Backend : public HotkeyBackend, public QAbstractNativeEventFilter {
public:
    explicit X11Backend(QObject *parent) : HotkeyBackend(parent) { qApp->installNativeEventFilter(this); }

    ~X11Backend() override
    {
        qApp->removeNativeEventFilter(this);
        ungrabAll();
    }

    QString name() const override { return QStringLiteral("x11"); }

    void setBindings(const QList<HotkeyBinding> &bindings) override
    {
        ungrabAll();
        Display *dpy = display();
        if (!dpy)
            return;
        for (const HotkeyBinding &b : bindings) {
            if (!b.enabled || b.sequence.count() != 1)
                continue;
            const QKeyCombination combo = b.sequence[0];
            const KeySym sym = XStringToKeysym(keysymName(combo.key()).toLatin1().constData());
            const KeyCode code = sym ? XKeysymToKeycode(dpy, sym) : 0;
            if (!code) {
                emit registrationFailed(b.id, tr("This key does not exist on the current keyboard layout."));
                continue;
            }
            unsigned mods = 0;
            const auto qmods = combo.keyboardModifiers();
            if (qmods & Qt::ShiftModifier) mods |= ShiftMask;
            if (qmods & Qt::ControlModifier) mods |= ControlMask;
            if (qmods & Qt::AltModifier) mods |= Mod1Mask;
            if (qmods & Qt::MetaModifier) mods |= Mod4Mask;

            g_grabFailed = false;
            XErrorHandler previous = XSetErrorHandler(onGrabError);
            for (unsigned extra : kIgnoredMasks)
                XGrabKey(dpy, code, mods | extra, DefaultRootWindow(dpy), True, GrabModeAsync, GrabModeAsync);
            XSync(dpy, False);
            XSetErrorHandler(previous);

            const Grab grab{b.id, code, mods};
            if (g_grabFailed) {
                ungrab(grab);
                emit registrationFailed(b.id, tr("Likely another application is already using it."));
                continue;
            }
            m_grabs.push_back(grab);
        }
    }

    bool nativeEventFilter(const QByteArray &type, void *message, qintptr *) override
    {
        if (type != "xcb_generic_event_t")
            return false;
        auto *event = static_cast<xcb_generic_event_t *>(message);
        if ((event->response_type & ~0x80) != XCB_KEY_PRESS)
            return false;
        auto *key = reinterpret_cast<xcb_key_press_event_t *>(event);
        const unsigned state = key->state & (ShiftMask | ControlMask | Mod1Mask | Mod4Mask);
        for (const Grab &g : m_grabs) {
            if (g.code == key->detail && g.mods == state) {
                const QString id = g.id;
                QMetaObject::invokeMethod(this, [this, id]() { emit activated(id); }, Qt::QueuedConnection);
                return true;
            }
        }
        return false;
    }

private:
    struct Grab {
        QString id;
        KeyCode code;
        unsigned mods;
    };
    static constexpr unsigned kIgnoredMasks[] = {0, LockMask, Mod2Mask, LockMask | Mod2Mask};

    static Display *display()
    {
        auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
        return x11 ? x11->display() : nullptr;
    }

    void ungrab(const Grab &g)
    {
        if (Display *dpy = display()) {
            for (unsigned extra : kIgnoredMasks)
                XUngrabKey(dpy, g.code, g.mods | extra, DefaultRootWindow(dpy));
        }
    }

    void ungrabAll()
    {
        for (const Grab &g : m_grabs)
            ungrab(g);
        if (Display *dpy = display())
            XFlush(dpy);
        m_grabs.clear();
    }

    QVector<Grab> m_grabs;
};
#endif

} // namespace

class PortalHotkeyBackend : public HotkeyBackend {
    Q_OBJECT

public:
    explicit PortalHotkeyBackend(QObject *parent) : HotkeyBackend(parent)
    {
        qDBusRegisterMetaType<PortalShortcut>();
        qDBusRegisterMetaType<PortalShortcutList>();
        registerHostApp();
        QDBusConnection::sessionBus().connect(
            kPortalService, kPortalPath, kShortcutsInterface, QStringLiteral("Activated"), this,
            SLOT(onActivated(QDBusObjectPath, QString, qulonglong, QVariantMap)));
    }

    ~PortalHotkeyBackend() override { closeSession(); }

    static bool isAvailable()
    {
        QDBusMessage get = QDBusMessage::createMethodCall(kPortalService, kPortalPath,
                                                          QStringLiteral("org.freedesktop.DBus.Properties"),
                                                          QStringLiteral("Get"));
        get << kShortcutsInterface << QStringLiteral("version");
        const QDBusMessage reply = QDBusConnection::sessionBus().call(get, QDBus::Block, 3000);
        return reply.type() == QDBusMessage::ReplyMessage;
    }

    QString name() const override { return QStringLiteral("portal"); }

    void setBindings(const QList<HotkeyBinding> &bindings) override
    {
        m_bindings.clear();
        for (const HotkeyBinding &b : bindings) {
            if (b.enabled)
                m_bindings << b;
        }
        closeSession();
        if (m_bindings.isEmpty())
            return;

        const QString token = QStringLiteral("snapline%1").arg(QRandomGenerator::global()->generate());
        auto *request = PortalRequest::call(kShortcutsInterface, QStringLiteral("CreateSession"),
                                            {QVariantMap{{QStringLiteral("session_handle_token"), token}}}, this);
        const int generation = ++m_generation;
        connect(request, &PortalRequest::error, this, [this, request](const QString &message) {
            request->deleteLater();
            failAll(message);
        });
        connect(request, &PortalRequest::finished, this,
                [this, request, generation](uint response, const QVariantMap &results) {
                    request->deleteLater();
                    if (generation != m_generation)
                        return;
                    m_session = results.value(QStringLiteral("session_handle")).toString();
                    if (response != 0 || m_session.isEmpty()) {
                        failAll(tr("The desktop refused to create a shortcut session."));
                        return;
                    }
                    bind();
                });
    }

private slots:
    void onActivated(const QDBusObjectPath &session, const QString &id, qulonglong timestamp, const QVariantMap &)
    {
        Q_UNUSED(timestamp)
        if (session.path() == m_session)
            emit activated(id);
    }

private:
    void registerHostApp()
    {
        // Lets the portal identify an unsandboxed app (xdg-desktop-portal >= 1.19). Qt 6.9+ does this
        // itself and only one registration per connection is accepted.
        if (QT_VERSION >= QT_VERSION_CHECK(6, 9, 0))
            return;
        QDBusMessage reg = QDBusMessage::createMethodCall(kPortalService, kPortalPath,
                                                          QStringLiteral("org.freedesktop.host.portal.Registry"),
                                                          QStringLiteral("Register"));
        reg << QGuiApplication::desktopFileName() << QVariantMap();
        QDBusConnection::sessionBus().call(reg, QDBus::Block, 2000);
    }

    void bind()
    {
        PortalShortcutList shortcuts;
        for (const HotkeyBinding &b : m_bindings) {
            shortcuts << PortalShortcut{b.id,
                                        {{QStringLiteral("description"), describe(b.id)},
                                         {QStringLiteral("preferred_trigger"), portalTrigger(b.sequence)}}};
        }
        auto *request = PortalRequest::call(
            kShortcutsInterface, QStringLiteral("BindShortcuts"),
            {QVariant::fromValue(QDBusObjectPath(m_session)), QVariant::fromValue(shortcuts), QString(), QVariantMap()},
            this);
        connect(request, &PortalRequest::error, this, [this, request](const QString &message) {
            request->deleteLater();
            failAll(message);
        });
        connect(request, &PortalRequest::finished, this, [this, request](uint response, const QVariantMap &) {
            request->deleteLater();
            if (response == 1)
                failAll(tr("The shortcuts were not confirmed in the desktop's dialog."));
            else if (response != 0)
                failAll(tr("The desktop did not accept the shortcuts."));
        });
    }

    void closeSession()
    {
        if (m_session.isEmpty())
            return;
        QDBusMessage close = QDBusMessage::createMethodCall(kPortalService, m_session,
                                                            QStringLiteral("org.freedesktop.portal.Session"),
                                                            QStringLiteral("Close"));
        QDBusConnection::sessionBus().asyncCall(close);
        m_session.clear();
    }

    void failAll(const QString &reason)
    {
        for (const HotkeyBinding &b : m_bindings)
            emit registrationFailed(b.id, reason);
    }

    QList<HotkeyBinding> m_bindings;
    QString m_session;
    int m_generation = 0;
};

HotkeyManager::HotkeyManager(QObject *parent) : QObject(parent)
{
    if (ScreenGrabber::isWayland()) {
        if (PortalHotkeyBackend::isAvailable())
            m_backend = std::make_unique<PortalHotkeyBackend>(nullptr);
        else
            m_backend = std::make_unique<NullBackend>(
                tr("This desktop does not offer global shortcuts to applications. Bind the command "
                   "\"snapline gui\" in your desktop's keyboard settings instead."),
                nullptr);
    } else {
#ifdef SNAPLINE_HAVE_X11
        m_backend = std::make_unique<X11Backend>(nullptr);
#else
        m_backend = std::make_unique<NullBackend>(tr("Built without X11 support."), nullptr);
#endif
    }
    connect(m_backend.get(), &HotkeyBackend::activated, this, &HotkeyManager::activated);
    connect(m_backend.get(), &HotkeyBackend::registrationFailed, this, &HotkeyManager::registrationFailed);
}

HotkeyManager::~HotkeyManager() = default;

void HotkeyManager::setBindings(const QList<HotkeyBinding> &bindings)
{
    m_backend->setBindings(bindings);
}

QString HotkeyManager::backendName() const
{
    return m_backend->name();
}

} // namespace snapline

#include "HotkeyManager.moc"
