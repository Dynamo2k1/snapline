#pragma once

#include "app/Config.h"
#include "app/History.h"
#include "overlay/Actions.h"

#include <QImage>
#include <QObject>
#include <QPointer>
#include <functional>

class QDialog;
class QWidget;
class QMenu;
class QNetworkAccessManager;
class QSystemTrayIcon;

namespace snapline {

struct Capture;
class CaptureController;
class HotkeyManager;
class Notifier;
class SettingsDialog;

class Application : public QObject {
    Q_OBJECT

public:
    explicit Application(QObject *parent = nullptr);
    ~Application() override;

    void start();

public slots:
    // gui | full | full-upload | settings | quit | ping
    void handleCommand(const QString &command, const QString &activationToken = QString());

private:
    void captureInteractive();
    void captureFullScreen(bool upload);
    void grab(std::function<void(const Capture &)> then);
    void askScreenshotPermission(std::function<void(const Capture &)> then);
    void bringToFront(QWidget *window);
    void onCaptureFinished(Action action, const QImage &image, const QRect &selection);

    void copyImage(const QImage &image);
    void saveImage(const QImage &image, bool askForPath);
    void printImage(const QImage &image);
    void uploadImage(const QImage &image, std::function<void(const QUrl &)> then = {});
    void share(Action action, const QUrl &link);

    void showSettings(bool proxyPage = false);
    void applyConfig(const Config &config);
    void rebuildTrayMenu();
    void notify(const QString &title, const QString &body, std::function<void()> onClick = {});
    void status(const QString &title, const QString &body, std::function<void()> onClick = {});
    void error(const QString &message);
    void showAbout();

    Config m_config;
    History m_history;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
    Notifier *m_notifier = nullptr;
    HotkeyManager *m_hotkeys = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    QPointer<CaptureController> m_capture;
    QPointer<SettingsDialog> m_settings;
    QPointer<QDialog> m_permissionDialog;
    QString m_activationToken; // from the launch that sent the current command
    bool m_grabbing = false;
    bool m_hotkeyWarningShown = false;
};

} // namespace snapline
