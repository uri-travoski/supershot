#include "UpdateManager.h"
#include <QCoreApplication>
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPushButton>
#include <QDesktopServices>
#include <vector>

UpdateManager& UpdateManager::instance() {
    static UpdateManager s_instance;
    return s_instance;
}

UpdateManager::UpdateManager(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

QString UpdateManager::currentVersion() const {
    QString ver = QCoreApplication::applicationVersion();
    if (ver.isEmpty()) {
        ver = "1.39.0";
    }
    return ver;
}

bool UpdateManager::isVersionNewer(const QString& remoteVersion, const QString& currentVersion) {
    auto parse = [](QString v) -> std::vector<int> {
        v = v.trimmed();
        if (v.startsWith('v', Qt::CaseInsensitive)) {
            v.remove(0, 1);
        }
        QStringList parts = v.split('.', Qt::SkipEmptyParts);
        std::vector<int> nums;
        for (const QString& part : parts) {
            QString digits;
            for (QChar c : part) {
                if (c.isDigit()) digits.append(c);
                else break;
            }
            nums.push_back(digits.isEmpty() ? 0 : digits.toInt());
        }
        while (nums.size() < 3) {
            nums.push_back(0);
        }
        return nums;
    };

    std::vector<int> r = parse(remoteVersion);
    std::vector<int> c = parse(currentVersion);

    for (size_t i = 0; i < std::max(r.size(), c.size()); ++i) {
        int rv = (i < r.size()) ? r[i] : 0;
        int cv = (i < c.size()) ? c[i] : 0;
        if (rv > cv) return true;
        if (rv < cv) return false;
    }
    return false;
}

void UpdateManager::checkForUpdates(bool silentIfUpToDate, QWidget* parent) {
    if (m_isChecking) return;
    m_isChecking = true;
    m_silentIfUpToDate = silentIfUpToDate;
    m_parentWidget = parent;

    emit checkStarted();

    QUrl url("https://api.github.com/repos/uri-travoski/supershot/releases/latest");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "SuperShot/" + currentVersion());
    req.setRawHeader("Accept", "application/vnd.github+json");

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onReplyFinished(reply);
    });
}

void UpdateManager::onReplyFinished(QNetworkReply* reply) {
    reply->deleteLater();
    m_isChecking = false;

    QWidget* parent = m_parentWidget.data();
    if (!parent) {
        parent = QApplication::activeWindow();
    }

    static const char* kDarkMessageBoxStyle =
        "QMessageBox { background-color: #242424; color: #ffffff; }"
        "QLabel { color: #ffffff; background: transparent; font-size: 13px; }"
        "QLabel#qt_msgbox_label { color: #ffffff; font-size: 14px; font-weight: bold; }"
        "QLabel#qt_msgbox_informativelabel { color: #ffffff; font-size: 12px; }"
        "QTextEdit { background-color: #1a1a1a; color: #ffffff; border: 1px solid #444444; border-radius: 4px; padding: 4px; font-size: 12px; }"
        "QPushButton { background-color: #383838; color: #ffffff; border: 1px solid #555555; border-radius: 4px; padding: 6px 16px; min-width: 80px; font-size: 12px; }"
        "QPushButton:hover { background-color: #484848; border-color: #777777; }"
        "QPushButton:pressed { background-color: #222222; border-color: #30e500; }"
        "QPushButton:default { background-color: #2e7d32; border: 1px solid #4caf50; color: #ffffff; font-weight: bold; }"
        "QPushButton:default:hover { background-color: #388e3c; }";

    if (reply->error() != QNetworkReply::NoError) {
        QString err = reply->errorString();
        emit checkError(err);
        if (!m_silentIfUpToDate) {
            QMessageBox box(parent);
            box.setWindowTitle(tr("Update Check Failed"));
            box.setIcon(QMessageBox::Warning);
            box.setText(tr("Could not check for updates"));
            box.setInformativeText(err);
            box.setStyleSheet(kDarkMessageBoxStyle);
            box.addButton(QMessageBox::Ok);
            box.exec();
        }
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        QString err = tr("Invalid response received from GitHub.");
        emit checkError(err);
        if (!m_silentIfUpToDate) {
            QMessageBox box(parent);
            box.setWindowTitle(tr("Update Check Failed"));
            box.setIcon(QMessageBox::Warning);
            box.setText(tr("Could not check for updates"));
            box.setInformativeText(err);
            box.setStyleSheet(kDarkMessageBoxStyle);
            box.addButton(QMessageBox::Ok);
            box.exec();
        }
        return;
    }

    QJsonObject obj = doc.object();
    QString tagName = obj.value("tag_name").toString();
    QString htmlUrl = obj.value("html_url").toString();
    if (htmlUrl.isEmpty()) {
        htmlUrl = "https://github.com/uri-travoski/supershot/releases";
    }

    m_latestVersion = tagName;
    m_releaseUrl = htmlUrl;

    bool newer = isVersionNewer(tagName, currentVersion());
    emit checkFinished(newer, tagName, htmlUrl);

    if (newer) {
        QMessageBox box(parent);
        box.setWindowTitle(tr("Update Available - SuperShot"));
        box.setIcon(QMessageBox::Information);
        box.setText(tr("A new version of SuperShot is available!"));
        box.setInformativeText(
            tr("Current version: %1\nLatest version: %2\n\nWould you like to open the GitHub release page to update?")
            .arg(currentVersion(), tagName)
        );
        QString body = obj.value("body").toString().trimmed();
        if (!body.isEmpty()) {
            box.setDetailedText(body);
        }
        box.setStyleSheet(kDarkMessageBoxStyle);
        QPushButton* updateBtn = box.addButton(tr("Yes, Update"), QMessageBox::AcceptRole);
        QPushButton* laterBtn = box.addButton(tr("No, Later"), QMessageBox::RejectRole);
        box.setDefaultButton(updateBtn);
        box.exec();

        if (box.clickedButton() == updateBtn) {
            QDesktopServices::openUrl(QUrl(htmlUrl));
        }
    } else {
        if (!m_silentIfUpToDate) {
            QMessageBox box(parent);
            box.setWindowTitle(tr("SuperShot is Up to Date"));
            box.setIcon(QMessageBox::Information);
            box.setText(tr("You are running the latest version of SuperShot (v%1).").arg(currentVersion()));
            box.setStyleSheet(kDarkMessageBoxStyle);
            box.addButton(QMessageBox::Ok);
            box.exec();
        }
    }
}
