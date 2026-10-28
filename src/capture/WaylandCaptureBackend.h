#pragma once

#include <QObject>
#include <QPixmap>
#include <QColor>
#include <QString>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>

class WaylandCaptureBackend : public QObject {
    Q_OBJECT

public:
    explicit WaylandCaptureBackend(QObject* parent = nullptr);
    ~WaylandCaptureBackend() override = default;

    void captureFullscreen();
    void captureRegion();
    void pickColor();

    void handlePortalResponse(uint response, const QVariantMap& results, bool isColorPicker);

signals:
    void screenshotReady(const QPixmap& pixmap);
    void colorPicked(const QColor& color, const QString& hex);
    void captureFailed(const QString& reason);
    void captureCancelled();

private slots:
    void onPortalResponse(uint response, const QVariantMap& results);
    void onPortalColorResponse(uint response, const QVariantMap& results);

private:
    void requestPortalScreenshot(bool interactive);
    void performDirectCapture();

    int m_requestCounter = 0;
};
