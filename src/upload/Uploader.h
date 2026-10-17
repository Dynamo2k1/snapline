#pragma once

#include <QNetworkRequest>
#include <QObject>
#include <QPointer>
#include <QUrl>

class QHttpMultiPart;
class QNetworkAccessManager;
class QNetworkReply;

namespace snapline {

struct Config;

class Uploader : public QObject {
    Q_OBJECT

public:
    // Returns nullptr and sets *error when the configured backend is incomplete.
    static Uploader *create(const Config &config, QNetworkAccessManager *nam, QObject *parent, QString *error);
    static void applyProxy(QNetworkAccessManager *nam, const Config &config);

    virtual QString name() const = 0;
    void upload(const QByteArray &data, const QString &mimeType, const QString &fileName);
    void abort();

signals:
    void progress(qint64 sent, qint64 total);
    void succeeded(const QUrl &url);
    void failed(const QString &message, const QString &details);

protected:
    Uploader(QNetworkAccessManager *nam, QObject *parent);
    virtual QNetworkRequest request() const = 0;
    virtual void fillForm(QHttpMultiPart *form, const QByteArray &data, const QString &mimeType,
                          const QString &fileName) const = 0;
    // Returns the image link or sets *error.
    virtual QUrl parseResponse(const QByteArray &body, QString *error) const = 0;

private:
    QNetworkAccessManager *m_nam;
    QPointer<QNetworkReply> m_reply;
};

} // namespace snapline
