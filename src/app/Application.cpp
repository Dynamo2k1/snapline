#include "app/Application.h"

#include "app/Clipboard.h"
#include "app/HotkeyManager.h"
#include "app/ImageExport.h"
#include "app/Notifier.h"
#include "app/SettingsDialog.h"
#include "capture/ScreenGrabber.h"
#include "core/FileNameTemplate.h"
#include "overlay/CaptureController.h"
#include "upload/UploadDialog.h"
#include "upload/Uploader.h"

#include <QAbstractButton>
#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QPainter>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrlQuery>
#include <QVBoxLayout>

namespace snapline {

namespace {

void revealInFileManager(const QString &path)
{
    QDBusMessage call = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.FileManager1"), QStringLiteral("/org/freedesktop/FileManager1"),
        QStringLiteral("org.freedesktop.FileManager1"), QStringLiteral("ShowItems"));
    call << QStringList{QUrl::fromLocalFile(path).toString()} << QString();
    const QDBusMessage reply = QDBusConnection::sessionBus().call(call, QDBus::Block, 2000);
    if (reply.type() != QDBusMessage::ReplyMessage)
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
}

QString extensionFor(const QString &format)
{
    return format == QLatin1String("jpg") ? QStringLiteral("jpg") : QStringLiteral("png");
}

} // namespace

Application::Application(QObject *parent) : QObject(parent), m_config(Config::load()) {}

Application::~Application() = default;

void Application::start()
{
    m_network = new QNetworkAccessManager(this);
    Uploader::applyProxy(m_network, m_config);

    m_tray = new QSystemTrayIcon(QApplication::windowIcon(), this);
    m_notifier = new Notifier(m_tray, this);
    m_menu = new QMenu();
    m_tray->setContextMenu(m_menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger)
            captureInteractive();
    });
    connect(m_menu, &QMenu::aboutToShow, this, &Application::rebuildTrayMenu);
    rebuildTrayMenu();
    m_tray->show();

    m_hotkeys = new HotkeyManager(this);
    connect(m_hotkeys, &HotkeyManager::activated, this, [this](const QString &id) {
        if (id == HotkeyId::Main)
            captureInteractive();
        else if (id == HotkeyId::SaveFull)
            captureFullScreen(false);
        else if (id == HotkeyId::UploadFull)
            captureFullScreen(true);
    });
    connect(m_hotkeys, &HotkeyManager::registrationFailed, this, [this](const QString &id, const QString &reason) {
        if (m_hotkeyWarningShown)
            return;
        m_hotkeyWarningShown = true;
        const QString key = m_config.hotkey(id).sequence.toString(QKeySequence::NativeText);
        notify(tr("Failed to register a hotkey: %1").arg(key), reason + QLatin1Char(' ') + tr("Click to open Settings."),
               [this]() { showSettings(); });
    });
    m_hotkeys->setReplaceDesktopKeys(m_config.replaceDesktopKeys);
    m_hotkeys->setBindings(m_config.hotkeys);

    const HotkeyBinding main = m_config.hotkey(HotkeyId::Main);
    if (main.enabled)
        m_tray->setToolTip(tr("Press %1 to take a screenshot").arg(main.sequence.toString(QKeySequence::NativeText)));
    else
        m_tray->setToolTip(QApplication::applicationDisplayName());
}

void Application::handleCommand(const QString &command, const QString &activationToken)
{
    m_activationToken = activationToken;
    if (command == QLatin1String("gui"))
        captureInteractive();
    else if (command == QLatin1String("full"))
        captureFullScreen(false);
    else if (command == QLatin1String("full-upload"))
        captureFullScreen(true);
    else if (command == QLatin1String("settings"))
        showSettings();
    else if (command == QLatin1String("quit"))
        QApplication::quit();
}

void Application::grab(std::function<void(const Capture &)> then)
{
    if (m_grabbing || m_capture)
        return;
    m_grabbing = true;
    ScreenGrabber *grabber = ScreenGrabber::create(m_config.captureBackend, this);
    connect(grabber, &ScreenGrabber::finished, this, [this, grabber, then](const Capture &capture) {
        grabber->deleteLater();
        m_grabbing = false;
        then(capture);
    });
    connect(grabber, &ScreenGrabber::failed, this, [this, grabber](const QString &message) {
        grabber->deleteLater();
        m_grabbing = false;
        if (!message.isEmpty())
            error(message);
    });
    connect(grabber, &ScreenGrabber::permissionDenied, this, [this, grabber, then]() {
        grabber->deleteLater();
        m_grabbing = false;
        askScreenshotPermission(then);
    });
    grabber->grab(m_config.captureCursor);
}

void Application::askScreenshotPermission(std::function<void(const Capture &)> then)
{
    if (m_permissionDialog) {
        bringToFront(m_permissionDialog);
        return;
    }
    auto *dialog = new QDialog;
    m_permissionDialog = dialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("Screenshot permission"));
    auto *layout = new QVBoxLayout(dialog);
    auto *text = new QLabel(tr("Your desktop has to allow Snapline to take screenshots.\n\n"
                               "Click the button below, then choose Allow in the window that appears. "
                               "You only need to do this once."),
                            dialog);
    text->setWordWrap(true);
    auto *status = new QLabel(dialog);
    status->setWordWrap(true);
    status->setTextFormat(Qt::PlainText);
    auto *buttons = new QDialogButtonBox(dialog);
    QPushButton *allow = buttons->addButton(tr("Allow screenshots…"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    layout->addWidget(text);
    layout->addWidget(status);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);

    // The click focuses this window, which is what lets the desktop show its own question.
    connect(allow, &QPushButton::clicked, dialog, [this, dialog, allow, status, then]() {
        allow->setEnabled(false);
        status->setText(tr("Waiting for your answer in the desktop's window…"));
        ScreenGrabber *grabber = ScreenGrabber::create(QStringLiteral("portal"), dialog);
        grabber->setUserMayBeAsked(true);
        connect(grabber, &ScreenGrabber::finished, dialog, [this, dialog, then](const Capture &) {
            // That picture shows this window; take the real one once it is gone.
            dialog->close();
            QTimer::singleShot(400, this, [this, then]() { grab(then); });
        });
        connect(grabber, &ScreenGrabber::permissionDenied, dialog, [allow, status]() {
            status->setText(tr("The desktop did not allow it. Click the button to try again."));
            allow->setEnabled(true);
        });
        connect(grabber, &ScreenGrabber::failed, dialog, [allow, status](const QString &message) {
            status->setText(message.isEmpty() ? tr("Cancelled.") : message);
            allow->setEnabled(true);
        });
        grabber->grab(false);
    });
    dialog->show();
    bringToFront(dialog);
    // Without a token from the hotkey the desktop may keep the window behind others; a notification
    // click brings its own token.
    if (m_activationToken.isEmpty() && ScreenGrabber::isWayland()) {
        notify(tr("Screenshot permission needed"), tr("Click here, then allow Snapline to take screenshots."),
               [this]() {
                   if (m_permissionDialog)
                       bringToFront(m_permissionDialog);
               });
    }
}

void Application::bringToFront(QWidget *window)
{
    // On Wayland only a token from a user action lets a window take focus; Qt uses it from the
    // environment when activating.
    if (!m_activationToken.isEmpty()) {
        qputenv("XDG_ACTIVATION_TOKEN", m_activationToken.toUtf8());
        m_activationToken.clear();
    }
    window->raise();
    window->activateWindow();
}

void Application::captureInteractive()
{
    grab([this](const Capture &capture) {
        const QRect initial = m_config.keepSelection ? m_config.lastSelection : QRect();
        m_capture = new CaptureController(capture, initial, this);
        connect(m_capture, &CaptureController::finished, this, &Application::onCaptureFinished);
        m_capture->show();
    });
}

void Application::captureFullScreen(bool upload)
{
    grab([this, upload](const Capture &capture) {
        const QRect all = capture.virtualGeometry();
        QImage image = capture.render(all, capture.maxDprIn(all));
        image.setDevicePixelRatio(1.0);
        if (upload)
            uploadImage(image);
        else
            saveImage(image, false);
    });
}

void Application::onCaptureFinished(Action action, const QImage &image, const QRect &selection)
{
    if (m_capture)
        m_capture->deleteLater();
    if (action != Action::Close && m_config.keepSelection && m_config.lastSelection != selection) {
        m_config.lastSelection = selection;
        m_config.save();
    }

    switch (action) {
    case Action::Copy:
        copyImage(image);
        break;
    case Action::Save:
        saveImage(image, true);
        break;
    case Action::Print:
        printImage(image);
        break;
    case Action::Upload:
        uploadImage(image);
        break;
    default:
        if (isShareAction(action))
            uploadImage(image, [this, action](const QUrl &link) { share(action, link); });
        break;
    }
}

void Application::copyImage(const QImage &image)
{
    Clipboard::setImage(image);
    notify(tr("Screenshot copied"), tr("Your screenshot is in the clipboard."));
}

void Application::saveImage(const QImage &image, bool askForPath)
{
    const QString ext = extensionFor(m_config.uploadFormat);
    static int counter = 0;
    const QString base = expandFileNameTemplate(m_config.fileNameTemplate, QDateTime::currentDateTime(), ++counter);
    QString path = uniqueFilePath(m_config.saveDirectory, base, ext);

    if (askForPath) {
        QDir().mkpath(m_config.saveDirectory);
        QString filter = ext == QLatin1String("jpg") ? tr("JPEG image (*.jpg *.jpeg)") : tr("PNG image (*.png)");
        path = QFileDialog::getSaveFileName(nullptr, tr("Save screenshot"), path,
                                            tr("PNG image (*.png)") + QStringLiteral(";;") + tr("JPEG image (*.jpg *.jpeg)"),
                                            &filter);
        if (path.isEmpty())
            return;
        if (QFileInfo(path).suffix().isEmpty())
            path += filter.contains(QLatin1String("jpg")) ? QStringLiteral(".jpg") : QStringLiteral(".png");
        const QString dir = QFileInfo(path).absolutePath();
        if (dir != m_config.saveDirectory) {
            m_config.saveDirectory = dir;
            m_config.save();
        }
    }

    EncodeOptions options;
    const QString suffix = QFileInfo(path).suffix().toLower();
    options.format = (suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg")) ? "jpg" : "png";
    options.jpegQuality = m_config.jpegQuality;
    options.optimizePng = m_config.optimize == QLatin1String("always");

    QString err;
    if (!writeImage(image, path, options, &err)) {
        error(tr("Could not save %1: %2").arg(path, err));
        return;
    }
    notify(tr("Screenshot saved"), tr("Saved to %1. Click to open the folder.").arg(path),
           [path]() { revealInFileManager(path); });
}

void Application::printImage(const QImage &image)
{
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dialog(&printer);
    dialog.setWindowTitle(tr("Print screenshot"));
    if (dialog.exec() != QDialog::Accepted)
        return;
    QPainter painter(&printer);
    const QRect page = painter.viewport();
    QSize size = image.size();
    if (size.width() > page.width() || size.height() > page.height())
        size.scale(page.size(), Qt::KeepAspectRatio);
    painter.setViewport(page.x(), page.y(), size.width(), size.height());
    painter.setWindow(image.rect());
    painter.drawImage(0, 0, image);
}

void Application::uploadImage(const QImage &image, std::function<void(const QUrl &)> then)
{
    QString err;
    Uploader *uploader = Uploader::create(m_config, m_network, nullptr, &err);
    if (!uploader) {
        QMessageBox box(QMessageBox::Information, tr("Upload"), err, QMessageBox::Ok | QMessageBox::Open);
        box.button(QMessageBox::Open)->setText(tr("Open Settings"));
        if (box.exec() == QMessageBox::Open)
            showSettings();
        return;
    }

    EncodeOptions options;
    options.format = m_config.uploadFormat == QLatin1String("jpg") ? "jpg" : "png";
    options.jpegQuality = m_config.jpegQuality;
    options.optimizePng = m_config.optimize != QLatin1String("never");
    const QByteArray data = encodeImage(image, options);
    const QString mime = options.format == "jpg" ? QStringLiteral("image/jpeg") : QStringLiteral("image/png");
    const QString name = QStringLiteral("screenshot.") + QString::fromLatin1(options.format);

    // Share actions need the link immediately, so keep their window from lingering.
    auto *dialog = new UploadDialog(uploader, data, mime, name, m_config.autoCloseUploadWindow || then, nullptr);
    connect(dialog, &UploadDialog::proxySettingsRequested, this, [this]() { showSettings(true); });
    connect(dialog, &UploadDialog::uploaded, this, [this, then](const QUrl &url) {
        m_history.add(url);
        if (m_config.autoCopyLink)
            Clipboard::setText(url.toString());
        if (then) {
            then(url);
        } else {
            notify(tr("Screenshot uploaded"),
                   m_config.autoCopyLink ? tr("The link is in your clipboard: %1").arg(url.toString()) : url.toString(),
                   [url]() { QDesktopServices::openUrl(url); });
        }
    });
    dialog->show();
    dialog->start();
}

void Application::share(Action action, const QUrl &link)
{
    const QString encoded = QString::fromUtf8(QUrl::toPercentEncoding(link.toString()));
    QString target;
    switch (action) {
    case Action::ShareGoogle:
        target = QStringLiteral("https://lens.google.com/uploadbyurl?url=") + encoded;
        break;
    case Action::ShareTinEye:
        target = QStringLiteral("https://tineye.com/search?url=") + encoded;
        break;
    case Action::ShareEmail:
        target = QStringLiteral("mailto:?subject=%1&body=%2")
                     .arg(QString::fromUtf8(QUrl::toPercentEncoding(tr("Screenshot"))), encoded);
        break;
    case Action::ShareTwitter:
        target = QStringLiteral("https://twitter.com/intent/tweet?url=") + encoded;
        break;
    case Action::ShareFacebook:
        target = QStringLiteral("https://www.facebook.com/sharer/sharer.php?u=") + encoded;
        break;
    case Action::ShareVk:
        target = QStringLiteral("https://vk.com/share.php?url=") + encoded;
        break;
    case Action::SharePinterest:
        target = QStringLiteral("https://pinterest.com/pin/create/button/?url=%1&media=%1").arg(encoded);
        break;
    default:
        return;
    }
    QDesktopServices::openUrl(QUrl(target, QUrl::StrictMode));
}

void Application::showSettings(bool proxyPage)
{
    if (!m_settings) {
        m_settings = new SettingsDialog(m_config);
        connect(m_settings, &SettingsDialog::applied, this, &Application::applyConfig);
        connect(m_settings, &SettingsDialog::hotkeyRecording, m_hotkeys, &HotkeyManager::setSuspended);
    }
    if (proxyPage)
        m_settings->showProxyPage();
    m_settings->show();
    m_settings->raise();
    m_settings->activateWindow();
}

void Application::applyConfig(const Config &config)
{
    const bool hotkeysChanged = [&]() {
        for (const HotkeyBinding &b : config.hotkeys) {
            const HotkeyBinding old = m_config.hotkey(b.id);
            if (old.enabled != b.enabled || old.sequence != b.sequence)
                return true;
        }
        return config.replaceDesktopKeys != m_config.replaceDesktopKeys;
    }();
    m_config = config;
    m_config.save();
    Uploader::applyProxy(m_network, m_config);
    if (hotkeysChanged) {
        m_hotkeyWarningShown = false;
        m_hotkeys->setReplaceDesktopKeys(m_config.replaceDesktopKeys);
        m_hotkeys->setBindings(m_config.hotkeys);
    }
}

void Application::rebuildTrayMenu()
{
    m_menu->clear();
    m_menu->addAction(QIcon(QStringLiteral(":/icons/snapline.svg")), tr("Take a screenshot"), this,
                      &Application::captureInteractive);
    m_menu->addAction(tr("Save full screen"), this, [this]() { captureFullScreen(false); });

    QMenu *recent = m_menu->addMenu(tr("Recent uploads"));
    const auto entries = m_history.recent(10);
    if (entries.isEmpty()) {
        recent->addAction(tr("No uploads yet"))->setEnabled(false);
    } else {
        for (const History::Entry &e : entries) {
            const QUrl url = e.url;
            recent->addAction(e.time.toString(QStringLiteral("yyyy-MM-dd HH:mm")) + QStringLiteral("  ") + url.toString(),
                              this, [this, url]() {
                                  Clipboard::setText(url.toString());
                                  notify(tr("Link copied"), url.toString(), [url]() { QDesktopServices::openUrl(url); });
                              });
        }
        recent->addSeparator();
        recent->addAction(tr("Clear history"), this, [this]() { m_history.clear(); });
    }

    m_menu->addSeparator();
    m_menu->addAction(tr("Settings…"), this, [this]() { showSettings(); });
    m_menu->addAction(tr("About"), this, &Application::showAbout);
    m_menu->addSeparator();
    m_menu->addAction(tr("Quit"), qApp, &QApplication::quit);
}

void Application::notify(const QString &title, const QString &body, std::function<void()> onClick)
{
    if (m_config.showNotifications)
        m_notifier->notify(title, body, std::move(onClick));
}

void Application::error(const QString &message)
{
    // Errors are always shown, regardless of the notification setting.
    m_notifier->notify(tr("Snapline error"), message);
}

void Application::showAbout()
{
    QMessageBox::about(nullptr, tr("About Snapline"),
                       tr("<h3>Snapline %1</h3>"
                          "<p>Select an area of the screen, annotate it, then copy, save, print or upload it.</p>"
                          "<p>Capture backend: %2<br>Hotkey backend: %3</p>"
                          "<p>Free software under the GPL-3.0-or-later license. Snapline is an independent project "
                          "and is not affiliated with Lightshot or Skillbrains.</p>")
                           .arg(QStringLiteral(SNAPLINE_VERSION),
                                ScreenGrabber::isWayland() ? QStringLiteral("Wayland") : QStringLiteral("X11"),
                                m_hotkeys->backendName()));
}

} // namespace snapline
