#include "AboutDialog.h"
#include "../core/IconManager.h"
#include "../core/UpdateManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QFile>
#include <QTextStream>
#include <QDialogButtonBox>
#include <QCoreApplication>
#include <QFrame>
#include <unistd.h>

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("About SuperShot"));
    resize(440, 420);
    setStyleSheet("QDialog { background-color: #242424; color: #ffffff; }");

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(14);
    layout->setContentsMargins(24, 24, 24, 24);

    QHBoxLayout* headerLayout = new QHBoxLayout();
    QLabel* iconLabel = new QLabel(this);
    iconLabel->setPixmap(IconManager::getAppIcon().pixmap(64, 64));
    headerLayout->addWidget(iconLabel);

    QVBoxLayout* titleLayout = new QVBoxLayout();
    QLabel* nameLabel = new QLabel("SuperShot", this);
    nameLabel->setStyleSheet("font-size: 22px; font-weight: bold; color: #30e500;");
    QString appVer = QCoreApplication::applicationVersion();
    if (appVer.isEmpty()) appVer = "1.35.2";
    QLabel* verLabel = new QLabel(tr("Version %1 (Linux x86_64)").arg(appVer), this);
    verLabel->setStyleSheet("font-size: 12px; color: #aaaaaa;");
    titleLayout->addWidget(nameLabel);
    titleLayout->addWidget(verLabel);
    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();
    layout->addLayout(headerLayout);

    QLabel* descLabel = new QLabel(
        tr("<b>SuperShot</b> is a blazing fast, ultra-lightweight screenshot capture "
           "and annotation tool for Linux with ksnip-inspired workflows.<br><br>"
           "• Multi-tab image & annotation editing<br>"
           "• Full annotation tools (Pen, Arrow, Shapes, Text, Badges, Blur)<br>"
           "• Select tool with Copy, Cut, Delete, and Crop raster actions<br>"
           "• Fullscreen, Region, & Color Picker<br>"
           "• Global shortcuts & System Tray integration"), this);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("font-size: 12px; line-height: 1.4; color: #dddddd;");
    layout->addWidget(descLabel);

    // Live Memory Footprint
    QLabel* ramLabel = new QLabel(QString(tr("⚡ <b>RAM Footprint:</b> %1")).arg(getMemoryUsageString()), this);
    ramLabel->setStyleSheet("background-color: #1a1a1a; border: 1px solid #333333; border-radius: 4px; padding: 6px; color: #30e500;");
    layout->addWidget(ramLabel);

    // Software Updates Card
    QFrame* updateCard = new QFrame(this);
    updateCard->setStyleSheet(
        "QFrame { background-color: #1e1e1e; border: 1px solid #333333; border-radius: 6px; }"
    );
    QHBoxLayout* updateLayout = new QHBoxLayout(updateCard);
    updateLayout->setContentsMargins(12, 10, 12, 10);
    updateLayout->setSpacing(10);

    QVBoxLayout* statusTextLayout = new QVBoxLayout();
    statusTextLayout->setSpacing(2);
    QLabel* updateHeader = new QLabel(tr("<b>Software Updates</b>"), updateCard);
    updateHeader->setStyleSheet("font-size: 12px; color: #ffffff; border: none; background: transparent;");
    statusTextLayout->addWidget(updateHeader);

    m_updateStatusLabel = new QLabel(updateCard);
    m_updateStatusLabel->setWordWrap(true);
    m_updateStatusLabel->setStyleSheet("font-size: 11px; color: #aaaaaa; border: none; background: transparent;");
    statusTextLayout->addWidget(m_updateStatusLabel);
    updateLayout->addLayout(statusTextLayout);

    updateLayout->addStretch();

    m_checkUpdatesBtn = new QPushButton(tr("Check for Updates"), updateCard);
    m_checkUpdatesBtn->setCursor(Qt::PointingHandCursor);
    m_checkUpdatesBtn->setStyleSheet(
        "QPushButton { background-color: #2b5b84; color: #ffffff; border: none; border-radius: 4px; padding: 6px 14px; font-weight: bold; font-size: 12px; }"
        "QPushButton:hover { background-color: #3572a5; }"
        "QPushButton:pressed { background-color: #1e4468; }"
        "QPushButton:disabled { background-color: #333333; color: #777777; }"
    );
    updateLayout->addWidget(m_checkUpdatesBtn);
    layout->addWidget(updateCard);

    // Initialize update status display
    UpdateManager& um = UpdateManager::instance();
    if (um.isChecking()) {
        m_updateStatusLabel->setText(tr("Checking GitHub for updates..."));
        m_updateStatusLabel->setStyleSheet("font-size: 11px; color: #aaaaaa; border: none; background: transparent;");
        m_checkUpdatesBtn->setEnabled(false);
    } else if (!um.latestVersion().isEmpty()) {
        if (UpdateManager::isVersionNewer(um.latestVersion(), um.currentVersion())) {
            m_updateStatusLabel->setText(tr("✨ Update available: %1").arg(um.latestVersion()));
            m_updateStatusLabel->setStyleSheet("font-size: 11px; color: #30e500; font-weight: bold; border: none; background: transparent;");
        } else {
            m_updateStatusLabel->setText(tr("✓ SuperShot is up to date (v%1)").arg(um.currentVersion()));
            m_updateStatusLabel->setStyleSheet("font-size: 11px; color: #888888; border: none; background: transparent;");
        }
    } else {
        m_updateStatusLabel->setText(tr("Current version: v%1").arg(appVer));
    }

    connect(m_checkUpdatesBtn, &QPushButton::clicked, this, &AboutDialog::onCheckUpdatesClicked);
    connect(&um, &UpdateManager::checkStarted, this, &AboutDialog::onCheckStarted);
    connect(&um, &UpdateManager::checkFinished, this, &AboutDialog::onCheckFinished);
    connect(&um, &UpdateManager::checkError, this, &AboutDialog::onCheckError);

    layout->addStretch();

    QDialogButtonBox* bbox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    bbox->setStyleSheet("QPushButton { background-color: #383838; color: #fff; padding: 6px 14px; border-radius: 4px; }");
    connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::accept);
    layout->addWidget(bbox);
}

void AboutDialog::onCheckUpdatesClicked() {
    UpdateManager::instance().checkForUpdates(false, this);
}

void AboutDialog::onCheckStarted() {
    m_updateStatusLabel->setText(tr("Checking GitHub for updates..."));
    m_updateStatusLabel->setStyleSheet("font-size: 11px; color: #aaaaaa; border: none; background: transparent;");
    m_checkUpdatesBtn->setEnabled(false);
}

void AboutDialog::onCheckFinished(bool updateAvailable, const QString& latestVersion, const QString& releaseUrl) {
    Q_UNUSED(releaseUrl);
    m_checkUpdatesBtn->setEnabled(true);
    if (updateAvailable) {
        m_updateStatusLabel->setText(tr("✨ Update available: %1").arg(latestVersion));
        m_updateStatusLabel->setStyleSheet("font-size: 11px; color: #30e500; font-weight: bold; border: none; background: transparent;");
    } else {
        m_updateStatusLabel->setText(tr("✓ SuperShot is up to date (v%1)").arg(UpdateManager::instance().currentVersion()));
        m_updateStatusLabel->setStyleSheet("font-size: 11px; color: #888888; border: none; background: transparent;");
    }
}

void AboutDialog::onCheckError(const QString& errorMessage) {
    Q_UNUSED(errorMessage);
    m_checkUpdatesBtn->setEnabled(true);
    m_updateStatusLabel->setText(tr("⚠ Update check failed"));
    m_updateStatusLabel->setStyleSheet("font-size: 11px; color: #ff5555; border: none; background: transparent;");
}

QString AboutDialog::getMemoryUsageString() {
    QFile file("/proc/self/statm");
    if (file.open(QIODevice::ReadOnly)) {
        QTextStream stream(&file);
        long size, resident, share, text, lib, data, dt;
        stream >> size >> resident >> share >> text >> lib >> data >> dt;
        long pageSizeKb = sysconf(_SC_PAGESIZE) / 1024;
        double rssMb = (resident * pageSizeKb) / 1024.0;
        return QString("%1 MB (Target < 60 MB)").arg(QString::number(rssMb, 'f', 1));
    }
    return "< 25 MB";
}
