#include "app/SettingsDialog.h"

#include "app/GnomeShortcut.h"
#include "app/ImageExport.h"
#include "capture/ScreenGrabber.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QUrl>

namespace snapline {

namespace {

void selectData(QComboBox *combo, const QString &value)
{
    const int index = combo->findData(value);
    combo->setCurrentIndex(index >= 0 ? index : 0);
}

QString hotkeyLabel(const QString &id)
{
    if (id == HotkeyId::Main)
        return QObject::tr("General hotkey");
    if (id == HotkeyId::SaveFull)
        return QObject::tr("Instant save of the full screen");
    return QObject::tr("Instant upload of the full screen");
}

} // namespace

SettingsDialog::SettingsDialog(const Config &config, QWidget *parent) : QDialog(parent), m_config(config)
{
    setWindowTitle(tr("Snapline Settings"));
    setAttribute(Qt::WA_DeleteOnClose);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildGeneralPage(), tr("General"));
    m_tabs->addTab(buildHotkeysPage(), tr("Hotkeys"));
    m_tabs->addTab(buildFormatsPage(), tr("Formats"));
    m_tabs->addTab(buildUploadPage(), tr("Upload"));
    m_proxyPage = buildProxyPage();
    m_tabs->addTab(m_proxyPage, tr("Proxy"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_tabs);
    layout->addWidget(buttons);
    resize(560, 460);
}

void SettingsDialog::showProxyPage()
{
    m_tabs->setCurrentWidget(m_proxyPage);
}

QWidget *SettingsDialog::buildGeneralPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    m_autoCopy = new QCheckBox(tr("Automatically copy the link after uploading"), page);
    m_autoClose = new QCheckBox(tr("Automatically close the upload window"), page);
    m_notifications = new QCheckBox(tr("Show notifications about copying and saving"), page);
    m_cursor = new QCheckBox(tr("Capture the mouse cursor"), page);
    m_keepSelection = new QCheckBox(tr("Keep the selected area position"), page);
    m_autoCopy->setChecked(m_config.autoCopyLink);
    m_autoClose->setChecked(m_config.autoCloseUploadWindow);
    m_notifications->setChecked(m_config.showNotifications);
    m_cursor->setChecked(m_config.captureCursor);
    m_keepSelection->setChecked(m_config.keepSelection);
    if (ScreenGrabber::isWayland())
        m_cursor->setToolTip(tr("On Wayland this works with the grim backend only."));
    for (QCheckBox *box : {m_autoCopy, m_autoClose, m_notifications, m_cursor, m_keepSelection})
        layout->addWidget(box);

    auto *form = new QFormLayout;
    m_captureBackend = new QComboBox(page);
    m_captureBackend->addItem(tr("Automatic"), QStringLiteral("auto"));
    m_captureBackend->addItem(tr("Qt / X11 direct"), QStringLiteral("qt"));
    m_captureBackend->addItem(tr("Desktop portal (GNOME, KDE)"), QStringLiteral("portal"));
    m_captureBackend->addItem(tr("grim (Sway, Hyprland, wlroots)"), QStringLiteral("grim"));
    selectData(m_captureBackend, m_config.captureBackend);
    form->addRow(tr("Capture method:"), m_captureBackend);

    auto *dirRow = new QHBoxLayout;
    m_saveDir = new QLineEdit(m_config.saveDirectory, page);
    auto *browse = new QPushButton(tr("Browse…"), page);
    connect(browse, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Save folder"), m_saveDir->text());
        if (!dir.isEmpty())
            m_saveDir->setText(dir);
    });
    dirRow->addWidget(m_saveDir);
    dirRow->addWidget(browse);
    form->addRow(tr("Save folder:"), dirRow);

    m_template = new QLineEdit(m_config.fileNameTemplate, page);
    m_template->setToolTip(tr("%Y year, %m month, %d day, %H hour, %M minute, %S second, %n counter"));
    form->addRow(tr("File name:"), m_template);

    layout->addLayout(form);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::buildHotkeysPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    auto *form = new QFormLayout;

    for (const QString &id : {HotkeyId::Main, HotkeyId::SaveFull, HotkeyId::UploadFull}) {
        const HotkeyBinding b = m_config.hotkey(id);
        auto *row = new QHBoxLayout;
        auto *enabled = new QCheckBox(page);
        enabled->setChecked(b.enabled);
        auto *edit = new QKeySequenceEdit(b.sequence, page);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        edit->setMaximumSequenceLength(1);
        edit->setClearButtonEnabled(true);
#endif
        connect(enabled, &QCheckBox::toggled, edit, &QWidget::setEnabled);
        edit->setEnabled(b.enabled);
        row->addWidget(enabled);
        row->addWidget(edit, 1);
        form->addRow(hotkeyLabel(id), row);
        m_hotkeys << HotkeyRow{id, enabled, edit};
    }
    layout->addLayout(form);

    if (ScreenGrabber::isWayland()) {
        auto *info = new QLabel(tr("On Wayland, global hotkeys go through your desktop. The first time, it may ask you "
                                   "to confirm them. If your desktop does not support this, bind the command "
                                   "<b>snapline gui</b> in its keyboard settings."),
                                page);
        info->setWordWrap(true);
        layout->addWidget(info);
    }
    if (GnomeShortcut::isGnome()) {
        auto *gnome = new QPushButton(tr("Add the general hotkey as a GNOME shortcut"), page);
        connect(gnome, &QPushButton::clicked, this, &SettingsDialog::installGnomeShortcut);
        layout->addWidget(gnome);
    }
    layout->addStretch();
    return page;
}

void SettingsDialog::installGnomeShortcut()
{
    const QKeySequence seq = m_hotkeys.first().edit->keySequence();
    const bool isPrint = seq.count() == 1 && seq[0].key() == Qt::Key_Print && seq[0].keyboardModifiers() == Qt::NoModifier;
    bool releasePrint = false;
    if (isPrint) {
        releasePrint = QMessageBox::question(this, tr("Use the Print key"),
                                             tr("GNOME uses the Print key for its own screenshot tool. Assign it to "
                                                "Snapline instead?")) == QMessageBox::Yes;
    }
    QString error;
    if (GnomeShortcut::install(seq, QStringLiteral("snapline gui"), releasePrint, &error))
        QMessageBox::information(this, tr("GNOME shortcut"), tr("The shortcut was added to GNOME's keyboard settings."));
    else
        QMessageBox::warning(this, tr("GNOME shortcut"), tr("Could not add the shortcut: %1").arg(error));
}

QWidget *SettingsDialog::buildFormatsPage()
{
    auto *page = new QWidget(this);
    auto *form = new QFormLayout(page);

    m_format = new QComboBox(page);
    m_format->addItem(QStringLiteral("PNG"), QStringLiteral("png"));
    m_format->addItem(QStringLiteral("JPEG"), QStringLiteral("jpg"));
    selectData(m_format, m_config.uploadFormat);
    form->addRow(tr("Upload using the format:"), m_format);

    m_jpegQuality = new QSpinBox(page);
    m_jpegQuality->setRange(1, 100);
    m_jpegQuality->setValue(m_config.jpegQuality);
    form->addRow(tr("JPEG quality:"), m_jpegQuality);

    m_optimize = new QComboBox(page);
    m_optimize->addItem(tr("Never"), QStringLiteral("never"));
    m_optimize->addItem(tr("Always"), QStringLiteral("always"));
    m_optimize->addItem(tr("Only when uploading"), QStringLiteral("upload_only"));
    selectData(m_optimize, m_config.optimize);
    if (!hasOptiPng()) {
        m_optimize->setEnabled(false);
        m_optimize->setToolTip(tr("Install optipng to enable this."));
    }
    form->addRow(tr("Optimize PNG (OptiPNG):"), m_optimize);
    return page;
}

QWidget *SettingsDialog::buildUploadPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);

    auto *top = new QFormLayout;
    m_backend = new QComboBox(page);
    m_backend->addItem(QStringLiteral("Imgur"), QStringLiteral("imgur"));
    m_backend->addItem(tr("Custom HTTP endpoint"), QStringLiteral("custom"));
    top->addRow(tr("Upload to:"), m_backend);
    layout->addLayout(top);

    m_backendPages = new QStackedWidget(page);

    auto *imgur = new QWidget(m_backendPages);
    auto *imgurForm = new QFormLayout(imgur);
    m_imgurId = new QLineEdit(m_config.imgurClientId, imgur);
    imgurForm->addRow(tr("Client ID:"), m_imgurId);
    auto *imgurHelp = new QLabel(tr("<a href=\"https://api.imgur.com/oauth2/addclient\">Register an Imgur application</a> "
                                    "(type: anonymous usage) and paste its Client ID."),
                                 imgur);
    imgurHelp->setOpenExternalLinks(true);
    imgurHelp->setWordWrap(true);
    imgurForm->addRow(imgurHelp);
    m_backendPages->addWidget(imgur);

    auto *custom = new QWidget(m_backendPages);
    auto *customForm = new QFormLayout(custom);
    m_customUrl = new QLineEdit(m_config.customUrl, custom);
    m_customUrl->setPlaceholderText(QStringLiteral("https://example.com/upload"));
    m_customField = new QLineEdit(m_config.customFileField, custom);
    m_customHeaders = new QPlainTextEdit(m_config.customHeaders, custom);
    m_customHeaders->setPlaceholderText(QStringLiteral("Authorization: Bearer …"));
    m_customHeaders->setMaximumHeight(70);
    m_customForm = new QPlainTextEdit(m_config.customFormFields, custom);
    m_customForm->setPlaceholderText(QStringLiteral("key=value"));
    m_customForm->setMaximumHeight(70);
    m_customLinkPath = new QLineEdit(m_config.customLinkPath, custom);
    m_customLinkPath->setPlaceholderText(tr("e.g. data.link — empty if the server answers with the bare URL"));
    customForm->addRow(tr("URL (POST, multipart):"), m_customUrl);
    customForm->addRow(tr("File field name:"), m_customField);
    customForm->addRow(tr("Extra headers:"), m_customHeaders);
    customForm->addRow(tr("Extra form fields:"), m_customForm);
    customForm->addRow(tr("Link in response (JSON path):"), m_customLinkPath);
    m_backendPages->addWidget(custom);

    layout->addWidget(m_backendPages);
    layout->addStretch();

    connect(m_backend, &QComboBox::currentIndexChanged, m_backendPages, &QStackedWidget::setCurrentIndex);
    selectData(m_backend, m_config.uploadBackend);
    m_backendPages->setCurrentIndex(m_backend->currentIndex());
    return page;
}

QWidget *SettingsDialog::buildProxyPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    m_proxyNone = new QRadioButton(tr("No proxy"), page);
    m_proxySystem = new QRadioButton(tr("Use the system proxy settings"), page);
    m_proxyManual = new QRadioButton(tr("Manual proxy configuration"), page);
    layout->addWidget(m_proxyNone);
    layout->addWidget(m_proxySystem);
    layout->addWidget(m_proxyManual);

    auto *form = new QFormLayout;
    m_proxyHost = new QLineEdit(m_config.proxyHost, page);
    m_proxyPort = new QSpinBox(page);
    m_proxyPort->setRange(1, 65535);
    m_proxyPort->setValue(m_config.proxyPort);
    form->addRow(tr("HTTP(S) proxy:"), m_proxyHost);
    form->addRow(tr("Port:"), m_proxyPort);
    layout->addLayout(form);
    layout->addStretch();

    auto syncEnabled = [this]() {
        m_proxyHost->setEnabled(m_proxyManual->isChecked());
        m_proxyPort->setEnabled(m_proxyManual->isChecked());
    };
    connect(m_proxyManual, &QRadioButton::toggled, this, syncEnabled);
    if (m_config.proxyType == QLatin1String("none"))
        m_proxyNone->setChecked(true);
    else if (m_config.proxyType == QLatin1String("manual"))
        m_proxyManual->setChecked(true);
    else
        m_proxySystem->setChecked(true);
    syncEnabled();
    return page;
}

Config SettingsDialog::config() const
{
    Config c = m_config;
    c.autoCopyLink = m_autoCopy->isChecked();
    c.autoCloseUploadWindow = m_autoClose->isChecked();
    c.showNotifications = m_notifications->isChecked();
    c.captureCursor = m_cursor->isChecked();
    c.keepSelection = m_keepSelection->isChecked();
    c.captureBackend = m_captureBackend->currentData().toString();
    c.saveDirectory = m_saveDir->text().trimmed();
    c.fileNameTemplate = m_template->text();

    c.hotkeys.clear();
    for (const HotkeyRow &row : m_hotkeys) {
        QKeySequence seq = row.edit->keySequence();
        if (seq.count() > 1)
            seq = QKeySequence(seq[0]);
        c.hotkeys << HotkeyBinding{row.id, seq, row.enabled->isChecked()};
    }

    c.uploadFormat = m_format->currentData().toString();
    c.jpegQuality = m_jpegQuality->value();
    c.optimize = m_optimize->currentData().toString();

    c.uploadBackend = m_backend->currentData().toString();
    c.imgurClientId = m_imgurId->text().trimmed();
    c.customUrl = m_customUrl->text().trimmed();
    c.customFileField = m_customField->text().trimmed();
    c.customHeaders = m_customHeaders->toPlainText();
    c.customFormFields = m_customForm->toPlainText();
    c.customLinkPath = m_customLinkPath->text().trimmed();

    c.proxyType = m_proxyNone->isChecked() ? QStringLiteral("none")
                  : m_proxyManual->isChecked() ? QStringLiteral("manual")
                                               : QStringLiteral("system");
    c.proxyHost = m_proxyHost->text().trimmed();
    c.proxyPort = m_proxyPort->value();
    return c;
}

void SettingsDialog::accept()
{
    const Config c = config();
    QString offending;
    switch (validateHotkeys(c.hotkeys, &offending)) {
    case HotkeyError::Invalid:
        m_tabs->setCurrentIndex(1);
        QMessageBox::warning(this, windowTitle(),
                             tr("The hotkey \"%1\" is invalid. To disable a hotkey, uncheck its checkbox.")
                                 .arg(hotkeyLabel(offending)));
        return;
    case HotkeyError::Duplicate:
        m_tabs->setCurrentIndex(1);
        QMessageBox::warning(this, windowTitle(),
                             tr("You cannot use the same hotkey for different functions. Please change one of them."));
        return;
    case HotkeyError::None:
        break;
    }
    emit applied(c);
    QDialog::accept();
}

} // namespace snapline
