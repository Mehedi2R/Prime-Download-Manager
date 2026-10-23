#pragma once
#include <QObject>
#include <QThread>
#include <httplib.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QSet>
#include <QList>
#include <future>
#include <memory>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include "../core/DownloadManager.h"
#include "../core/UrlProbe.h"
#include "../media/YtDlpWrapper.h"

class ExtensionServer : public QObject {
    Q_OBJECT
public:
    explicit ExtensionServer(QObject* parent = nullptr) : QObject(parent) {}
    ~ExtensionServer() { stop(); }

    void start(int port = 41245) {
        if (m_running) return;
        m_running = true;
        m_serverThread = std::thread([this, port]() {
#ifdef Q_OS_WIN
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
            m_server = std::make_unique<httplib::Server>();

            // Setup CORS
            m_server->set_default_headers({
                {"Access-Control-Allow-Origin", "*"},
                {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
                {"Access-Control-Allow-Headers", "Content-Type, Range, Authorization, Origin, X-Requested-With"}
            });

            m_server->Options(".*", [](const httplib::Request&, httplib::Response& res) {
                res.status = 200;
            });

            m_server->Get("/health", [](const httplib::Request&, httplib::Response& res) {
                res.set_content(R"({"status":"ok","version":"3.0.46","service":"Prime Download Manager"})", "application/json");
            });

            m_server->Get("/ping", [](const httplib::Request&, httplib::Response& res) {
                res.set_content(R"({"status":"ok","app":"Prime Download Manager","version":"3.0.46"})", "application/json");
            });

            m_server->Post("/log_error", [](const httplib::Request& req, httplib::Response& res) {
                QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromRawData(req.body.data(), static_cast<int>(req.body.size())));
                if (doc.isObject()) {
                    QJsonObject j = doc.object();
                    QString category = j["category"].toString("EXTENSION");
                    QString message = j["message"].toString();
                    QString details;
                    if (j["details"].isObject()) {
                        details = QString::fromUtf8(QJsonDocument(j["details"].toObject()).toJson(QJsonDocument::Compact));
                    } else if (j["details"].isArray()) {
                        details = QString::fromUtf8(QJsonDocument(j["details"].toArray()).toJson(QJsonDocument::Compact));
                    } else {
                        details = j["details"].toString();
                    }
                    DownloadManager::instance().logError(category, message, details);
                }
                res.status = 200;
                res.set_content(R"({"status":"ok"})", "application/json");
            });

            // Endpoint 1: /probe (and legacy /probe-url)
            auto handleProbe = [this](const httplib::Request& req, httplib::Response& res) {
                QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromRawData(req.body.data(), static_cast<int>(req.body.size())));
                if (!doc.isObject()) {
                    res.status = 400;
                    return;
                }
                QJsonObject j = doc.object();
                QString url = j["url"].toString();
                QString webpageUrl = j.contains("webpage_url") ? j["webpage_url"].toString() : j["webpageUrl"].toString();
                // If webpageUrl is a video portal page (YouTube, X, TikTok, Facebook, Pinterest, Instagram, Dailymotion, Tumblr),
                // use webpageUrl for probing so yt-dlp inspects the master resolution ladder (240p to 4K)
                if (!webpageUrl.isEmpty() && CategoryRules::isVideoPortal(webpageUrl)) {
                    QString u = url.toLower();
                    bool isCdnStream = u.contains("googlevideo.com") || u.contains("twimg.com") ||
                                       u.contains("tiktokcdn.com") || u.contains("akamaized.net") ||
                                       u.contains("byteoversea.com") || u.contains("fbcdn.net") ||
                                       u.contains("cdninstagram.com") || u.contains("pinimg.com") ||
                                       u.contains("dmcdn.net") || u.contains("media.tumblr.com") ||
                                       u.contains("vtt.tumblr.com") || u.contains(".m3u8") ||
                                       u.contains(".mpd") || u.startsWith("blob:") || u.startsWith("data:");
                    if (isCdnStream || url.isEmpty() || !CategoryRules::isVideoPortal(url)) {
                        url = webpageUrl;
                    }
                } else if (url.isEmpty()) {
                    url = webpageUrl;
                }
                if (url.isEmpty()) {
                    res.status = 400;
                    res.set_content(R"({"error":"URL is required"})", "application/json");
                    return;
                }

                if (DownloadManager::instance().isUrlBlocked(url)) {
                    res.status = 200;
                    res.set_content(R"({"status":"blocked","reason":"Blocked site or address exception"})", "application/json");
                    return;
                }

                // High-performance probe cache check (TTL: 300 seconds)
                QString cacheKey = url;
                if (url.contains("youtube.com", Qt::CaseInsensitive) || url.contains("youtu.be", Qt::CaseInsensitive)) {
                    static const QRegularExpression ytrx(R"((?:v=|shorts\/|youtu\.be\/)([a-zA-Z0-9_-]{11}))");
                    auto m = ytrx.match(url);
                    if (m.hasMatch()) cacheKey = "yt:" + m.captured(1);
                }

                bool forceFresh = j.value("fresh").toBool(false);
                qint64 nowSec = QDateTime::currentSecsSinceEpoch();
                if (!forceFresh) {
                    std::lock_guard<std::mutex> lock(m_probeCacheMutex);
                    if (m_probeCache.contains(cacheKey)) {
                        const auto& entry = m_probeCache[cacheKey];
                        if (nowSec - entry.timestamp < 300) {
                            res.status = 200;
                            res.set_content(QJsonDocument(entry.data).toJson(QJsonDocument::Compact).toStdString(), "application/json");
                            return;
                        }
                    }
                }

                auto pPromise = std::make_shared<std::promise<QJsonObject>>();
                auto future = pPromise->get_future();
                auto isDone = std::make_shared<std::atomic<bool>>(false);

                QMetaObject::invokeMethod(qApp, [pPromise, isDone, url]() {
                    UrlProbe* probe = new UrlProbe();
                    QObject::connect(probe, &UrlProbe::probeFinished, [pPromise, isDone, probe, url](const ProbeResult& res) {
                        QJsonObject resData;
                        resData["url"] = url;
                        resData["title"] = res.fileName;
                        resData["file_name"] = res.fileName;
                        resData["total_size"] = static_cast<double>(res.totalSize);

                        QJsonArray avQualities;
                        if (!res.videoFormats.isEmpty()) {
                            for (const auto& f : res.videoFormats) {
                                if (f.height > 0 && f.height < 240) continue; // Minimum video quality is 240p
                                QJsonObject qObj;
                                qObj["value"] = (f.height > 0) ? QString("%1p").arg(f.height) : (f.formatId.contains("audio") ? "audio" : "best");
                                qObj["format_id"] = f.formatId;
                                qObj["height"] = f.height;
                                qObj["size"] = static_cast<double>(f.filesize);
                                qObj["label"] = f.label;
                                avQualities.append(qObj);
                            }
                        } else if (res.isVideo) {
                            QJsonObject qObj;
                            qObj["value"] = "best";
                            qObj["format_id"] = "best";
                            qObj["height"] = 0;
                            qObj["size"] = static_cast<double>(res.totalSize);
                            qObj["label"] = (res.totalSize > 0) ? QString("Original Stream (%1)").arg(CategoryRules::formatBytes(res.totalSize)) : "Original Stream";
                            avQualities.append(qObj);
                        }
                        resData["available_qualities"] = avQualities;
                        resData["category"] = res.category;

                        QJsonObject root;
                        root["success"] = res.success;
                        root["url"] = url;
                        root["title"] = res.fileName;
                        root["file_name"] = res.fileName;
                        root["total_size"] = static_cast<double>(res.totalSize);
                        root["available_qualities"] = avQualities;
                        root["category"] = res.category;
                        root["data"] = resData;

                        if (!isDone->exchange(true)) {
                            try {
                                pPromise->set_value(root);
                            } catch (...) {}
                        }
                        probe->deleteLater();
                    });
                    probe->probe(url);
                });

                if (future.wait_for(std::chrono::seconds(32)) == std::future_status::ready) {
                    QJsonObject root = future.get();
                    {
                        std::lock_guard<std::mutex> lock(m_probeCacheMutex);
                        m_probeCache[cacheKey] = { QDateTime::currentSecsSinceEpoch(), root };
                    }
                    res.status = 200;
                    res.set_content(QJsonDocument(root).toJson(QJsonDocument::Compact).toStdString(), "application/json");
                } else {
                    isDone->store(true);
                    // Fast fallback response with valid format options if probe times out
                    QJsonObject root;
                    root["success"] = true;
                    root["url"] = url;
                    QJsonObject resData;
                    resData["url"] = url;

                    QString vTitle = CategoryRules::resolveVideoTitle("", "", "", "", url);
                    QString vFn = vTitle + ".mp4";
                    root["title"] = vFn;
                    root["file_name"] = vFn;
                    root["category"] = "Videos";
                    resData["title"] = vFn;
                    resData["file_name"] = vFn;
                    resData["category"] = "Videos";

                    QJsonArray avQualities;
                    auto addQ = [&](const QString& val, const QString& fId, int h, const QString& lbl) {
                        QJsonObject q;
                        q["value"] = val;
                        q["format_id"] = fId;
                        q["height"] = h;
                        q["size"] = 0;
                        q["label"] = lbl;
                        avQualities.append(q);
                    };
                    addQ("2160p", "bestvideo[height<=2160]+bestaudio/best[height<=2160]/best", 2160, "4K Ultra HD (2160p)");
                    addQ("1440p", "bestvideo[height<=1440]+bestaudio/best[height<=1440]/best", 1440, "2K Quad HD (1440p)");
                    addQ("1080p", "bestvideo[height<=1080]+bestaudio/best[height<=1080]/best", 1080, "Full HD (1080p)");
                    addQ("720p", "bestvideo[height<=720]+bestaudio/best[height<=720]/best", 720, "HD (720p)");
                    addQ("480p", "bestvideo[height<=480]+bestaudio/best[height<=480]/best", 480, "Standard (480p)");
                    addQ("360p", "bestvideo[height<=360]+bestaudio/best[height<=360]/best", 360, "Medium (360p)");
                    addQ("240p", "bestvideo[height<=240]+bestaudio/best[height<=240]/best", 240, "Low (240p)");
                    addQ("audio", "bestaudio/best", 0, "Audio Only (Best Quality)");

                    root["available_qualities"] = avQualities;
                    resData["available_qualities"] = avQualities;
                    root["data"] = resData;
                    res.status = 200;
                    res.set_content(QJsonDocument(root).toJson(QJsonDocument::Compact).toStdString(), "application/json");
                }
            };

            m_server->Post("/probe", handleProbe);
            m_server->Post("/probe-url", handleProbe);

            // Endpoint 2: /download (and legacy /start-download)
            auto handleDownload = [this](const httplib::Request& req, httplib::Response& res) {
                QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromRawData(req.body.data(), static_cast<int>(req.body.size())));
                if (!doc.isObject()) {
                    res.status = 400;
                    return;
                }
                QJsonObject j = doc.object();
                QString url = j["url"].toString();
                if (url.isEmpty() && j.contains("video_url")) {
                    url = j["video_url"].toString();
                }
                QString directServerUrl = j.contains("direct_server_url") ? j["direct_server_url"].toString() : "";
                QString webpageUrl = j.contains("webpage_url") ? j["webpage_url"].toString() : j["webpageUrl"].toString();
                if (!directServerUrl.isEmpty() && !directServerUrl.startsWith("blob:") && !directServerUrl.startsWith("data:")) {
                    url = directServerUrl;
                } else if (url.isEmpty()) {
                    url = webpageUrl;
                }

                QString title = j.contains("title") ? j["title"].toString() : (j.contains("fileName") ? j["fileName"].toString() : j["file_name"].toString());
                QString quality = j.contains("quality") ? j["quality"].toString() : "best";
                QJsonArray qualities = j["qualities"].toArray();
                if (qualities.isEmpty()) {
                    quint64 totalSz = 0;
                    if (j.contains("total_size")) {
                        totalSz = j["total_size"].toVariant().toULongLong();
                    } else if (j.contains("size")) {
                        totalSz = j["size"].toVariant().toULongLong();
                    }
                    if (totalSz > 0) {
                        QJsonObject szObj;
                        szObj["size"] = static_cast<double>(totalSz);
                        szObj["value"] = "best";
                        szObj["height"] = 0;
                        qualities.append(szObj);
                    }
                }
                int threads = j.value("threads").toInt(8);
                bool startNow = j.value("start_now").toBool(false);

                if (j.contains("cookies")) {
                    QString cookies = j["cookies"].toString();
                    if (!cookies.trimmed().isEmpty()) {
                        QString tempDir = QDir::tempPath() + "/PrimeDownloadManager";
                        QDir().mkpath(tempDir);
                        QFile cf(tempDir + "/cookies.txt");
                        if (cf.open(QIODevice::WriteOnly)) {
                            cf.write(cookies.toUtf8());
                        }
                    }
                }

                if (url.isEmpty()) {
                    res.status = 400;
                    res.set_content(R"({"error":"URL is required"})", "application/json");
                    return;
                }

                if (DownloadManager::instance().isUrlBlocked(url, webpageUrl)) {
                    res.status = 200;
                    res.set_content(R"({"status":"blocked","reason":"Blocked site or address exception"})", "application/json");
                    return;
                }

                QString audioUrl = j.contains("audio_url") ? j["audio_url"].toString() : "";
                emit downloadRequestedFull(url, title, quality, qualities, threads, webpageUrl, startNow, audioUrl);

                res.status = 200;
                res.set_content(R"({"success":true,"status":"prompted","task_id":"prompted"})", "application/json");
            };

            m_server->Post("/download", handleDownload);
            m_server->Post("/start-download", handleDownload);
            m_server->Post("/add-download", handleDownload);

            m_server->Get("/config", [](const httplib::Request&, httplib::Response& res) {
                QJsonObject cfg;
                cfg["file_types"] = DownloadManager::instance().fileTypes();
                cfg["blocked_sites"] = DownloadManager::instance().blockedSites();
                QJsonArray arr;
                for (const auto& ex : DownloadManager::instance().addressExceptions()) {
                    arr.append(ex);
                }
                cfg["address_exceptions"] = arr;
                res.set_content(QJsonDocument(cfg).toJson(QJsonDocument::Compact).toStdString(), "application/json");
            });

            qWarning() << "[ExtensionServer] Starting HTTP server on 127.0.0.1:" << port;
            bool ok = m_server->listen("127.0.0.1", port);
            qWarning() << "[ExtensionServer] HTTP server listen finished, returned:" << ok;
        });
    }

    void stop() {
        m_running = false;
        if (m_server) {
            m_server->stop();
        }
        if (m_serverThread.joinable()) {
            m_serverThread.join();
        }
    }

signals:
    void probeRequested(const QString& url);
    void downloadRequested(const QString& url, const QString& fileName, quint64 totalSize, const QString& category, int threads, const QString& webpageUrl);
    void downloadRequestedFull(const QString& url, const QString& title, const QString& quality, const QJsonArray& qualities, int threads, const QString& webpageUrl, bool startNow, const QString& audioUrl = "");

private:
    struct ProbeCacheEntry {
        qint64 timestamp = 0;
        QJsonObject data;
    };
    QMap<QString, ProbeCacheEntry> m_probeCache;
    std::mutex m_probeCacheMutex;

    std::atomic<bool> m_running{false};
    std::unique_ptr<httplib::Server> m_server;
    std::thread m_serverThread;
};
