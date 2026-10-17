#include "app/Config.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace snapline {

namespace {

QList<HotkeyBinding> defaultHotkeys()
{
    return {
        {HotkeyId::Main, QKeySequence(Qt::Key_Print), true},
        {HotkeyId::SaveFull, QKeySequence(Qt::SHIFT | Qt::Key_Print), true},
        {HotkeyId::UploadFull, QKeySequence(), false},
    };
}

QString defaultSaveDirectory()
{
    QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (pictures.isEmpty())
        pictures = QDir::homePath();
    return QDir(pictures).filePath(QStringLiteral("Screenshots"));
}

} // namespace

HotkeyBinding Config::hotkey(const QString &id) const
{
    for (const HotkeyBinding &b : hotkeys) {
        if (b.id == id)
            return b;
    }
    return {id, {}, false};
}

Config Config::load()
{
    QSettings s;
    Config c;

    c.hotkeys = defaultHotkeys();
    s.beginGroup(QStringLiteral("hotkeys"));
    for (HotkeyBinding &b : c.hotkeys) {
        b.sequence = QKeySequence(s.value(b.id, b.sequence.toString(QKeySequence::PortableText)).toString(),
                                  QKeySequence::PortableText);
        b.enabled = s.value(b.id + QStringLiteral("_enabled"), b.enabled).toBool();
    }
    s.endGroup();

    s.beginGroup(QStringLiteral("general"));
    c.autoCopyLink = s.value(QStringLiteral("auto_copy_link"), c.autoCopyLink).toBool();
    c.autoCloseUploadWindow = s.value(QStringLiteral("auto_close_upload_window"), c.autoCloseUploadWindow).toBool();
    c.showNotifications = s.value(QStringLiteral("show_notifications"), c.showNotifications).toBool();
    c.captureCursor = s.value(QStringLiteral("capture_cursor"), c.captureCursor).toBool();
    c.keepSelection = s.value(QStringLiteral("keep_selection"), c.keepSelection).toBool();
    c.captureBackend = s.value(QStringLiteral("capture_backend"), c.captureBackend).toString();
    c.lastSelection = s.value(QStringLiteral("last_selection")).toRect();
    s.endGroup();

    s.beginGroup(QStringLiteral("format"));
    c.uploadFormat = s.value(QStringLiteral("upload_format"), c.uploadFormat).toString();
    c.jpegQuality = qBound(1, s.value(QStringLiteral("jpeg_quality"), c.jpegQuality).toInt(), 100);
    c.optimize = s.value(QStringLiteral("optimize"), c.optimize).toString();
    s.endGroup();

    s.beginGroup(QStringLiteral("save"));
    c.saveDirectory = s.value(QStringLiteral("directory"), defaultSaveDirectory()).toString();
    c.fileNameTemplate = s.value(QStringLiteral("filename_template"), c.fileNameTemplate).toString();
    s.endGroup();

    s.beginGroup(QStringLiteral("network"));
    c.proxyType = s.value(QStringLiteral("proxy_type"), c.proxyType).toString();
    c.proxyHost = s.value(QStringLiteral("proxy_host")).toString();
    c.proxyPort = s.value(QStringLiteral("proxy_port"), c.proxyPort).toInt();
    s.endGroup();

    s.beginGroup(QStringLiteral("upload"));
    c.uploadBackend = s.value(QStringLiteral("backend"), c.uploadBackend).toString();
    c.imgurClientId = s.value(QStringLiteral("imgur_client_id")).toString();
    c.customUrl = s.value(QStringLiteral("custom_url")).toString();
    c.customFileField = s.value(QStringLiteral("custom_file_field"), c.customFileField).toString();
    c.customHeaders = s.value(QStringLiteral("custom_headers")).toString();
    c.customFormFields = s.value(QStringLiteral("custom_form_fields")).toString();
    c.customLinkPath = s.value(QStringLiteral("custom_link_path")).toString();
    s.endGroup();

    return c;
}

void Config::save() const
{
    QSettings s;

    s.beginGroup(QStringLiteral("hotkeys"));
    for (const HotkeyBinding &b : hotkeys) {
        s.setValue(b.id, b.sequence.toString(QKeySequence::PortableText));
        s.setValue(b.id + QStringLiteral("_enabled"), b.enabled);
    }
    s.endGroup();

    s.beginGroup(QStringLiteral("general"));
    s.setValue(QStringLiteral("auto_copy_link"), autoCopyLink);
    s.setValue(QStringLiteral("auto_close_upload_window"), autoCloseUploadWindow);
    s.setValue(QStringLiteral("show_notifications"), showNotifications);
    s.setValue(QStringLiteral("capture_cursor"), captureCursor);
    s.setValue(QStringLiteral("keep_selection"), keepSelection);
    s.setValue(QStringLiteral("capture_backend"), captureBackend);
    s.setValue(QStringLiteral("last_selection"), lastSelection);
    s.endGroup();

    s.beginGroup(QStringLiteral("format"));
    s.setValue(QStringLiteral("upload_format"), uploadFormat);
    s.setValue(QStringLiteral("jpeg_quality"), jpegQuality);
    s.setValue(QStringLiteral("optimize"), optimize);
    s.endGroup();

    s.beginGroup(QStringLiteral("save"));
    s.setValue(QStringLiteral("directory"), saveDirectory);
    s.setValue(QStringLiteral("filename_template"), fileNameTemplate);
    s.endGroup();

    s.beginGroup(QStringLiteral("network"));
    s.setValue(QStringLiteral("proxy_type"), proxyType);
    s.setValue(QStringLiteral("proxy_host"), proxyHost);
    s.setValue(QStringLiteral("proxy_port"), proxyPort);
    s.endGroup();

    s.beginGroup(QStringLiteral("upload"));
    s.setValue(QStringLiteral("backend"), uploadBackend);
    s.setValue(QStringLiteral("imgur_client_id"), imgurClientId);
    s.setValue(QStringLiteral("custom_url"), customUrl);
    s.setValue(QStringLiteral("custom_file_field"), customFileField);
    s.setValue(QStringLiteral("custom_headers"), customHeaders);
    s.setValue(QStringLiteral("custom_form_fields"), customFormFields);
    s.setValue(QStringLiteral("custom_link_path"), customLinkPath);
    s.endGroup();
}

} // namespace snapline
