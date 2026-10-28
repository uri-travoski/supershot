#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>

class AboutDialog : public QDialog {
    Q_OBJECT

public:
    explicit AboutDialog(QWidget* parent = nullptr);

    // Getters for update controls
    QPushButton* checkUpdatesButton() const { return m_checkUpdatesBtn; }
    QLabel* updateStatusLabel() const { return m_updateStatusLabel; }

private slots:
    void onCheckUpdatesClicked();
    void onCheckStarted();
    void onCheckFinished(bool updateAvailable, const QString& latestVersion, const QString& releaseUrl);
    void onCheckError(const QString& errorMessage);

private:
    QString getMemoryUsageString();

    QPushButton* m_checkUpdatesBtn = nullptr;
    QLabel* m_updateStatusLabel = nullptr;
};
