#pragma once

#include <QString>
#include <QGuiApplication>

namespace Platform {

enum class Type {
    X11,
    Wayland
};

inline Type current() {
    if (qApp) {
        QString platform = QGuiApplication::platformName().toLower();
        if (platform.contains("wayland")) {
            return Type::Wayland;
        }
        if (platform.contains("xcb")) {
            return Type::X11;
        }
    }
    QString session = qEnvironmentVariable("XDG_SESSION_TYPE").toLower();
    if (session == "wayland" || !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
        return Type::Wayland;
    }
    return Type::X11;
}

inline bool isWayland() {
    return current() == Type::Wayland;
}

inline bool isX11() {
    return current() == Type::X11;
}

inline QString name() {
    return isWayland() ? QStringLiteral("Wayland") : QStringLiteral("X11");
}

} // namespace Platform
