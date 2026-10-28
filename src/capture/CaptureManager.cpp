#include "CaptureManager.h"
#include "RegionSnippingOverlay.h"
#include "ColorPickerOverlay.h"
#include "WaylandCaptureBackend.h"
#include "../core/PlatformHelper.h"
#include "../core/SettingsManager.h"
#include "../core/ClipboardHelper.h"
#include <QGuiApplication>
#include <QScreen>
#include <QDebug>
#include <malloc.h>

CaptureManager& CaptureManager::instance() {
    static CaptureManager s_instance;
    return s_instance;
}

CaptureManager::CaptureManager() {
}

WaylandCaptureBackend* CaptureManager::waylandBackend() {
    if (!m_waylandBackend) {
        m_waylandBackend = new WaylandCaptureBackend(this);
        connect(m_waylandBackend, &WaylandCaptureBackend::screenshotReady, this, &CaptureManager::onRegionCaptured);
        connect(m_waylandBackend, &WaylandCaptureBackend::colorPicked, this, &CaptureManager::onColorPicked);
        connect(m_waylandBackend, &WaylandCaptureBackend::captureCancelled, this, &CaptureManager::captureCancelled);
        connect(m_waylandBackend, &WaylandCaptureBackend::captureFailed, this, &CaptureManager::captureFailed);
    }
    return m_waylandBackend;
}

RegionSnippingOverlay* CaptureManager::regionOverlay() {
    if (!m_regionOverlay) {
        m_regionOverlay = new RegionSnippingOverlay();
        connect(m_regionOverlay, &RegionSnippingOverlay::regionCaptured, this, &CaptureManager::onRegionCaptured);
        connect(m_regionOverlay, &RegionSnippingOverlay::snippingCancelled, this, &CaptureManager::captureCancelled);
    }
    return m_regionOverlay;
}

ColorPickerOverlay* CaptureManager::colorOverlay() {
    if (!m_colorOverlay) {
        m_colorOverlay = new ColorPickerOverlay();
        connect(m_colorOverlay, &ColorPickerOverlay::colorPicked, this, &CaptureManager::onColorPicked);
        connect(m_colorOverlay, &ColorPickerOverlay::pickingCancelled, this, &CaptureManager::captureCancelled);
    }
    return m_colorOverlay;
}

void CaptureManager::captureFullscreen() {
    int delay = SettingsManager::instance().captureDelay();
    auto runCapture = [this]() {
        if (Platform::isWayland()) {
            waylandBackend()->captureFullscreen();
        } else {
            doCaptureFullscreen();
        }
    };

    if (delay > 0) {
        QTimer::singleShot(delay * 1000, this, runCapture);
    } else {
        runCapture();
    }
}

void CaptureManager::doCaptureFullscreen() {
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) {
        emit captureFailed(tr("No primary screen detected"));
        return;
    }
    QRect virtualGeometry = screen->virtualGeometry();
    QPixmap grab = screen->grabWindow(0, virtualGeometry.x(), virtualGeometry.y(), virtualGeometry.width(), virtualGeometry.height());
    grab.setDevicePixelRatio(1.0);

    if (SettingsManager::instance().autoCopyToClipboard()) {
        ClipboardHelper::copyImage(grab);
    }

    emit screenshotReady(grab);
}

void CaptureManager::captureRegion() {
    int delay = SettingsManager::instance().captureDelay();
    auto runCapture = [this]() {
        if (Platform::isWayland()) {
            waylandBackend()->captureRegion();
        } else {
            regionOverlay()->startSnipping();
        }
    };

    if (delay > 0) {
        QTimer::singleShot(delay * 1000, this, runCapture);
    } else {
        runCapture();
    }
}

void CaptureManager::pickColor() {
    if (Platform::isWayland()) {
        waylandBackend()->pickColor();
    } else {
        colorOverlay()->startPicking();
    }
}

void CaptureManager::onRegionCaptured(const QPixmap& pixmap) {
    QPixmap normPix = pixmap;
    normPix.setDevicePixelRatio(1.0);
    if (SettingsManager::instance().autoCopyToClipboard()) {
        ClipboardHelper::copyImage(normPix);
    }

    emit screenshotReady(normPix);
}

void CaptureManager::onColorPicked(const QColor& color, const QString& hex) {
    emit colorPicked(color, hex);
}
