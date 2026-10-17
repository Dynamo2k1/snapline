#pragma once

#include "app/Config.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QKeySequenceEdit;
class QLineEdit;
class QPlainTextEdit;
class QRadioButton;
class QSpinBox;
class QStackedWidget;
class QTabWidget;

namespace snapline {

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(const Config &config, QWidget *parent = nullptr);
    Config config() const;
    void showProxyPage();

signals:
    void applied(const snapline::Config &config);
    // True while a hotkey field has focus and records keys; global hotkeys should be off meanwhile.
    void hotkeyRecording(bool recording);

protected:
    void accept() override;
    void hideEvent(QHideEvent *event) override;

private:
    struct HotkeyRow {
        QString id;
        QCheckBox *enabled;
        QKeySequenceEdit *edit;
    };

    QWidget *buildGeneralPage();
    QWidget *buildHotkeysPage();
    QWidget *buildFormatsPage();
    QWidget *buildUploadPage();
    QWidget *buildProxyPage();
    void updateRecording();

    Config m_config;
    QTabWidget *m_tabs = nullptr;
    QWidget *m_proxyPage = nullptr;

    QCheckBox *m_autoCopy = nullptr;
    QCheckBox *m_autoClose = nullptr;
    QCheckBox *m_notifications = nullptr;
    QCheckBox *m_cursor = nullptr;
    QCheckBox *m_keepSelection = nullptr;
    QComboBox *m_captureBackend = nullptr;
    QLineEdit *m_saveDir = nullptr;
    QLineEdit *m_template = nullptr;

    QList<HotkeyRow> m_hotkeys;
    bool m_recording = false;

    QComboBox *m_format = nullptr;
    QSpinBox *m_jpegQuality = nullptr;
    QComboBox *m_optimize = nullptr;

    QComboBox *m_backend = nullptr;
    QStackedWidget *m_backendPages = nullptr;
    QLineEdit *m_imgurId = nullptr;
    QLineEdit *m_customUrl = nullptr;
    QLineEdit *m_customField = nullptr;
    QPlainTextEdit *m_customHeaders = nullptr;
    QPlainTextEdit *m_customForm = nullptr;
    QLineEdit *m_customLinkPath = nullptr;

    QRadioButton *m_proxyNone = nullptr;
    QRadioButton *m_proxySystem = nullptr;
    QRadioButton *m_proxyManual = nullptr;
    QLineEdit *m_proxyHost = nullptr;
    QSpinBox *m_proxyPort = nullptr;
};

} // namespace snapline
