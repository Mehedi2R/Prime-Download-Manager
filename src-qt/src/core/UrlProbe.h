#pragma once
#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QPointer>
#include <QTimer>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include "MirrorHealer.h"
#include "CategoryRules.h"
#include "../media/YtDlpWrapper.h"

struct ProbeResult {
    QString url;
    QString finalUrl;
    QString fileName;
    quint64 totalSize = 0;
    bool supportsRanges = false;
    QString mimeType;
    QString category;
    bool isVideo = false;
    QString videoTitle;
    QVector<VideoFormatOption> videoFormats;
    bool success = false;
    QString errorMessage;
};

class UrlProbe : public QObject {
    Q_OBJECT
public:
    explicit UrlProbe(QObject* parent = nullptr) : QObject(parent) {
        // 35-second safety timeout so a probe never hangs or leaks
        QTimer::singleShot(35000, this, [this]() {
            if (!m_finished) {
                m_finished = true;
                if (m_currentReply) {
                    m_currentReply->disconnect();
                    m_currentReply->abort();
                    m_currentReply->deleteLater();
                }
                if (m_videoNetworkReply) {
                    m_videoNetworkReply->disconnect();
                    m_videoNetworkReply->abort();
                    m_videoNetworkReply->deleteLater();
                }
                if (m_currentYtdlp) {
                    m_currentYtdlp->stopProbe();
                    m_currentYtdlp->deleteLater();
                }
                ProbeResult res;
                res.url = m_targetUrl;
                res.finalUrl = m_targetUrl;
                if (CategoryRules::isVideoPortal(m_targetUrl)) {
                    res.isVideo = true;
                    res.category = "Videos";
                    if (!m_fastTitle.isEmpty()) {
                        res.videoTitle = m_fastTitle;
                        res.fileName = CategoryRules::sanitizeFilename(m_fastTitle + ".mp4");
                        res.success = true;
                    } else {
                        QString vId = extractVideoId(m_targetUrl);
                        res.fileName = !vId.isEmpty() ? QString("YouTube_Video_%1.mp4").arg(vId) : "Video_Download.mp4";
                        res.success = false;
                    }
                    VideoFormatOption f2160; f2160.formatId = "bestvideo[height<=2160]+bestaudio/best[height<=2160]/best"; f2160.label = "4K Ultra HD (2160p)"; f2160.height = 2160; f2160.ext = "mp4";
                    VideoFormatOption f1440; f1440.formatId = "bestvideo[height<=1440]+bestaudio/best[height<=1440]/best"; f1440.label = "2K Quad HD (1440p)"; f1440.height = 1440; f1440.ext = "mp4";
                    VideoFormatOption f1080; f1080.formatId = "bestvideo[height<=1080]+bestaudio/best[height<=1080]/best"; f1080.label = "1080p (Full HD)"; f1080.height = 1080; f1080.ext = "mp4";
                    VideoFormatOption f720; f720.formatId = "bestvideo[height<=720]+bestaudio/best[height<=720]/best"; f720.label = "720p (HD)"; f720.height = 720; f720.ext = "mp4";
                    VideoFormatOption f480; f480.formatId = "bestvideo[height<=480]+bestaudio/best[height<=480]/best"; f480.label = "480p (Standard)"; f480.height = 480; f480.ext = "mp4";
                    VideoFormatOption f360; f360.formatId = "bestvideo[height<=360]+bestaudio/best[height<=360]/best"; f360.label = "360p (Medium)"; f360.height = 360; f360.ext = "mp4";
                    VideoFormatOption f240; f240.formatId = "bestvideo[height<=240]+bestaudio/best[height<=240]/best"; f240.label = "240p (Low)"; f240.height = 240; f240.ext = "mp4";
                    VideoFormatOption faudio; faudio.formatId = "bestaudio/best"; faudio.label = "Audio Only (Best Quality)"; faudio.height = 0; faudio.ext = "m4a";
                    res.videoFormats << f2160 << f1440 << f1080 << f720 << f480 << f360 << f240 << faudio;
                } else {
                    res.success = false;
                    res.errorMessage = "Probe timeout";
                }
                emit probeFinished(res);
            }
        });
    }

    ~UrlProbe() {
        if (m_currentReply) {
            m_currentReply->disconnect();
            m_currentReply->abort();
            m_currentReply->deleteLater();
        }
        if (m_videoNetworkReply) {
            m_videoNetworkReply->disconnect();
            m_videoNetworkReply->abort();
            m_videoNetworkReply->deleteLater();
        }
        if (m_currentYtdlp) {
            m_currentYtdlp->stopProbe();
            m_currentYtdlp->deleteLater();
        }
    }

    static QNetworkAccessManager* sharedNam() {
        static QPointer<QNetworkAccessManager> s_nam;
        if (!s_nam) {
            s_nam = new QNetworkAccessManager(QCoreApplication::instance());
        }
        return s_nam;
    }

    void probeUrl(const QString& rawUrl) { probe(rawUrl); }
    void probe(const QString& rawUrl) {
        QString healed = MirrorHealer::healUrl(rawUrl);
        m_targetUrl = healed;
        QUrl qurl(healed);
        if (!qurl.isValid()) {
            m_finished = true;
            ProbeResult res;
            res.url = healed;
            res.success = false;
            res.errorMessage = "Invalid URL format";
            emit probeFinished(res);
            return;
        }

        // Check if video portal (only if not already a direct media stream)
        if (!CategoryRules::isDirectMediaStream(healed) && CategoryRules::isVideoPortal(healed)) {
            probeVideoPortal(healed);
            return;
        }

        sendHeadRequest(healed, 0);
    }

signals:
    void videoTitleDiscovered(const QString& title);
    void probeFinished(const ProbeResult& result);

private:
    void fetchVideoTitleViaNetwork(const QString& url) {
        QUrl qurl(url);
        QString host = qurl.host().toLower();
        QUrl apiUrl;
        if (host.contains("youtube.com") || host.contains("youtu.be")) {
            apiUrl = QUrl(QString("https://www.youtube.com/oembed?url=%1&format=json").arg(QString::fromUtf8(QUrl::toPercentEncoding(url))));
        } else if (host.contains("vimeo.com")) {
            apiUrl = QUrl(QString("https://vimeo.com/api/oembed.json?url=%1").arg(QString::fromUtf8(QUrl::toPercentEncoding(url))));
        } else {
            apiUrl = qurl;
        }

        QNetworkRequest req(apiUrl);
        req.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/128.0.0.0 Safari/537.36");
        req.setRawHeader("Accept", "*/*");
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

        QNetworkReply* reply = sharedNam()->get(req);
        m_videoNetworkReply = reply;
        connect(reply, &QNetworkReply::finished, this, [this, reply, host, url]() {
            if (m_videoNetworkReply == reply) m_videoNetworkReply = nullptr;
            reply->deleteLater();
            if (m_finished || reply->error() != QNetworkReply::NoError) return;
            QByteArray data = reply->readAll();
            QString discoveredTitle;
            if (host.contains("youtube.com") || host.contains("youtu.be") || host.contains("vimeo.com")) {
                QJsonDocument doc = QJsonDocument::fromJson(data);
                if (doc.isObject()) {
                    discoveredTitle = doc.object()["title"].toString().trimmed();
                }
            }
            if (discoveredTitle.isEmpty()) {
                QString html = QString::fromUtf8(data);
                QRegularExpression re("<title[^>]*>(.*?)</title>", QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
                auto match = re.match(html);
                if (match.hasMatch()) {
                    discoveredTitle = match.captured(1).trimmed();
                }
            }
            if (!discoveredTitle.isEmpty()) {
                discoveredTitle = CategoryRules::resolveVideoTitle(discoveredTitle, "", "", "", url);
                if (!discoveredTitle.isEmpty() && !CategoryRules::isGenericTitle(discoveredTitle)) {
                    m_fastTitle = discoveredTitle;
                    emit videoTitleDiscovered(discoveredTitle);
                }
            }
        });
    }

    static QString extractVideoId(const QString& url) {
        QUrl qurl(url);
        if (qurl.host().contains("youtu.be", Qt::CaseInsensitive)) {
            QString path = qurl.path();
            while (path.startsWith('/')) path = path.mid(1);
            return path;
        }
        QUrlQuery query(qurl);
        if (query.hasQueryItem("v")) {
            return query.queryItemValue("v");
        }
        // Generic fallback: last segment of path
        QString p = qurl.path();
        while (p.endsWith('/')) p.chop(1);
        int slash = p.lastIndexOf('/');
        QString seg = (slash != -1) ? p.mid(slash + 1) : "";
        if (seg == "watch") return "";
        return seg;
    }

    void probeVideoPortal(const QString& url) {
        // Fast direct network request to detect title within ~400ms
        fetchVideoTitleViaNetwork(url);

        YtDlpWrapper* ytdlp = new YtDlpWrapper(this);
        m_currentYtdlp = ytdlp;
        connect(ytdlp, &YtDlpWrapper::videoProbed, this, [this, ytdlp, url](const VideoProbeInfo& info) {
            if (m_currentYtdlp == ytdlp) m_currentYtdlp = nullptr;
            ytdlp->deleteLater();
            if (m_finished) return;
            m_finished = true;

            ProbeResult res;
            res.url = url;
            res.finalUrl = url;
            res.isVideo = true;
            res.category = "Videos";

            if (info.success) {
                res.success = true;
                res.videoTitle = !info.title.isEmpty() ? info.title : m_fastTitle;
                res.videoFormats = info.formats;
                res.totalSize = info.totalSize;
                res.supportsRanges = true;
                QString ext = (info.ext.isEmpty() || info.ext == "unknown_video" || info.ext == "image") ? "mp4" : info.ext;
                res.fileName = CategoryRules::sanitizeFilename(res.videoTitle + "." + ext);
            } else {
                res.errorMessage = info.errorMessage;
                if (!m_fastTitle.isEmpty()) {
                    res.success = true;
                    res.videoTitle = m_fastTitle;
                    res.fileName = CategoryRules::sanitizeFilename(m_fastTitle + ".mp4");
                } else {
                    QString vId = extractVideoId(url);
                    if (!vId.isEmpty()) {
                        res.fileName = QString("YouTube_Video_%1.mp4").arg(vId);
                    } else {
                        QString timelapse = QString("Video_%1.mp4").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
                        res.fileName = timelapse;
                    }
                    res.success = false;
                }

                // If yt-dlp couldn't extract formats (e.g. rate limit / network error), provide standard stream presets
                if (res.videoFormats.isEmpty()) {
                    VideoFormatOption f2160; f2160.formatId = "bestvideo[height<=2160]+bestaudio/best[height<=2160]/best"; f2160.label = "4K Ultra HD (2160p)"; f2160.height = 2160; f2160.ext = "mp4";
                    VideoFormatOption f1440; f1440.formatId = "bestvideo[height<=1440]+bestaudio/best[height<=1440]/best"; f1440.label = "2K Quad HD (1440p)"; f1440.height = 1440; f1440.ext = "mp4";
                    VideoFormatOption f1080; f1080.formatId = "bestvideo[height<=1080]+bestaudio/best[height<=1080]/best"; f1080.label = "1080p (Full HD)"; f1080.height = 1080; f1080.ext = "mp4";
                    VideoFormatOption f720; f720.formatId = "bestvideo[height<=720]+bestaudio/best[height<=720]/best"; f720.label = "720p (HD)"; f720.height = 720; f720.ext = "mp4";
                    VideoFormatOption f480; f480.formatId = "bestvideo[height<=480]+bestaudio/best[height<=480]/best"; f480.label = "480p (Standard)"; f480.height = 480; f480.ext = "mp4";
                    VideoFormatOption f360; f360.formatId = "bestvideo[height<=360]+bestaudio/best[height<=360]/best"; f360.label = "360p (Medium)"; f360.height = 360; f360.ext = "mp4";
                    VideoFormatOption f240; f240.formatId = "bestvideo[height<=240]+bestaudio/best[height<=240]/best"; f240.label = "240p (Low)"; f240.height = 240; f240.ext = "mp4";
                    VideoFormatOption faudio; faudio.formatId = "bestaudio/best"; faudio.label = "Audio Only (Best Quality)"; faudio.height = 0; faudio.ext = "m4a";
                    res.videoFormats << f2160 << f1440 << f1080 << f720 << f480 << f360 << f240 << faudio;
                }
            }
            emit probeFinished(res);
        });
        ytdlp->probeVideo(url);
    }

    void sendHeadRequest(const QString& url, int redirectCount) {
        if (m_finished) return;
        if (redirectCount > 6) {
            m_finished = true;
            ProbeResult res;
            res.url = url;
            res.success = false;
            res.errorMessage = "Too many redirects";
            emit probeFinished(res);
            return;
        }

        QNetworkRequest req;
        req.setUrl(QUrl(url));
        req.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/128.0.0.0 Safari/537.36");
        req.setRawHeader("Accept", "*/*");
        req.setRawHeader("Accept-Encoding", "identity");
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        // Force HTTP/1.1 for lightweight probing to eliminate Qt HTTP/2 stream close crashes
        req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

        QNetworkReply* reply = sharedNam()->head(req);
        m_currentReply = reply;
        connect(reply, &QNetworkReply::finished, this, [this, reply, url, redirectCount]() {
            handleHeadReply(reply, url, redirectCount);
        });
    }

    void handleHeadReply(QNetworkReply* reply, const QString& url, int redirectCount) {
        if (m_finished || !reply) return;
        reply->disconnect(this);

        // Check for manual redirect if not automatically followed
        QVariant redirectTarget = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
        if (redirectTarget.isValid() && redirectCount < 6) {
            QUrl redirectUrl = reply->url().resolved(redirectTarget.toUrl());
            reply->deleteLater();
            sendHeadRequest(redirectUrl.toString(), redirectCount + 1);
            return;
        }

        if (reply->error() != QNetworkReply::NoError) {
            reply->deleteLater();
            // Some servers reject HEAD with 405 or 403, fallback to GET range 0-0
            tryRangeGet(url, 0);
            return;
        }

        ProbeResult res = parseReplyHeaders(reply, url);
        reply->deleteLater();
        m_finished = true;
        res.success = true;
        emit probeFinished(res);
    }

    void tryRangeGet(const QString& url, int redirectCount) {
        if (m_finished) return;
        if (redirectCount > 6) {
            m_finished = true;
            ProbeResult res;
            res.url = url;
            res.success = false;
            res.errorMessage = "Too many redirects";
            emit probeFinished(res);
            return;
        }

        QNetworkRequest req;
        req.setUrl(QUrl(url));
        req.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/128.0.0.0 Safari/537.36");
        req.setRawHeader("Accept", "*/*");
        req.setRawHeader("Range", "bytes=0-0");
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        // Force HTTP/1.1 for range probing
        req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

        QNetworkReply* reply = sharedNam()->get(req);
        m_currentReply = reply;
        connect(reply, &QNetworkReply::finished, this, [this, reply, url, redirectCount]() {
            if (m_finished || !reply) return;
            reply->disconnect(this);

            QVariant redirectTarget = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
            if (redirectTarget.isValid() && redirectCount < 6) {
                QUrl redirectUrl = reply->url().resolved(redirectTarget.toUrl());
                reply->deleteLater();
                tryRangeGet(redirectUrl.toString(), redirectCount + 1);
                return;
            }

            if (reply->error() != QNetworkReply::NoError && reply->error() != QNetworkReply::ContentOperationNotPermittedError) {
                QString err = reply->errorString();
                reply->deleteLater();
                m_finished = true;
                ProbeResult res;
                res.url = url;
                res.success = false;
                res.errorMessage = err;
                emit probeFinished(res);
                return;
            }

            ProbeResult res = parseReplyHeaders(reply, url);
            reply->deleteLater();
            m_finished = true;
            res.success = true;
            emit probeFinished(res);
        });
    }

    ProbeResult parseReplyHeaders(QNetworkReply* reply, const QString& url) {
        ProbeResult res;
        res.url = url;
        res.finalUrl = reply->url().toString();
        if (res.finalUrl.isEmpty()) res.finalUrl = url;

        // Content Length
        if (reply->hasRawHeader("Content-Length")) {
            res.totalSize = reply->rawHeader("Content-Length").toULongLong();
        }

        // Content Range: e.g. bytes 0-0/170588782
        if (reply->hasRawHeader("Content-Range")) {
            QString cr = QString::fromUtf8(reply->rawHeader("Content-Range"));
            int slashIdx = cr.lastIndexOf('/');
            if (slashIdx != -1) {
                bool ok = false;
                quint64 sz = cr.mid(slashIdx + 1).trimmed().toULongLong(&ok);
                if (ok && sz > 0) res.totalSize = sz;
            }
        }

        // Accept Ranges
        if (reply->hasRawHeader("Accept-Ranges")) {
            res.supportsRanges = reply->rawHeader("Accept-Ranges").toLower().contains("bytes");
        } else {
            res.supportsRanges = (res.totalSize > 0);
        }

        // MIME type
        if (reply->hasRawHeader("Content-Type")) {
            res.mimeType = QString::fromUtf8(reply->rawHeader("Content-Type")).trimmed();
        }

        // 1. Extract Filename from Content-Disposition
        QString fname;
        if (reply->hasRawHeader("Content-Disposition")) {
            QString cd = QString::fromUtf8(reply->rawHeader("Content-Disposition"));

            // Check RFC 5987 filename*=UTF-8''encoded_name
            int starPos = cd.indexOf("filename*=", 0, Qt::CaseInsensitive);
            if (starPos != -1) {
                QString part = cd.mid(starPos + 10).trimmed();
                int semi = part.indexOf(';');
                if (semi != -1) part = part.left(semi).trimmed();
                if (part.startsWith("UTF-8''", Qt::CaseInsensitive)) {
                    QString enc = part.mid(7);
                    fname = QUrl::fromPercentEncoding(enc.toUtf8());
                } else {
                    fname = part;
                }
            }

            // Fallback to standard filename="..."
            if (fname.isEmpty()) {
                int fnPos = cd.indexOf("filename=", 0, Qt::CaseInsensitive);
                if (fnPos != -1) {
                    fname = cd.mid(fnPos + 9).trimmed();
                    if (fname.startsWith('"')) {
                        int closingQuote = fname.indexOf('"', 1);
                        if (closingQuote != -1) {
                            fname = fname.mid(1, closingQuote - 1);
                        } else {
                            fname = fname.mid(1);
                        }
                    } else if (fname.startsWith('\'')) {
                        int closingQuote = fname.indexOf('\'', 1);
                        if (closingQuote != -1) {
                            fname = fname.mid(1, closingQuote - 1);
                        } else {
                            fname = fname.mid(1);
                        }
                    } else {
                        int semi = fname.indexOf(';');
                        if (semi != -1) fname = fname.left(semi).trimmed();
                    }
                }
            }
        }

        // 2. Extract Filename from final redirected URL path
        if (fname.isEmpty()) {
            QUrl finalQurl(res.finalUrl);
            QString path = finalQurl.path();
            while (path.endsWith('/')) path.chop(1);
            int lastSlash = path.lastIndexOf('/');
            if (lastSlash != -1) {
                fname = path.mid(lastSlash + 1);
            } else {
                fname = path;
            }
            fname = QUrl::fromPercentEncoding(fname.toUtf8());
        }

        // 3. Fallback MIME extension detection
        QFileInfo fi(fname);
        QString currentExt = fi.suffix().toLower();
        bool isGenericExt = currentExt.isEmpty() || currentExt == "bin" || currentExt == "tmp" ||
                            currentExt == "php" || currentExt == "asp" || currentExt == "aspx" ||
                            currentExt == "jsp" || currentExt == "cgi" || currentExt == "html" || currentExt == "htm";

        if (isGenericExt && !res.mimeType.isEmpty()) {
            QString detectedExt = CategoryRules::mimeToExtension(res.mimeType);
            if (!detectedExt.isEmpty()) {
                if (currentExt.isEmpty()) {
                    fname += "." + detectedExt;
                } else if (currentExt == "bin" || currentExt == "tmp") {
                    fname = fi.completeBaseName() + "." + detectedExt;
                }
            }
        }

        if (fname.isEmpty()) {
            fname = "downloaded_file.bin";
        }

        res.fileName = CategoryRules::sanitizeFilename(fname);
        res.category = CategoryRules::categorizeFilename(res.fileName);
        if (res.category == "Videos") {
            res.isVideo = true;
        }
        return res;
    }

    QPointer<QNetworkReply> m_currentReply;
    QPointer<QNetworkReply> m_videoNetworkReply;
    QPointer<YtDlpWrapper> m_currentYtdlp;
    QString m_targetUrl;
    QString m_fastTitle;
    bool m_finished = false;
};
