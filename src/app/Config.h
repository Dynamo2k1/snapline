#pragma once

#include "core/Hotkeys.h"

#include <QList>
#include <QRect>
#include <QString>

namespace snapline {

namespace HotkeyId {
inline const QString Main = QStringLiteral("main");
inline const QString SaveFull = QStringLiteral("save_full");
inline const QString UploadFull = QStringLiteral("upload_full");
} // namespace HotkeyId

struct Config {
    QList<HotkeyBinding> hotkeys;

    bool autoCopyLink = true;
    bool autoCloseUploadWindow = true;
    bool showNotifications = true;
    bool captureCursor = false;
    bool keepSelection = false;
    QString captureBackend = QStringLiteral("auto"); // auto | qt | portal | grim

    QString uploadFormat = QStringLiteral("png"); // png | jpg
    int jpegQuality = 90;
    QString optimize = QStringLiteral("never"); // never | always | upload_only

    QString saveDirectory;
    QString fileNameTemplate = QStringLiteral("Screenshot_%Y-%m-%d_%H-%M-%S");

    QString proxyType = QStringLiteral("system"); // none | system | manual
    QString proxyHost;
    int proxyPort = 8080;

    QString uploadBackend = QStringLiteral("imgur"); // imgur | custom
    QString imgurClientId;
    QString customUrl;
    QString customFileField = QStringLiteral("file");
    QString customHeaders;    // "Name: value" per line
    QString customFormFields; // "key=value" per line
    QString customLinkPath;   // JSON path; empty = whole body

    QRect lastSelection;

    HotkeyBinding hotkey(const QString &id) const;

    static Config load();
    void save() const;
};

} // namespace snapline
