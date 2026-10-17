#pragma once

#include <QDialog>
#include <QUrl>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;

namespace snapline {

class Uploader;

// "Uploading image" window: progress, then the link or an error with retry.
class UploadDialog : public QDialog {
    Q_OBJECT

public:
    UploadDialog(Uploader *uploader, const QByteArray &imageData, const QString &mimeType, const QString &fileName,
                 bool autoClose, QWidget *parent = nullptr);
    void start();

signals:
    void uploaded(const QUrl &url);
    void proxySettingsRequested();

private:
    void onSucceeded(const QUrl &url);
    void onFailed(const QString &message, const QString &details);

    Uploader *m_uploader;
    QByteArray m_data;
    QString m_mimeType;
    QString m_fileName;
    bool m_autoClose;
    QUrl m_url;

    QLabel *m_status;
    QProgressBar *m_progress;
    QLineEdit *m_link;
    QPlainTextEdit *m_details;
    QPushButton *m_copy;
    QPushButton *m_open;
    QPushButton *m_retry;
    QPushButton *m_proxy;
    QPushButton *m_detailsToggle;
};

} // namespace snapline
