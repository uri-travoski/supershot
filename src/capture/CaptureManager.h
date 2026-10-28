#pragma once

#include <QObject>
#include <QPixmap>
#include <QColor>
#include <QTimer>

class RegionSnippingOverlay;
class ColorPickerOverlay;
class WaylandCaptureBackend;

class CaptureManager : public QObject {
    Q_OBJECT

public:
    static CaptureManager& instance();

    void captureFullscreen();
    void captureRegion();
    void pickColor();

    WaylandCaptureBackend* waylandBackend();

signals:
    void screenshotReady(const QPixmap& pixmap);
    void colorPicked(const QColor& color, const QString& hex);
    void captureCancelled();
    void captureFailed(const QString& reason);

private slots:
    void doCaptureFullscreen();
    void onRegionCaptured(const QPixmap& pixmap);
    void onColorPicked(const QColor& color, const QString& hex);

private:
    CaptureManager();

    RegionSnippingOverlay* regionOverlay();
    ColorPickerOverlay* colorOverlay();

    RegionSnippingOverlay* m_regionOverlay = nullptr;
    ColorPickerOverlay* m_colorOverlay = nullptr;
    WaylandCaptureBackend* m_waylandBackend = nullptr;
};
