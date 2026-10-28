#pragma once

#include <QObject>
#include <QString>
#include <memory>

enum class HotkeyAction {
    Fullscreen,
    Region,
    ColorPicker,
    Editor
};

class HotkeyManager : public QObject {
    Q_OBJECT

public:
    struct Impl;

    static HotkeyManager& instance();
    ~HotkeyManager();

    bool init();
    void updateHotkeys();

    void onPortalActivated(const QString& actionId);

signals:
    void hotkeyTriggered(HotkeyAction action);

private:
    HotkeyManager();

    void requestPortalShortcuts();
    bool initX11();
    void updateX11Hotkeys();

    std::unique_ptr<Impl> m_impl;
    bool m_initialized = false;
};
