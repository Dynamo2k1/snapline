#include "upload/UploadDialog.h"

#include "app/Clipboard.h"
#include "upload/Uploader.h"

#include <QBoxLayout>
#include <QDesktopServices>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>

namespace snapline {

UploadDialog::UploadDialog(Uploader *uploader, const QByteArray &data, const QString &mimeType,
                           const QString &fileName, bool autoClose, QWidget *parent)
    : QDialog(parent), m_uploader(uploader), m_data(data), m_mimeType(mimeType), m_fileName(fileName),
      m_autoClose(autoClose)
{
    setWindowTitle(tr("Uploading image"));
    setAttribute(Qt::WA_DeleteOnClose);
    setMinimumWidth(420);
    uploader->setParent(this);

    auto *layout = new QVBoxLayout(this);
    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_progress = new QProgressBar(this);
    m_link = new QLineEdit(this);
    m_link->setReadOnly(true);
    m_details = new QPlainTextEdit(this);
    m_details->setReadOnly(true);
    m_details->setMinimumHeight(140);

    auto *buttons = new QHBoxLayout;
    m_detailsToggle = new QPushButton(tr("Detailed information"), this);
    m_detailsToggle->setCheckable(true);
    m_proxy = new QPushButton(tr("Proxy settings…"), this);
    m_retry = new QPushButton(tr("Retry"), this);
    m_copy = new QPushButton(tr("Copy link"), this);
    m_open = new QPushButton(tr("Open"), this);
    auto *closeButton = new QPushButton(tr("Close"), this);
    buttons->addWidget(m_detailsToggle);
    buttons->addWidget(m_proxy);
    buttons->addStretch();
    buttons->addWidget(m_retry);
    buttons->addWidget(m_copy);
    buttons->addWidget(m_open);
    buttons->addWidget(closeButton);

    layout->addWidget(m_status);
    layout->addWidget(m_progress);
    layout->addWidget(m_link);
    layout->addWidget(m_details);
    layout->addLayout(buttons);

    connect(m_detailsToggle, &QPushButton::toggled, m_details, &QWidget::setVisible);
    connect(m_proxy, &QPushButton::clicked, this, [this]() {
        emit proxySettingsRequested();
        close();
    });
    connect(m_retry, &QPushButton::clicked, this, &UploadDialog::start);
    connect(m_copy, &QPushButton::clicked, this, [this]() { Clipboard::setText(m_url.toString()); });
    connect(m_open, &QPushButton::clicked, this, [this]() { QDesktopServices::openUrl(m_url); });
    connect(closeButton, &QPushButton::clicked, this, &QDialog::close);
    connect(this, &QDialog::finished, m_uploader, &Uploader::abort);

    connect(uploader, &Uploader::progress, this, [this](qint64 sent, qint64 total) {
        if (total > 0) {
            m_progress->setRange(0, 1000);
            m_progress->setValue(int(sent * 1000 / total));
        }
    });
    connect(uploader, &Uploader::succeeded, this, &UploadDialog::onSucceeded);
    connect(uploader, &Uploader::failed, this, &UploadDialog::onFailed);
}

void UploadDialog::start()
{
    m_status->setText(tr("Uploading to %1…").arg(m_uploader->name()));
    m_progress->setRange(0, 0);
    m_progress->show();
    for (QWidget *w : {static_cast<QWidget *>(m_link), static_cast<QWidget *>(m_details), static_cast<QWidget *>(m_copy),
                       static_cast<QWidget *>(m_open), static_cast<QWidget *>(m_retry), static_cast<QWidget *>(m_proxy),
                       static_cast<QWidget *>(m_detailsToggle)})
        w->hide();
    m_detailsToggle->setChecked(false);
    adjustSize();
    m_uploader->upload(m_data, m_mimeType, m_fileName);
}

void UploadDialog::onSucceeded(const QUrl &url)
{
    m_url = url;
    emit uploaded(url);
    if (m_autoClose) {
        close();
        return;
    }
    m_status->setText(tr("Screenshot uploaded."));
    m_progress->hide();
    m_link->setText(url.toString());
    m_link->show();
    m_link->selectAll();
    m_copy->show();
    m_open->show();
    adjustSize();
}

void UploadDialog::onFailed(const QString &message, const QString &details)
{
    m_status->setText(tr("Upload failed. Retry?") + QStringLiteral("\n") + message);
    m_progress->hide();
    m_details->setPlainText(details);
    m_detailsToggle->show();
    m_retry->show();
    m_proxy->show();
    m_retry->setDefault(true);
    adjustSize();
    show();
    raise();
    activateWindow();
}

} // namespace snapline
