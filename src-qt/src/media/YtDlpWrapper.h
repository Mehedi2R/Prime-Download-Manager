#pragma once
#include <QObject>
#include <QProcess>
#include <QFileInfo>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include "../core/CategoryRules.h"

struct VideoFormatOption {
    QString formatId;
    QString label;      // e.g. "1080p Full HD (MP4) ~80 MB"
    QString ext;        // "mp4", "m4a", "webm", etc.
    quint64 filesize = 0;
    int height = 0;
};

struct VideoProbeInfo {
    QString title;
    QString ext;
    quint64 totalSize = 0;
    QVector<VideoFormatOption> formats;
    bool success = false;
    QString errorMessage;
};

class YtDlpWrapper : public QObject {
    Q_OBJECT
public:
    explicit YtDlpWrapper(QObject* parent = nullptr) : QObject(parent), m_process(nullptr) {}
    ~YtDlpWrapper() {
        stop();
    }

#ifdef Q_OS_WIN
    static HANDLE getGlobalChildJob() {
        static HANDLE s_job = NULL;
        if (!s_job) {
            s_job = CreateJobObjectW(NULL, NULL);
            if (s_job) {
                JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {0};
                jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
                SetInformationJobObject(s_job, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
            }
        }
        return s_job;
    }

    static void assignProcessToChildJob(qint64 pid) {
        if (pid <= 0) return;
        HANDLE hJob = getGlobalChildJob();
        if (!hJob) return;
        HANDLE hProcess = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
        if (hProcess) {
            AssignProcessToJobObject(hJob, hProcess);
            CloseHandle(hProcess);
        }
    }
#endif

    static void directKillProcess(QProcess* proc) {
        if (!proc) return;
#ifdef Q_OS_WIN
        qint64 pid = proc->processId();
        if (pid > 0) {
            HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
            if (hProcess) {
                TerminateProcess(hProcess, 1);
                CloseHandle(hProcess);
            }
        }
#endif
        proc->kill();
    }

    static QString findExecutable(const QString& name) {
        QString appDir = QCoreApplication::applicationDirPath();
        QString localApp = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        QStringList candidates = {
            appDir + "/" + name + ".exe",
            appDir + "/bin/" + name + ".exe",
            appDir + "/../bin/" + name + ".exe",
            appDir + "/../../bin/" + name + ".exe",
            localApp + "/PrimeDownloadManager/bin/" + name + ".exe",
            "D:/Desktop App/Prime Download Manager/src-qt/bin/" + name + ".exe",
            "D:/Desktop App/Prime Download Manager/src-qt/build/Release/bin/" + name + ".exe",
            "C:/ProgramData/chocolatey/bin/" + name + ".exe"
        };
        for (const auto& p : candidates) {
            if (QFile::exists(p)) return QDir::cleanPath(p);
        }
        return name; // Fallback to PATH
    }

    static QString findNodeJs() {
        QString path = QStandardPaths::findExecutable("node");
        if (!path.isEmpty()) return path;
        QStringList candidates = {
            "C:/Program Files/nodejs/node.exe",
            "C:/Program Files (x86)/nodejs/node.exe",
            QDir::homePath() + "/AppData/Roaming/npm/node.exe",
            QDir::homePath() + "/AppData/Local/Programs/node.exe"
        };
        for (const auto& c : candidates) {
            if (QFile::exists(c)) return QDir::cleanPath(c);
        }
        return QString();
    }

    static QString formatSizeStr(quint64 bytes) {
        if (bytes == 0) return "";
        double sz = static_cast<double>(bytes);
        if (sz >= 1024.0 * 1024.0 * 1024.0) {
            return QString(" ~%1 GB").arg(sz / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
        }
        if (sz >= 1024.0 * 1024.0) {
            return QString(" ~%1 MB").arg(sz / (1024.0 * 1024.0), 0, 'f', 1);
        }
        return QString(" ~%1 KB").arg(sz / 1024.0, 0, 'f', 0);
    }

    void stopProbe() {
        if (m_probeProcess) {
            directKillProcess(m_probeProcess);
            m_probeProcess->disconnect(this);
            m_probeProcess->deleteLater();
            m_probeProcess = nullptr;
        }
    }

    void probeVideo(const QString& url, bool retryWithoutCookies = false) {
        stopProbe();
        QString ytdlp = findExecutable("yt-dlp");
        m_probeProcess = new QProcess(this);
        QProcess* proc = m_probeProcess;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("PYTHONIOENCODING", "utf-8");
        proc->setProcessEnvironment(env);
        QStringList args = {"-j", "--no-warnings", "--no-check-certificates", "--no-playlist", "--encoding", "utf-8"};

        QString nodeJs = findNodeJs();
        if (!nodeJs.isEmpty()) {
            args << "--js-runtimes" << ("node:" + nodeJs);
        }



        QString cookiePath = QDir::tempPath() + "/PrimeDownloadManager/cookies.txt";
        bool usedCookies = false;
        if (!retryWithoutCookies && QFile::exists(cookiePath) && QFileInfo(cookiePath).size() > 0) {
            args << "--cookies" << cookiePath;
            usedCookies = true;
        }
        args << url;

        // 10-second safety watchdog for video extraction
        QTimer* probeKillTimer = new QTimer(proc);
        probeKillTimer->setSingleShot(true);
        connect(probeKillTimer, &QTimer::timeout, this, [this]() {
            stopProbe();
        });
        probeKillTimer->start(10000);

        connect(proc, &QProcess::finished, this, [this, proc, url, retryWithoutCookies, usedCookies](int exitCode) {
            if (m_probeProcess == proc) m_probeProcess = nullptr;
            proc->deleteLater();
            VideoProbeInfo info;
            if (exitCode == 0) {
                QByteArray out = proc->readAllStandardOutput();
                QJsonDocument doc = QJsonDocument::fromJson(out);
                if (doc.isObject()) {
                    QJsonObject obj = doc.object();
                    QString rawTitle = obj["title"].toString();
                    if (rawTitle.isEmpty()) rawTitle = obj["fulltitle"].toString();
                    QString uploader = obj["uploader"].toString().trimmed();
                    QString channel = obj["channel"].toString().trimmed();
                    QString description = obj["description"].toString().trimmed();
                    info.title = CategoryRules::resolveVideoTitle(rawTitle, description, uploader, channel, url);
                    QString rawExt = obj["ext"].toString().trimmed().toLower();
                    info.ext = (rawExt.isEmpty() || rawExt == "unknown_video" || rawExt == "image") ? "mp4" : rawExt;
                    
                    if (obj.contains("filesize") && !obj["filesize"].isNull()) {
                        info.totalSize = static_cast<quint64>(obj["filesize"].toVariant().toULongLong());
                    } else if (obj.contains("filesize_approx") && !obj["filesize_approx"].isNull()) {
                        info.totalSize = static_cast<quint64>(obj["filesize_approx"].toVariant().toULongLong());
                    }

                    // 1. Find best audio stream size for accurate combined file size (prefer direct HTTPS)
                    quint64 bestAudioSize = 0;
                    if (obj.contains("formats") && obj["formats"].isArray()) {
                        for (const auto& fVal : obj["formats"].toArray()) {
                            QJsonObject f = fVal.toObject();
                            QString vcodec = f["vcodec"].toString();
                            QString acodec = f["acodec"].toString();
                            QString proto = f["protocol"].toString();
                            if ((vcodec == "none" || vcodec.isEmpty()) && acodec != "none" && !acodec.isEmpty()) {
                                if (proto.contains("m3u8", Qt::CaseInsensitive)) continue; // skip bloated m3u8 audio
                                quint64 aSize = 0;
                                if (f.contains("filesize") && !f["filesize"].isNull()) {
                                    aSize = static_cast<quint64>(f["filesize"].toVariant().toULongLong());
                                } else if (f.contains("filesize_approx") && !f["filesize_approx"].isNull()) {
                                    aSize = static_cast<quint64>(f["filesize_approx"].toVariant().toULongLong());
                                }
                                if (aSize > bestAudioSize) bestAudioSize = aSize;
                            }
                        }
                    }

                    // 2. Map available video streams grouped by standard resolution tier (IDM-Style)
                    // Accurately handles landscape, vertical (Shorts/Reels/TikTok/Pinterest/X), square, and ultrawide
                    struct StreamMeta {
                        int resTier = 0;
                        int height = 0;
                        int width = 0;
                        int fps = 0;
                        quint64 size = 0;
                        QString ext;
                        double tbr = 0.0;
                        bool isDirect = false;
                    };
                    QMap<int, StreamMeta> heightMap; // resTier (2160, 1440, 1080, 720, 480, 360, 240) -> best stream

                    if (obj.contains("formats") && obj["formats"].isArray()) {
                        for (const auto& fVal : obj["formats"].toArray()) {
                            QJsonObject f = fVal.toObject();
                            int h = f["height"].toInt(0);
                            int w = f["width"].toInt(0);
                            if (h <= 0 && w <= 0) continue;

                            // Determine standard resolution tier key (240p to 4K / 8K)
                            int resTier = 0;
                            if (w > 0 && h > 0) {
                                int minDim = qMin(w, h);
                                int maxDim = qMax(w, h);
                                if (minDim >= 2160 || maxDim >= 3840) resTier = 2160;
                                else if (minDim >= 1440 || maxDim >= 2560) resTier = 1440;
                                else if (minDim >= 1080 || maxDim >= 1900) resTier = 1080;
                                else if (minDim >= 720 || maxDim >= 1260) resTier = 720;
                                else if (minDim >= 480 || maxDim >= 840) resTier = 480;
                                else if (minDim >= 360 || maxDim >= 600) resTier = 360;
                                else if (minDim >= 240 || maxDim >= 400) resTier = 240;
                                else resTier = minDim;
                            } else if (h > 0) {
                                if (h >= 2160) resTier = 2160;
                                else if (h >= 1440) resTier = 1440;
                                else if (h >= 1080) resTier = 1080;
                                else if (h >= 720) resTier = 720;
                                else if (h >= 480) resTier = 480;
                                else if (h >= 360) resTier = 360;
                                else if (h >= 240) resTier = 240;
                                else resTier = h;
                            } else {
                                if (w >= 3840) resTier = 2160;
                                else if (w >= 2560) resTier = 1440;
                                else if (w >= 1920) resTier = 1080;
                                else if (w >= 1280) resTier = 720;
                                else if (w >= 854) resTier = 480;
                                else if (w >= 640) resTier = 360;
                                else if (w >= 426) resTier = 240;
                                else resTier = w;
                            }

                            int fps = f["fps"].toInt(0);
                            double tbr = f["tbr"].toDouble(0.0);
                            QString rawFExt = f["ext"].toString().trimmed().toLower();
                            QString fExt = (rawFExt.isEmpty() || rawFExt == "unknown_video" || rawFExt == "image") ? "mp4" : rawFExt;
                            QString proto = f["protocol"].toString();
                            bool isM3u8 = proto.contains("m3u8", Qt::CaseInsensitive);
                            bool isDirect = !isM3u8 && (proto.startsWith("http", Qt::CaseInsensitive) || proto == "https" || proto == "http_dash_segments");

                            quint64 fSize = 0;
                            if (f.contains("filesize") && !f["filesize"].isNull()) {
                                fSize = static_cast<quint64>(f["filesize"].toVariant().toULongLong());
                            } else if (f.contains("filesize_approx") && !f["filesize_approx"].isNull()) {
                                fSize = static_cast<quint64>(f["filesize_approx"].toVariant().toULongLong());
                            }

                            if (!heightMap.contains(resTier)) {
                                StreamMeta sm;
                                sm.resTier = resTier;
                                sm.height = h;
                                sm.width = w;
                                sm.fps = fps;
                                sm.size = fSize;
                                sm.ext = fExt;
                                sm.tbr = tbr;
                                sm.isDirect = isDirect;
                                heightMap[resTier] = sm;
                            } else {
                                const auto& cur = heightMap[resTier];
                                bool replace = false;
                                if (!cur.isDirect && isDirect) {
                                    replace = true;
                                } else if (cur.isDirect == isDirect) {
                                    if (tbr > cur.tbr || (cur.tbr == 0.0 && fSize > cur.size)) {
                                        replace = true;
                                    }
                                }
                                if (replace) {
                                    StreamMeta sm;
                                    sm.resTier = resTier;
                                    sm.height = h;
                                    sm.width = w;
                                    sm.fps = fps;
                                    sm.size = fSize;
                                    sm.ext = fExt;
                                    sm.tbr = tbr;
                                    sm.isDirect = isDirect;
                                    heightMap[resTier] = sm;
                                }
                            }
                        }
                    }

                    // Fallback to top-level if formats array has no separated video streams
                    if (heightMap.isEmpty()) {
                        int topH = obj["height"].toInt(0);
                        int topW = obj["width"].toInt(0);
                        int resTier = (topH > 0 && topW > 0) ? qMin(topH, topW) : (topH > 0 ? topH : 720);
                        if (resTier > 0) {
                            StreamMeta sm;
                            sm.resTier = resTier;
                            sm.height = topH;
                            sm.width = topW;
                            sm.fps = obj["fps"].toInt(0);
                            sm.size = info.totalSize;
                            sm.ext = (info.ext.isEmpty() || info.ext == "unknown_video") ? "mp4" : info.ext;
                            heightMap[resTier] = sm;
                        }
                    }

                    // 1. Sorted descending list of all available resolutions (highest resolution first - default)
                    QList<int> sortedHeights = heightMap.keys();
                    std::sort(sortedHeights.begin(), sortedHeights.end(), std::greater<int>());

                    for (int h : sortedHeights) {
                        if (h < 240) continue; // Minimum video quality is 240p
                        const auto& sm = heightMap[h];
                        quint64 combinedSize = sm.size > 0 ? (sm.size + bestAudioSize) : 0;
                        QString fpsTag = (sm.fps >= 50) ? QString(" %1fps").arg(sm.fps) : "";

                        QString tierLabel;
                        if (h >= 4320) tierLabel = QString("8K Ultra HD (%1p%2)").arg(h).arg(fpsTag);
                        else if (h >= 2160) tierLabel = QString("4K Ultra HD (%1p%2)").arg(h).arg(fpsTag);
                        else if (h >= 1440) tierLabel = QString("2K Quad HD (%1p%2)").arg(h).arg(fpsTag);
                        else if (h >= 1080) tierLabel = QString("Full HD (%1p%2)").arg(h).arg(fpsTag);
                        else if (h >= 720) tierLabel = QString("HD (%1p%2)").arg(h).arg(fpsTag);
                        else if (h >= 480) tierLabel = QString("Standard (%1p%2)").arg(h).arg(fpsTag);
                        else if (h >= 360) tierLabel = QString("Medium (%1p%2)").arg(h).arg(fpsTag);
                        else tierLabel = QString("Low (%1p%2)").arg(h).arg(fpsTag);

                        VideoFormatOption opt;
                        opt.height = h;
                        opt.ext = (sm.ext.isEmpty() || sm.ext == "unknown_video") ? "mp4" : sm.ext;
                        opt.filesize = combinedSize;
                        opt.formatId = QString("%1p").arg(h);
                        opt.label = QString("%1%2").arg(tierLabel).arg(formatSizeStr(combinedSize));
                        info.formats.append(opt);
                    }

                    // 2. Best Available (Auto) option
                    quint64 maxSz = info.totalSize;
                    if (maxSz == 0) {
                        for (const auto& sm : heightMap) {
                            if (sm.size > maxSz) maxSz = sm.size;
                        }
                        if (maxSz > 0 && bestAudioSize > 0) maxSz += bestAudioSize;
                    }
                    VideoFormatOption best;
                    best.formatId = "bestvideo+bestaudio/best";
                    best.label = QString("Best Available (Auto)%1").arg(formatSizeStr(maxSz));
                    best.ext = (info.ext.isEmpty() || info.ext == "unknown_video") ? "mp4" : info.ext;
                    best.filesize = maxSz;
                    best.height = 9999;
                    info.formats.append(best);

                    // 3. Audio Only option
                    VideoFormatOption audioOpt;
                    audioOpt.formatId = "bestaudio/best";
                    audioOpt.label = QString("Audio Only (MP3 / M4A)%1").arg(formatSizeStr(bestAudioSize));
                    audioOpt.ext = "m4a";
                    audioOpt.filesize = bestAudioSize;
                    audioOpt.height = 0;
                    info.formats.append(audioOpt);

                    info.success = true;
                    emit videoProbed(info);
                    return;
                }
            }

            // Error parsing - friendly message
            QString rawErr = QString::fromUtf8(proc->readAllStandardError()).trimmed();
            if (usedCookies && !retryWithoutCookies) {
                probeVideo(url, true);
                return;
            }

            if (rawErr.contains("This video is unavailable", Qt::CaseInsensitive)) {
                info.errorMessage = "This video is unavailable or has been removed";
            } else if (rawErr.contains("Sign in to confirm you're not a bot", Qt::CaseInsensitive)) {
                info.errorMessage = "Sign-in required or bot verification triggered on YouTube";
            } else if (rawErr.contains("Private video", Qt::CaseInsensitive)) {
                info.errorMessage = "This video is private and cannot be downloaded";
            } else if (!rawErr.isEmpty()) {
                QString firstLine = rawErr.split('\n').first().trimmed();
                if (firstLine.contains("ERROR: [youtube] ")) {
                    int col = firstLine.indexOf("] ");
                    if (col != -1) firstLine = firstLine.mid(col + 2);
                } else if (firstLine.startsWith("ERROR: ")) {
                    firstLine = firstLine.mid(7);
                }
                info.errorMessage = firstLine;
            } else {
                info.errorMessage = "Unable to fetch video information from server";
            }

            info.success = false;
            emit videoProbed(info);
        });

        connect(proc, &QProcess::started, this, [proc]() {
#ifdef Q_OS_WIN
            assignProcessToChildJob(proc->processId());
#endif
        });

        proc->start(ytdlp, args);
    }

    static void cleanupIntermediateFiles(const QString& dirPath, const QString& baseName, const QString& tempDir = "") {
        QStringList dirs = { dirPath };
        if (!tempDir.isEmpty() && tempDir != dirPath) dirs << tempDir;
        QString defaultTemp = QDir::tempPath() + "/PrimeDownloadManager";
        if (!dirs.contains(defaultTemp)) dirs << defaultTemp;

        for (const QString& dPath : dirs) {
            QDir dir(dPath);
            if (!dir.exists()) continue;
            QStringList nameFilters;
            nameFilters << baseName + "*.f[0-9]*"
                        << baseName + "*.temp*"
                        << baseName + "*.part*"
                        << baseName + "*.ytdl*";
            QFileInfoList list = dir.entryInfoList(nameFilters, QDir::Files);
            for (const auto& f : list) {
                QFile::remove(f.filePath());
            }
        }

        // If a 0-byte or extensionless file exists alongside the merged media file, clean it up
        QDir dir(dirPath);
        QString bareFile = dir.filePath(baseName);
        if (QFile::exists(bareFile)) {
            QFileInfo fi(bareFile);
            if (fi.suffix().isEmpty()) {
                QString mp4File = dir.filePath(baseName + ".mp4");
                QString mkvFile = dir.filePath(baseName + ".mkv");
                QString webmFile = dir.filePath(baseName + ".webm");
                if (QFile::exists(mp4File) || QFile::exists(mkvFile) || QFile::exists(webmFile)) {
                    QFile::remove(bareFile);
                }
            }
        }

        // Clean up leftover .unknown_video if .mp4 exists
        QString unkFile = dir.filePath(baseName + ".unknown_video");
        if (QFile::exists(unkFile) && QFile::exists(dir.filePath(baseName + ".mp4"))) {
            QFile::remove(unkFile);
        }
    }

    void startDownload(const QString& url, const QString& format, const QString& outputPath, const QString& referer = "", const QString& tempDir = "") {
        startDownloadInternal(url, format, outputPath, false, referer, tempDir);
    }

    void startDownloadInternal(const QString& url, const QString& format, const QString& outputPath, bool retryWithoutCookies, const QString& referer = "", const QString& tempDir = "") {
        stop();

        QString ytdlp = findExecutable("yt-dlp");
        QString ffmpeg = findExecutable("ffmpeg");

        m_process = new QProcess(this);
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("PYTHONIOENCODING", "utf-8");
        m_process->setProcessEnvironment(env);
        m_lastStderr.clear();
        QStringList args;
        args << "--no-warnings" << "--no-check-certificates" << "--newline" << "--no-playlist" << "--encoding" << "utf-8";
        args << "--retries" << "10" << "--fragment-retries" << "10" << "--file-access-retries" << "5" << "--retry-sleep" << "2";
        args << "--force-overwrites" << "--no-keep-video";
        args << "--user-agent" << "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36";

        if (!referer.isEmpty()) {
            args << "--referer" << referer;
        }

        if (QFile::exists(ffmpeg)) {
            args << "--ffmpeg-location" << ffmpeg;
            args << "--postprocessor-args" << "ffmpeg:-y";
        }

        QString nodeJs = findNodeJs();
        if (!nodeJs.isEmpty()) {
            args << "--js-runtimes" << ("node:" + nodeJs);
        }



        QString cookiePath = QDir::tempPath() + "/PrimeDownloadManager/cookies.txt";
        bool usedCookies = false;
        if (!retryWithoutCookies && QFile::exists(cookiePath) && QFileInfo(cookiePath).size() > 0) {
            args << "--cookies" << cookiePath;
            usedCookies = true;
        }

        // Lossless stream muxing without quality loss or downscaling
        QFileInfo fi(outputPath);
        QString targetExt = fi.suffix().toLower();
        if (targetExt.isEmpty() || targetExt == "unknown_video") targetExt = "mp4";
        QString dirPath = fi.absolutePath();
        QDir().mkpath(dirPath);
        QString baseName = fi.completeBaseName();
        if (baseName.endsWith(".unknown_video", Qt::CaseInsensitive)) {
            baseName.chop(14);
        }
        if (baseName.isEmpty()) baseName = "video";

        QString effectiveFormat = format;
        if (effectiveFormat == "audio" || effectiveFormat == "bestaudio/best" || targetExt == "mp3" || targetExt == "m4a") {
            // Audio only mode
            args << "-f" << "bestaudio/best";
            if (targetExt == "mp3") {
                args << "-x" << "--audio-format" << "mp3" << "--audio-quality" << "0";
            } else {
                args << "-x" << "--audio-format" << targetExt << "--audio-quality" << "0";
            }
        } else {
            // Video mode with exact format selector (Strict resolution matching without quality loss or downscaling)
            int targetHeight = 0;
            if (effectiveFormat.endsWith("p") && effectiveFormat.left(effectiveFormat.length() - 1).toInt() > 0) {
                targetHeight = effectiveFormat.left(effectiveFormat.length() - 1).toInt();
            } else if (effectiveFormat.contains("4k", Qt::CaseInsensitive) || effectiveFormat.contains("2160", Qt::CaseInsensitive)) {
                targetHeight = 2160;
            } else if (effectiveFormat.contains("2k", Qt::CaseInsensitive) || effectiveFormat.contains("1440", Qt::CaseInsensitive)) {
                targetHeight = 1440;
            } else if (effectiveFormat.contains("1080", Qt::CaseInsensitive)) {
                targetHeight = 1080;
            } else if (effectiveFormat.contains("720", Qt::CaseInsensitive)) {
                targetHeight = 720;
            } else if (effectiveFormat.contains("480", Qt::CaseInsensitive)) {
                targetHeight = 480;
            } else if (effectiveFormat.contains("360", Qt::CaseInsensitive)) {
                targetHeight = 360;
            } else if (effectiveFormat.contains("240", Qt::CaseInsensitive)) {
                targetHeight = 240;
            } else {
                static const QRegularExpression hRx(R"(height(?:<=|=)(\d+))");
                auto match = hRx.match(effectiveFormat);
                if (match.hasMatch()) {
                    targetHeight = match.captured(1).toInt();
                }
            }

            if (targetHeight > 0) {
                int targetWidth = 1920;
                if (targetHeight >= 4320) targetWidth = 7680;
                else if (targetHeight >= 2160) targetWidth = 3840;
                else if (targetHeight >= 1440) targetWidth = 2560;
                else if (targetHeight >= 1080) targetWidth = 1920;
                else if (targetHeight >= 720) targetWidth = 1280;
                else if (targetHeight >= 480) targetWidth = 854;
                else if (targetHeight >= 360) targetWidth = 640;
                else targetWidth = 426;

                // Universal multi-aspect resolution selector: guarantees true resolution across
                // landscape (16:9), vertical (9:16 Shorts/Reels/TikTok/Pinterest/X), square (1:1), and widescreen
                QString fSelector = QString(
                    "bestvideo[height=%1]+bestaudio/"
                    "bestvideo[width=%1]+bestaudio/"
                    "bestvideo[width=%2]+bestaudio/"
                    "bestvideo[height<=%1][width<=%2]+bestaudio/"
                    "bestvideo[width<=%1][height<=%2]+bestaudio/"
                    "bestvideo[height<=%1]+bestaudio/"
                    "bestvideo[width<=%1]+bestaudio/"
                    "bestvideo+bestaudio/best"
                ).arg(targetHeight).arg(targetWidth);

                args << "-f" << fSelector;
                args << "-S" << QString("res:%1,fps,codec:h264:vp9:av1,size,br").arg(targetHeight);
                args << "--format-sort-force";
            } else if (!effectiveFormat.isEmpty() && effectiveFormat != "best") {
                args << "-f" << effectiveFormat;
                args << "-S" << "res,fps,codec:h264:vp9:av1,size,br";
            } else {
                args << "-f" << "bestvideo+bestaudio/best";
                args << "-S" << "res,fps,codec:h264:vp9:av1,size,br";
            }

            // Remux streams lossless via ffmpeg stream copy (-c copy) into targetExt (e.g. mp4)
            args << "--merge-output-format" << targetExt;
            args << "--remux-video" << targetExt;
            args << "--postprocessor-args" << "Merger:-c copy";
            args << "--postprocessor-args" << "VideoRemuxer:-c copy";
        }

        // Isolate all intermediate, part, and fragment files in temporary directory
        QString resolvedTemp = tempDir.isEmpty() ? (QDir::tempPath() + "/PrimeDownloadManager") : tempDir;
        QDir().mkpath(resolvedTemp);
        args << "-P" << QString("temp:%1").arg(resolvedTemp);
        args << "-P" << QString("home:%1").arg(dirPath);
        args << "-o" << QString("%1.%(ext)s").arg(baseName);
        args << url;

        m_stdoutBuffer.clear();
        connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
            if (!m_process) return;
            m_stdoutBuffer += m_process->readAllStandardOutput();
            int idx;
            while ((idx = m_stdoutBuffer.indexOf('\n')) != -1 || (idx = m_stdoutBuffer.indexOf('\r')) != -1) {
                QString line = QString::fromUtf8(m_stdoutBuffer.left(idx)).trimmed();
                m_stdoutBuffer.remove(0, idx + 1);
                if (!line.isEmpty()) {
                    parseProgressLine(line);
                }
            }
        });

        connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
            if (!m_process) return;
            m_lastStderr += QString::fromUtf8(m_process->readAllStandardError());
        });

        connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
            if (err == QProcess::FailedToStart) {
                m_lastStderr += "yt-dlp executable failed to start. Please check permissions.";
            }
        });

        connect(m_process, &QProcess::finished, this, [this, url, format, outputPath, dirPath, baseName, targetExt, usedCookies, retryWithoutCookies, referer, tempDir, resolvedTemp, ffmpeg](int code) {
            bool ok = (code == 0);
            if (!ok && usedCookies && !retryWithoutCookies) {
                // If failed due to expired/rejected cookies, automatically retry cleanly without cookies
                startDownloadInternal(url, format, outputPath, true, referer, tempDir);
                return;
            }

            // 1. Recover and remux if yt-dlp left an .unknown_video or .image file
            QString unknownVideoPath = dirPath + "/" + baseName + ".unknown_video";
            QString imageVideoPath = dirPath + "/" + baseName + ".image";
            QString targetPath = dirPath + "/" + baseName + "." + targetExt;
            QString recoverySource = "";
            if (QFile::exists(unknownVideoPath)) recoverySource = unknownVideoPath;
            else if (QFile::exists(imageVideoPath)) recoverySource = imageVideoPath;

            auto finishCleanupAndEmit = [this, dirPath, baseName, targetPath, outputPath, resolvedTemp, code](bool okResult) {
                // 2. Also check if any file was output with another extension or matching pattern
                if (!QFile::exists(targetPath)) {
                    QDir d(dirPath);
                    QStringList entries = d.entryList({baseName + ".*"}, QDir::Files);
                    for (const QString& entry : entries) {
                        QString fullEntry = d.filePath(entry);
                        if (entry.endsWith(".unknown_video", Qt::CaseInsensitive) || entry.endsWith(".image", Qt::CaseInsensitive)) {
                            QFile::remove(targetPath);
                            QFile::rename(fullEntry, targetPath);
                            okResult = true;
                            break;
                        } else if (!entry.endsWith(".part") && !entry.endsWith(".ytdl") && !entry.endsWith(".temp")) {
                            okResult = true;
                            break;
                        }
                    }
                }

                // Immediately clean up any leftover .f137, .f140, .temp, .part files
                cleanupIntermediateFiles(dirPath, baseName, resolvedTemp);

                // Verify if completed media file exists and is authentic non-corrupt media
                quint64 finalSize = 0;
                if (QFile::exists(targetPath)) finalSize = QFileInfo(targetPath).size();
                else if (QFile::exists(outputPath)) finalSize = QFileInfo(outputPath).size();

                if (code != 0) {
                    if (finalSize < 65536) {
                        okResult = false;
                        if (QFile::exists(targetPath)) QFile::remove(targetPath);
                        if (QFile::exists(outputPath)) QFile::remove(outputPath);
                    } else {
                        okResult = true;
                    }
                } else {
                    okResult = (finalSize > 0);
                }

                QString err = okResult ? "" : m_lastStderr.trimmed();
                if (m_process) {
                    m_process->deleteLater();
                    m_process = nullptr;
                }
                emit downloadFinished(okResult, err);
            };

            if (!recoverySource.isEmpty()) {
                if (QFile::exists(ffmpeg)) {
                    m_ffmpegProc = new QProcess(this);
                    QStringList fArgs = {"-y", "-i", recoverySource, "-c", "copy", targetPath};
                    connect(m_ffmpegProc, &QProcess::started, this, [this]() {
#ifdef Q_OS_WIN
                        if (m_ffmpegProc) assignProcessToChildJob(m_ffmpegProc->processId());
#endif
                    });
                    connect(m_ffmpegProc, &QProcess::finished, this, [this, recoverySource, targetPath, finishCleanupAndEmit](int fExitCode) {
                        if (m_ffmpegProc) {
                            m_ffmpegProc->deleteLater();
                            m_ffmpegProc = nullptr;
                        }
                        bool okRemux = (fExitCode == 0 && QFile::exists(targetPath));
                        if (okRemux) {
                            QFile::remove(recoverySource);
                        } else {
                            QFile::remove(targetPath);
                            QFile::rename(recoverySource, targetPath);
                            okRemux = true;
                        }
                        finishCleanupAndEmit(okRemux);
                    });
                    m_ffmpegProc->start(ffmpeg, fArgs);
                    return;
                } else {
                    QFile::remove(targetPath);
                    QFile::rename(recoverySource, targetPath);
                    finishCleanupAndEmit(true);
                    return;
                }
            }

            finishCleanupAndEmit(ok);
        });

        connect(m_process, &QProcess::started, this, [this]() {
#ifdef Q_OS_WIN
            assignProcessToChildJob(m_process->processId());
#endif
        });

        m_process->start(ytdlp, args);
    }

    void stop() {
        stopProbe();
        if (m_process) {
            directKillProcess(m_process);
            m_process->disconnect(this);
            m_process->deleteLater();
            m_process = nullptr;
        }
        if (m_ffmpegProc) {
            directKillProcess(m_ffmpegProc);
            m_ffmpegProc->disconnect(this);
            m_ffmpegProc->deleteLater();
            m_ffmpegProc = nullptr;
        }
    }

signals:
    void videoProbed(const VideoProbeInfo& info);
    void progress(double percent, quint64 downloadedBytes, double speedBytesPerSec, qint64 etaSeconds);
    void downloadFinished(bool success, const QString& errorMsg = "");

private:
    QString m_lastStderr;

private:
    void parseProgressLine(const QString& line) {
        if (line.contains("[Merger]") || line.contains("Merging formats")) {
            emit progress(99.0, 0, 0, 0);
            return;
        }
        if (!line.startsWith("[download]")) return;

        static const QRegularExpression rx(
            R"(\[download\]\s+([\d.]+)%\s+of\s+~?\s*([\d.]+)\s*([A-Za-z]+)(?:\s+at\s+([\d.]+)\s*([A-Za-z]+)/s)?(?:\s+(?:ETA|in)\s+([0-9:]+))?)"
        );
        auto match = rx.match(line);
        if (match.hasMatch()) {
            double percent = match.captured(1).toDouble();
            double sizeVal = match.captured(2).toDouble();
            QString sizeUnit = match.captured(3).toUpper();

            quint64 totalBytes = parseBytes(sizeVal, sizeUnit);
            quint64 downloaded = static_cast<quint64>(totalBytes * (percent / 100.0));

            double speedBytes = 0.0;
            if (match.captured(4).length() > 0) {
                double speedVal = match.captured(4).toDouble();
                QString speedUnit = match.captured(5).toUpper();
                speedBytes = parseBytes(speedVal, speedUnit);
            }

            qint64 etaSec = 0;
            QString etaStr = match.captured(6);
            if (!etaStr.isEmpty()) {
                QStringList etaParts = etaStr.split(':');
                if (etaParts.size() == 2) {
                    etaSec = etaParts[0].toInt() * 60 + etaParts[1].toInt();
                } else if (etaParts.size() == 3) {
                    etaSec = etaParts[0].toInt() * 3600 + etaParts[1].toInt() * 60 + etaParts[2].toInt();
                }
            }

            emit progress(percent, downloaded, speedBytes, etaSec);
        }
    }

    quint64 parseBytes(double val, const QString& unit) {
        if (unit.startsWith("K")) return static_cast<quint64>(val * 1024.0);
        if (unit.startsWith("M")) return static_cast<quint64>(val * 1024.0 * 1024.0);
        if (unit.startsWith("G")) return static_cast<quint64>(val * 1024.0 * 1024.0 * 1024.0);
        return static_cast<quint64>(val);
    }

    QByteArray m_stdoutBuffer;
    QProcess* m_process = nullptr;
    QProcess* m_probeProcess = nullptr;
    QProcess* m_ffmpegProc = nullptr;
};
