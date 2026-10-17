#include "upload/Uploader.h"

#include "app/Config.h"
#include "core/JsonPath.h"

#include <QHttpMultiPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QNetworkReply>

namespace snapline {

namespace {

// Replies are small JSON or a bare link; anything bigger is a broken or hostile server.
constexpr qint64 kMaxReplyBytes = 1024 * 1024;

QHttpPart filePart(const QString &field, const QByteArray &data, const QString &mimeType, const QString &fileName)
{
    QHttpPart part;
    part.setHeader(QNetworkRequest::ContentTypeHeader, mimeType);
    part.setHeader(QNetworkRequest::ContentDispositionHeader,
                   QStringLiteral("form-data; name=\"%1\"; filename=\"%2\"").arg(field, fileName));
    part.setBody(data);
    return part;
}

QHttpPart textPart(const QString &field, const QString &value)
{
    QHttpPart part;
    part.setHeader(QNetworkRequest::ContentDispositionHeader, QStringLiteral("form-data; name=\"%1\"").arg(field));
    part.setBody(value.toUtf8());
    return part;
}

bool isWebUrl(const QUrl &url)
{
    return url.isValid() && (url.scheme() == QLatin1String("https") || url.scheme() == QLatin1String("http"));
}

class ImgurUploader : public Uploader {
public:
    ImgurUploader(const QString &clientId, QNetworkAccessManager *nam, QObject *parent)
        : Uploader(nam, parent), m_clientId(clientId) {}

    QString name() const override { return QStringLiteral("Imgur"); }

protected:
    QNetworkRequest request() const override
    {
        QNetworkRequest r(QUrl(QStringLiteral("https://api.imgur.com/3/image")));
        r.setRawHeader("Authorization", "Client-ID " + m_clientId.toUtf8());
        return r;
    }

    void fillForm(QHttpMultiPart *form, const QByteArray &data, const QString &mimeType,
                  const QString &fileName) const override
    {
        form->append(filePart(QStringLiteral("image"), data, mimeType, fileName));
        form->append(textPart(QStringLiteral("type"), QStringLiteral("file")));
    }

    QUrl parseResponse(const QByteArray &body, QString *error) const override
    {
        bool ok = false;
        const QUrl link(extractJsonPath(body, QStringLiteral("data.link"), &ok));
        if (ok && isWebUrl(link))
            return link;
        const QString reason = extractJsonPath(body, QStringLiteral("data.error"), &ok);
        *error = ok ? reason : QObject::tr("Unexpected response from Imgur.");
        return {};
    }

private:
    QString m_clientId;
};

class CustomUploader : public Uploader {
public:
    CustomUploader(const Config &config, QNetworkAccessManager *nam, QObject *parent)
        : Uploader(nam, parent),
          m_url(config.customUrl.trimmed()),
          m_field(config.customFileField.trimmed().isEmpty() ? QStringLiteral("file") : config.customFileField.trimmed()),
          m_headers(config.customHeaders),
          m_formFields(config.customFormFields),
          m_linkPath(config.customLinkPath) {}

    QString name() const override { return m_url.host(); }

protected:
    QNetworkRequest request() const override
    {
        QNetworkRequest r(m_url);
        for (const QString &line : m_headers.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            const int colon = line.indexOf(QLatin1Char(':'));
            if (colon > 0)
                r.setRawHeader(line.left(colon).trimmed().toUtf8(), line.mid(colon + 1).trimmed().toUtf8());
        }
        return r;
    }

    void fillForm(QHttpMultiPart *form, const QByteArray &data, const QString &mimeType,
                  const QString &fileName) const override
    {
        for (const QString &line : m_formFields.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            const int eq = line.indexOf(QLatin1Char('='));
            if (eq > 0)
                form->append(textPart(line.left(eq).trimmed(), line.mid(eq + 1).trimmed()));
        }
        form->append(filePart(m_field, data, mimeType, fileName));
    }

    QUrl parseResponse(const QByteArray &body, QString *error) const override
    {
        bool ok = false;
        const QUrl link(extractJsonPath(body, m_linkPath, &ok));
        if (ok && isWebUrl(link))
            return link;
        *error = QObject::tr("Incorrect response from the image hosting server.");
        return {};
    }

private:
    QUrl m_url;
    QString m_field;
    QString m_headers;
    QString m_formFields;
    QString m_linkPath;
};

} // namespace

Uploader::Uploader(QNetworkAccessManager *nam, QObject *parent) : QObject(parent), m_nam(nam) {}

Uploader *Uploader::create(const Config &config, QNetworkAccessManager *nam, QObject *parent, QString *error)
{
    if (config.uploadBackend == QLatin1String("custom")) {
        if (!isWebUrl(QUrl(config.customUrl.trimmed()))) {
            *error = tr("No upload URL is configured. Set one in Settings → Upload.");
            return nullptr;
        }
        return new CustomUploader(config, nam, parent);
    }
    if (config.imgurClientId.trimmed().isEmpty()) {
        *error = tr("Imgur needs a Client ID. Register an application at https://api.imgur.com/oauth2/addclient "
                    "and paste its Client ID in Settings → Upload.");
        return nullptr;
    }
    return new ImgurUploader(config.imgurClientId.trimmed(), nam, parent);
}

void Uploader::applyProxy(QNetworkAccessManager *nam, const Config &config)
{
    if (config.proxyType == QLatin1String("manual") && !config.proxyHost.trimmed().isEmpty()) {
        QNetworkProxyFactory::setUseSystemConfiguration(false);
        nam->setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, config.proxyHost.trimmed(), quint16(config.proxyPort)));
    } else if (config.proxyType == QLatin1String("none")) {
        QNetworkProxyFactory::setUseSystemConfiguration(false);
        nam->setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
    } else {
        QNetworkProxyFactory::setUseSystemConfiguration(true);
        nam->setProxy(QNetworkProxy(QNetworkProxy::DefaultProxy));
    }
}

void Uploader::upload(const QByteArray &data, const QString &mimeType, const QString &fileName)
{
    abort();
    auto *form = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    fillForm(form, data, mimeType, fileName);
    QNetworkRequest req = request();
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Snapline/" SNAPLINE_VERSION));
    req.setTransferTimeout(60000);

    QNetworkReply *reply = m_nam->post(req, form);
    form->setParent(reply);
    m_reply = reply;

    connect(reply, &QNetworkReply::uploadProgress, this, &Uploader::progress);
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64) {
        if (received > kMaxReplyBytes) {
            reply->setProperty("snapline_too_large", true);
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::OperationCanceledError && m_reply != reply)
            return;
        if (reply->property("snapline_too_large").toBool()) {
            emit failed(tr("The server sent an unexpectedly large reply."), QString());
            return;
        }
        const QByteArray body = reply->readAll();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString details = QStringLiteral("HTTP %1\n%2\n\n%3")
                                    .arg(status)
                                    .arg(reply->errorString(), QString::fromUtf8(body.left(4000)));
        QString error;
        const QUrl link = parseResponse(body, &error);
        if (reply->error() != QNetworkReply::NoError && link.isEmpty()) {
            emit failed(error.isEmpty() ? reply->errorString() : error, details);
            return;
        }
        if (link.isEmpty()) {
            emit failed(error, details);
            return;
        }
        emit succeeded(link);
    });
}

void Uploader::abort()
{
    if (m_reply) {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->abort();
    }
}

} // namespace snapline
