#include "HotkeyManager.h"
#include "SettingsManager.h"
#include "PlatformHelper.h"

#include <QSocketNotifier>
#include <QList>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDebug>

// Include X11 headers after Qt headers and undefine conflicting macros
#include <X11/Xlib.h>
#include <X11/keysym.h>

#ifdef None
#undef None
#endif
#ifdef Status
#undef Status
#endif
#ifdef Bool
#undef Bool
#endif
#ifdef Cursor
#undef Cursor
#endif

struct GrabbedKey {
    int keycode;
    unsigned int modifiers;
    HotkeyAction action;
};

struct HotkeyManager::Impl {
    Display* display = nullptr;
    Window root = 0;
    QSocketNotifier* notifier = nullptr;
    QList<GrabbedKey> grabbedKeys;
};

HotkeyManager& HotkeyManager::instance() {
    static HotkeyManager s_instance;
    return s_instance;
}

HotkeyManager::HotkeyManager()
    : m_impl(std::make_unique<Impl>())
{
}

HotkeyManager::~HotkeyManager() {
    if (m_impl->display) {
        if (m_impl->root) {
            for (const auto& grab : m_impl->grabbedKeys) {
                const unsigned int masks[] = {0, Mod2Mask, LockMask, Mod2Mask | LockMask};
                for (unsigned int m : masks) {
                    XUngrabKey(m_impl->display, grab.keycode, grab.modifiers | m, m_impl->root);
                }
            }
            m_impl->grabbedKeys.clear();
            XFlush(m_impl->display);
        }
        XCloseDisplay(m_impl->display);
        m_impl->display = nullptr;
    }
}

static KeySym stringToKeySym(const QString& keyStr) {
    if (keyStr.compare("Print", Qt::CaseInsensitive) == 0 ||
        keyStr.compare("PrintScreen", Qt::CaseInsensitive) == 0 ||
        keyStr.compare("PrntScrn", Qt::CaseInsensitive) == 0) {
        return XK_Print;
    }
    if (keyStr.length() == 1) {
        char c = keyStr.toLatin1().at(0);
        if (c >= 'a' && c <= 'z') return XK_a + (c - 'a');
        if (c >= 'A' && c <= 'Z') return XK_A + (c - 'A');
        if (c >= '0' && c <= '9') return XK_0 + (c - '0');
    }
    if (keyStr.startsWith("F", Qt::CaseInsensitive) && keyStr.length() > 1) {
        int fNum = keyStr.mid(1).toInt();
        if (fNum >= 1 && fNum <= 12) return XK_F1 + (fNum - 1);
    }
    return XStringToKeysym(keyStr.toUtf8().constData());
}

static void grabSingleShortcut(HotkeyManager::Impl* impl, HotkeyAction action, const QString& sequenceStr) {
    if (!impl->display || sequenceStr.trimmed().isEmpty()) return;

    QStringList parts = sequenceStr.split("+", Qt::SkipEmptyParts);
    if (parts.isEmpty()) return;

    unsigned int xMods = 0;
    QString keyPart;

    for (const QString& raw : parts) {
        QString part = raw.trimmed();
        if (part.compare("Ctrl", Qt::CaseInsensitive) == 0 || part.compare("Control", Qt::CaseInsensitive) == 0) {
            xMods |= ControlMask;
        } else if (part.compare("Shift", Qt::CaseInsensitive) == 0) {
            xMods |= ShiftMask;
        } else if (part.compare("Alt", Qt::CaseInsensitive) == 0) {
            xMods |= Mod1Mask;
        } else if (part.compare("Meta", Qt::CaseInsensitive) == 0 || part.compare("Super", Qt::CaseInsensitive) == 0) {
            xMods |= Mod4Mask;
        } else {
            keyPart = part;
        }
    }

    if (keyPart.isEmpty()) return;

    KeySym sym = stringToKeySym(keyPart);
    if (sym == NoSymbol) return;

    KeyCode code = XKeysymToKeycode(impl->display, sym);
    if (code == 0) return;

    const unsigned int masks[] = {0, Mod2Mask, LockMask, Mod2Mask | LockMask};
    for (unsigned int m : masks) {
        XGrabKey(impl->display, code, xMods | m, impl->root, True, GrabModeAsync, GrabModeAsync);
    }

    impl->grabbedKeys.append({code, xMods, action});
    XFlush(impl->display);
}

bool HotkeyManager::init() {
    if (m_initialized) return true;
    m_initialized = true;

    if (Platform::isWayland()) {
        requestPortalShortcuts();
        return true;
    }

    return initX11();
}

bool HotkeyManager::initX11() {
    m_impl->display = XOpenDisplay(nullptr);
    if (!m_impl->display) {
        qWarning() << "HotkeyManager: Could not open X11 display (running non-X11 environment)";
        return false;
    }

    m_impl->root = DefaultRootWindow(m_impl->display);
    int fd = ConnectionNumber(m_impl->display);
    m_impl->notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);

    connect(m_impl->notifier, &QSocketNotifier::activated, this, [this]() {
        if (!m_impl->display) return;
        while (XPending(m_impl->display)) {
            XEvent ev;
            XNextEvent(m_impl->display, &ev);
            if (ev.type == KeyPress) {
                unsigned int cleanMods = ev.xkey.state & ~(Mod2Mask | LockMask);
                for (const auto& grab : m_impl->grabbedKeys) {
                    if (grab.keycode == static_cast<int>(ev.xkey.keycode) && grab.modifiers == cleanMods) {
                        emit hotkeyTriggered(grab.action);
                        break;
                    }
                }
            }
        }
    });

    updateX11Hotkeys();
    return true;
}

void HotkeyManager::updateHotkeys() {
    if (Platform::isWayland()) {
        requestPortalShortcuts();
    } else {
        updateX11Hotkeys();
    }
}

void HotkeyManager::updateX11Hotkeys() {
    if (!m_impl->display || !m_impl->root) return;

    for (const auto& grab : m_impl->grabbedKeys) {
        const unsigned int masks[] = {0, Mod2Mask, LockMask, Mod2Mask | LockMask};
        for (unsigned int m : masks) {
            XUngrabKey(m_impl->display, grab.keycode, grab.modifiers | m, m_impl->root);
        }
    }
    m_impl->grabbedKeys.clear();
    XFlush(m_impl->display);

    const auto& s = SettingsManager::instance();
    grabSingleShortcut(m_impl.get(), HotkeyAction::Fullscreen, s.hotkeyFullscreen());
    grabSingleShortcut(m_impl.get(), HotkeyAction::Region, s.hotkeyRegion());
    grabSingleShortcut(m_impl.get(), HotkeyAction::ColorPicker, s.hotkeyColorPicker());
    grabSingleShortcut(m_impl.get(), HotkeyAction::Editor, s.hotkeyEditor());
}

void HotkeyManager::requestPortalShortcuts() {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) return;

    QDBusMessage msg = QDBusMessage::createMethodCall(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.GlobalShortcuts",
        "CreateSession"
    );

    QVariantMap options;
    options["handle_token"] = "sshot_gs_session";
    options["session_handle_token"] = "sshot_gs_token";
    msg << options;

    bus.asyncCall(msg);
}

void HotkeyManager::onPortalActivated(const QString& actionId) {
    if (actionId == "fullscreen") {
        emit hotkeyTriggered(HotkeyAction::Fullscreen);
    } else if (actionId == "region") {
        emit hotkeyTriggered(HotkeyAction::Region);
    } else if (actionId == "colorpicker") {
        emit hotkeyTriggered(HotkeyAction::ColorPicker);
    } else if (actionId == "editor") {
        emit hotkeyTriggered(HotkeyAction::Editor);
    }
}
