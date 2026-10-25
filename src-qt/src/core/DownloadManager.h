#pragma once
#include <QObject>
#include <QVector>
#include <QMap>
#include <QMutex>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QUuid>
#include <QTimer>
#include <QThreadPool>
#include "DownloadTask.h"
#include "SegmentWorker.h"
#include "CategoryRules.h"
#include "MirrorHealer.h"
#include "FastStorageAllocator.h"
#include "../media/YtDlpWrapper.h"

class DownloadManager : public QObject {
    Q_OBJECT
public:
    static DownloadManager& instance() {
        static DownloadManager mgr;
        return mgr;
    }

    void init() {
        loadConfig();
        loadTasks();
    }

    void shutdown() {
        for (auto* p : m_muxProcesses) {
            if (p) {
                YtDlpWrapper::directKillProcess(p);
                p->disconnect();
                p->deleteLater();
            }
        }
        m_muxProcesses.clear();

        for (auto* yt : m_videoWorkers) {
            if (yt) {
                yt->stop();
                yt->disconnect();
                yt->deleteLater();
            }
        }
        m_videoWorkers.clear();

        for (auto& workers : m_activeWorkers) {
            for (auto* w : workers) {
                if (w) {
                    w->stop();
                    w->disconnect();
                    w->deleteLater();
                }
            }
        }
        m_activeWorkers.clear();

        for (auto& file : m_taskFiles) {
            if (file && file->isOpen()) {
                file->flush();
                file->close();
            }
        }
        m_taskFiles.clear();
        m_taskMutexes.clear();
        flushSaveTasks();
    }

    const QVector<DownloadTask>& tasks() const { return m_tasks; }
    DownloadTask* findTask(const QString& id) {
        for (auto& t : m_tasks) {
            if (t.id == id) return &t;
        }
        return nullptr;
    }

    QString defaultSavePath() const { return m_defaultSavePath; }
    void setDefaultSavePath(const QString& path) {
        m_defaultSavePath = path;
        saveConfig();
    }

    QString tempPath() const { return m_tempPath.isEmpty() ? (QDir::tempPath() + "/PrimeDownloadManager") : m_tempPath; }
    void setTempPath(const QString& path) {
        m_tempPath = path;
        saveConfig();
    }

    int defaultThreads() const { return m_defaultThreads; }
    void setDefaultThreads(int th) {
        m_defaultThreads = th;
        saveConfig();
    }

    bool autoClipboard() const { return m_autoClipboard; }
    void setAutoClipboard(bool val) {
        m_autoClipboard = val;
        saveConfig();
    }

    bool launchStartup() const { return m_launchStartup; }
    void setLaunchStartup(bool val) {
        m_launchStartup = val;
        saveConfig();
    }

    QString fileTypes() const { return m_fileTypes; }
    void setFileTypes(const QString& ft) {
        m_fileTypes = ft;
        saveConfig();
    }

    QString blockedSites() const { return m_blockedSites; }
    void setBlockedSites(const QString& bs) {
        m_blockedSites = bs;
        saveConfig();
    }

    bool showStartDlg() const { return m_showStartDlg; }
    void setShowStartDlg(bool val) {
        m_showStartDlg = val;
        saveConfig();
    }

    bool queueOnly() const { return m_queueOnly; }
    void setQueueOnly(bool val) {
        m_queueOnly = val;
        saveConfig();
    }

    bool showCompleteDlg() const { return m_showCompleteDlg; }
    void setShowCompleteDlg(bool val) {
        m_showCompleteDlg = val;
        saveConfig();
    }

    bool startImmediately() const { return m_startImmediately; }
    void setStartImmediately(bool val) {
        m_startImmediately = val;
        saveConfig();
    }

    QString duplicateAction() const { return m_duplicateAction; }
    void setDuplicateAction(const QString& da) {
        m_duplicateAction = da;
        saveConfig();
    }

    QString userAgent() const { return m_userAgent; }
    void setUserAgent(const QString& ua) {
        m_userAgent = ua;
        saveConfig();
    }

    static QStringList defaultAddressExceptions() {
        return QStringList{
            "http://*.akamaihd.net/*.mp3",
            "http://*.akamaihd.net/*.zip",
            "http://*.appspot.com/*/audio/*.mp3",
            "http://*.appspot.com/*/Sounds/*",
            "http://*.ask.com/*toolbar/*config*.zip",
            "http://*.cloudfront.net/game/*/res/*.bin",
            "http://*.cloudfront.net/game/*/res/*.zip",
            "http://*.cox.com/*.gz",
            "http://*.download.windowsupdate.com/*",
            "http://*.farmville.com/*.gz",
            "http://*.farmville.com/*/sounds/*.mp3",
            "http://*.gstatic.com/*/sounds/*.mp3",
            "http://*.internetdownloadmanager.com/updates/*/idmupdt.exe*",
            "http://*.meebo.com/*skin/*/sound/*.mp3",
            "http://*.skyrock.com/*.img",
            "http://*.skyrock.net/*.img",
            "http://*.teletalk.com.bd/admitcard/card.php",
            "http://*.vk.com/*.zip",
            "http://*.vkontakte.ru/*.zip",
            "http://*.wikifortio.com/node-fs/upload/*",
            "http://*.zgncdn.com/*.gz",
            "http://*.zgncdn.com/*.mp3",
            "http://*.zgncdn.com/*.z",
            "http://*.zynga.com/*.mp3",
            "http://6rbmasr.egyptany.net/*.ram",
            "http://aa.voice2page.com/*",
            "http://ad.*.com/st?ad_type=*",
            "http://ad.*.net/st?ad_type=*",
            "http://ad.yieldmanager.com/st?*",
            "http://ak.imgfarm.com/images/download/spokesperson/html5/audio/*.mp3",
            "http://api.browser.ya.ru/configs/all_zip?*",
            "http://assets.*.zynga.com/*.zip",
            "http://ccbar.ask.com/images/*.bin",
            "http://cdmdownload.adobe.com/firefox/*primetime*.zip",
            "http://cdn.engine.*/pu.pdf",
            "http://ciscobinary.openh264.org/*.zip",
            "http://counters.gigya.com/wildfire/counters/*=.tif",
            "http://dar.youknowbest.com/Resources/*.img",
            "http://get*.opera.com/pub/opera/*/autoupdate/*.exe",
            "http://get*.opera.com/pub/opera/*/autoupdate/*.msi",
            "http://get.geo.opera.com/res/servicefiles/*.zip",
            "http://images.apple.com/*/*.mov",
            "http://img.imgsmail.ru/*/message.bin",
            "http://img.tfd.com/hm/mp3/*.mp3",
            "http://imgfarm.com/images/*.bin",
            "http://mail.yimg.com/*/i/us/pim/*.mp3",
            "http://movies.apple.com/*/*.mov",
            "http://msedge.b.tlu.dl.delivery.mp.microsoft.com/filestreamingservice/files/*",
            "http://o.aolcdn.com/cdn.webmail.aol.com/*/aol/*/sounds/*.mp3",
            "http://quickaccess-dl.maxthon.com/quickaccess_*.dat*",
            "http://static.ak.fbcdn.net/*.mp3",
            "http://static.ak.fbcdn.net/*.ogg",
            "http://static.playfish.com/game/*.bin",
            "http://statics.verycd.com/javascripts/*.js.r*",
            "http://toolbar.live.com/static/sm/*.bin",
            "http://village*.amazonaws.com/static/sound/*.mp3",
            "http://widget*.meebo.com/*.mp3",
            "http://www.6rb.com/*.ram",
            "http://www.6rbtop.com/library/resources/*.ram",
            "http://www.cbox.ws/box/click*.wav",
            "http://www.nseries.com/nseries/*.mp3",
            "http://www.slimjetbrowser.com/release/*_console.exe",
            "http://www.sonyericsson.com/origin/images/content/*.exe",
            "http://x.myspacecdn.com/modules/webim/static/audio/*.mp3",
            "https://*.akamaihd.net/*.gz",
            "https://*.akamaihd.net/*.mp3",
            "https://*.gstatic.com/*/sounds/*.mp3",
            "https://*.gvt1.com/edgedl/widevine-cdm/*win*.zip?*",
            "https://*.internetdownloadmanager.com/updates/*/idmupdt.exe*",
            "https://*.nic.in/*/*/*.aspx",
            "https://*.nic.in/*/*/*.php",
            "https://ak.imgfarm.com/images/download/spokesperson/html5/audio/*.mp3",
            "https://cdmdownload.adobe.com/firefox/*primetime*.zip",
            "https://cdn.thegameawards.com/frontend/video/tga*.mp4",
            "https://compiles.overleaf.com/project/*/output/output.pdf?*",
            "https://fpdownload.macromedia.com/get/*.z",
            "https://i3j3u3u9.ssl.hwcdn.net/common/sounds/interval.mp3",
            "https://img6.wsimg.com/serpimg/img/solutionsimg/*.mp4",
            "https://lookaside.fbsbx.com/file/*.pdf?token=*",
            "https://pc.xuexi.cn/points/study-login/asset/*.mp3",
            "https://redirector.gvt1.com/edgedl/widevine-cdm/*.zip",
            "https://sso-f.orange.fr/omoi_erb/facture/v1.0/pdf?*",
            "https://swx.cdn.skype.com/assets/*/audio/*.m4a",
            "https://swx.cdn.skype.com/assets/*/audio/*.ogg",
            "https://web.whatsapp.com/res/*.mp3",
            "https://www.bing.com/api/shopping/v1/ux/images/autoApply/*.mp4",
            "https://www.syssel.net/hoefs/software_count.php",
            "https://www.youtube.com/s/search/audio/*.mp3"
        };
    }

    static bool matchWildcard(const QString& pattern, const QString& text) {
        if (pattern.isEmpty() || text.isEmpty()) return false;
        QString rxStr;
        for (int i = 0; i < pattern.length(); ++i) {
            QChar c = pattern[i];
            if (c == '*') {
                rxStr += ".*";
            } else if (c == '?') {
                rxStr += ".";
            } else if (QString(".^$|()[]{}+\\").contains(c)) {
                rxStr += "\\" + QString(c);
            } else {
                rxStr += c;
            }
        }
        QRegularExpression rx("^" + rxStr + "$", QRegularExpression::CaseInsensitiveOption);
        return rx.match(text).hasMatch();
    }

    static bool matchWildcardUrl(const QString& pattern, const QString& url) {
        if (pattern.isEmpty() || url.isEmpty()) return false;
        QString p = pattern.trimmed();
        QString u = url.trimmed();

        // If pattern has no scheme (e.g. *.update.microsoft.com or download.windowsupdate.com)
        if (!p.contains("://")) {
            QUrl qurl(u);
            QString host = qurl.host().isEmpty() ? u : qurl.host();
            QString rxStr;
            for (QChar c : p) {
                if (c == '*') rxStr += ".*";
                else if (c == '?') rxStr += ".";
                else if (QString(".^$|()[]{}+\\").contains(c)) rxStr += "\\" + QString(c);
                else rxStr += c;
            }
            QRegularExpression rx("^" + rxStr + "$", QRegularExpression::CaseInsensitiveOption);
            if (rx.match(host).hasMatch() || rx.match(u).hasMatch()) return true;
        }

        // Full URL match with wildcard
        QString rxStr;
        for (QChar c : p) {
            if (c == '*') rxStr += ".*";
            else if (c == '?') rxStr += ".";
            else if (QString(".^$|()[]{}+\\").contains(c)) rxStr += "\\" + QString(c);
            else rxStr += c;
        }
        if (!p.endsWith("*")) {
            rxStr += "(\\?.*)?";
        }
        QRegularExpression rx("^" + rxStr + "$", QRegularExpression::CaseInsensitiveOption);
        return rx.match(u).hasMatch();
    }

    bool isUrlBlocked(const QString& rawUrl, const QString& webpageUrl = "") const {
        if (rawUrl.isEmpty()) return false;
        QString u = rawUrl.trimmed();
        QString wp = webpageUrl.trimmed();

        // 1. Check Blocked Sites
        QStringList sites = m_blockedSites.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        for (const QString& site : sites) {
            if (matchWildcardUrl(site, u)) return true;
            if (!wp.isEmpty() && matchWildcardUrl(site, wp)) return true;
        }

        // 2. Check Address Exceptions (the 88 addresses)
        for (const QString& exc : m_addressExceptions) {
            if (matchWildcardUrl(exc, u)) return true;
            if (!wp.isEmpty() && matchWildcardUrl(exc, wp)) return true;
        }

        return false;
    }

    QStringList addressExceptions() const { return m_addressExceptions; }
    void setAddressExceptions(const QStringList& list) {
        m_addressExceptions = list;
        saveConfig();
    }

    QString categoryFolder(const QString& category) const {
        if (m_categoryFolders.contains(category) && !m_categoryFolders[category].isEmpty()) {
            return m_categoryFolders.value(category);
        }
        return m_defaultSavePath;
    }

    QString resolveSavePath(const QString& category, const QString& filename, const QString& customDir = "", const QString& rawUrl = "") {
        QString dir = customDir;
        if (dir.isEmpty()) {
            dir = categoryFolder(category);
        }
        if (dir.isEmpty()) {
            dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        }

        QDir qdir(dir);
        if (!qdir.exists()) {
            qdir.mkpath(".");
        }

        QString safeName = CategoryRules::sanitizeFilename(filename);
        if (safeName.endsWith(".unknown_video", Qt::CaseInsensitive)) {
            safeName = safeName.left(safeName.length() - 14) + ".mp4";
        }
        if (safeName.endsWith(".unknown_video.mp4", Qt::CaseInsensitive)) {
            safeName = safeName.left(safeName.length() - 18) + ".mp4";
        }
        if (safeName.endsWith(".image", Qt::CaseInsensitive)) {
            safeName = safeName.left(safeName.length() - 6) + ".mp4";
        }
        if (safeName.endsWith(".image.mp4", Qt::CaseInsensitive)) {
            safeName = safeName.left(safeName.length() - 10) + ".mp4";
        }

        // Guarantee authentic extension
        if (!CategoryRules::hasValidExtension(safeName)) {
            safeName = CategoryRules::ensureFilenameWithExtension(safeName, rawUrl, "", category);
        }

        if (category == "Videos") {
            QString ext = QFileInfo(safeName).suffix().toLower();
            if (ext != "mp4" && ext != "mkv" && ext != "webm" && ext != "avi" && ext != "mov" && ext != "ts" && ext != "flv") {
                if (ext == "image") {
                    safeName = QFileInfo(safeName).completeBaseName() + ".mp4";
                } else {
                    safeName += ".mp4";
                }
            }
        } else if (category == "Music") {
            QString ext = QFileInfo(safeName).suffix().toLower();
            if (ext != "mp3" && ext != "m4a" && ext != "wav" && ext != "flac" && ext != "aac" && ext != "ogg") {
                safeName += ".m4a";
            }
        }
        QString fullPath = qdir.filePath(safeName);

        // Ensure unique filename if already exists
        if (QFile::exists(fullPath)) {
            QFileInfo fi(fullPath);
            QString base = fi.completeBaseName();
            QString ext = fi.suffix();
            if (ext.isEmpty() && category == "Videos") ext = "mp4";
            if (ext.isEmpty() && category == "Music") ext = "m4a";
            int counter = 1;
            while (QFile::exists(fullPath)) {
                QString newName = ext.isEmpty() ? QString("%1 (%2)").arg(base).arg(counter)
                                                : QString("%1 (%2).%3").arg(base).arg(counter).arg(ext);
                fullPath = qdir.filePath(newName);
                counter++;
            }
        }
        return fullPath;
    }

    QString startDownload(
        const QString& rawUrl,
        const QString& fileName,
        quint64 totalSize,
        const QString& category,
        const QString& saveDir = "",
        int threads = 8,
        const QString& webpageUrl = "",
        const QString& formatQuality = "best",
        bool startImmediately = true,
        const QString& audioUrl = "",
        const QString& eTag = "",
        const QString& lastModified = "",
        bool supportsRange = true
    ) {
        if (isUrlBlocked(rawUrl, webpageUrl)) {
            logError("DOWNLOAD:BLOCKED", "Automatic download cancelled: URL matches exclusion list", rawUrl);
            return QString();
        }

        QString healedUrl = MirrorHealer::healUrl(rawUrl);
        if (isUrlBlocked(healedUrl, webpageUrl)) {
            logError("DOWNLOAD:BLOCKED", "Automatic download cancelled: URL matches exclusion list", healedUrl);
            return QString();
        }

        // For video portals with canonical permalinks (Instagram, TikTok, YouTube Shorts, etc.), prefer the canonical webpageUrl over any direct CDN chunk URL
        if (!webpageUrl.isEmpty() && CategoryRules::isVideoPortal(webpageUrl) && 
            (webpageUrl.contains("instagram.com") || webpageUrl.contains("tiktok.com")) &&
            (webpageUrl.contains("/reel/") || webpageUrl.contains("/reels/") || webpageUrl.contains("/p/") || webpageUrl.contains("/video/"))) {
            healedUrl = webpageUrl;
        }

        QString finalName = fileName;
        if (!CategoryRules::hasValidExtension(finalName)) {
            finalName = CategoryRules::ensureFilenameWithExtension(finalName, healedUrl, "", category);
        }

        QString actualCategory = category;
        if (actualCategory.isEmpty() || actualCategory == "Others") {
            QString detected = CategoryRules::categorizeFilename(finalName);
            if (detected != "Others") {
                actualCategory = detected;
            } else if (CategoryRules::isVideoPortal(healedUrl) || CategoryRules::isVideoPortal(webpageUrl)) {
                actualCategory = "Videos";
            }
        }

        bool isDirect = CategoryRules::isDirectMediaStream(healedUrl);
        bool isVid = !isDirect && (CategoryRules::isVideoPortal(healedUrl) || 
                     CategoryRules::isVideoPortal(webpageUrl) || 
                     healedUrl.contains(".m3u8", Qt::CaseInsensitive) || 
                     healedUrl.contains(".mpd", Qt::CaseInsensitive) || 
                     formatQuality.contains("bestvideo", Qt::CaseInsensitive) || 
                     formatQuality.contains("bestaudio", Qt::CaseInsensitive) ||
                     (formatQuality.endsWith("p") || formatQuality.contains("audio")));

        if (actualCategory == "Videos" || actualCategory == "Music" || isVid) {
            QString forcedExt = (actualCategory == "Music" || formatQuality.contains("audio", Qt::CaseInsensitive)) ? "m4a" : "mp4";
            finalName = CategoryRules::applyQualityToFilename(finalName, formatQuality, 0, forcedExt);
        }

        QString savePath = resolveSavePath(actualCategory, finalName, saveDir, healedUrl);

        DownloadTask task;
        task.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        task.url = healedUrl;
        task.fileName = QFileInfo(savePath).fileName();
        task.totalSize = totalSize;
        task.category = actualCategory;
        task.savePath = savePath;
        task.quality = formatQuality.isEmpty() ? "best" : formatQuality;
        task.segmentsCount = qBound(1, threads, 32);
        task.webpageUrl = webpageUrl;
        task.audioUrl = (webpageUrl.contains("instagram.com") || healedUrl.contains("instagram.com")) ? "" : audioUrl;
        task.status = startImmediately ? "Connecting..." : "Paused";
        task.eTag = eTag;
        task.lastModified = lastModified;
        task.supportsRange = supportsRange;

        // Build initial segments for HTTP downloads
        if (!isVid) {
            if (!task.supportsRange) {
                // Non-resumable / no byte ranges -> standard single stream from byte 0
                task.segmentsCount = 1;
                SegmentProgress seg;
                seg.id = 0;
                seg.start = 0;
                seg.end = (totalSize > 0) ? (totalSize - 1) : 0;
                seg.downloaded = 0;
                seg.percent = 0.0;
                seg.speed = 0.0;
                task.segments.append(seg);
            } else {
                // Intelligent thread scaling: Prevents aggressive CDN throttling (Telegram, GitHub, etc.)
                int safeThreads = threads;
                if (totalSize > 0) {
                    if (totalSize < 5 * 1024 * 1024) safeThreads = qMin(safeThreads, 4);
                    else if (totalSize < 30 * 1024 * 1024) safeThreads = qMin(safeThreads, 8);
                    else if (totalSize < 100 * 1024 * 1024) safeThreads = qMin(safeThreads, 16);
                } else {
                    safeThreads = qMin(safeThreads, 8);
                }
                task.segmentsCount = qBound(1, safeThreads, 32);

                if (totalSize > 0) {
                    // Instant zero-fill-free sparse file allocation (<1ms)
                    FastStorageAllocator::allocateFileInstantly(savePath, totalSize);
                    quint64 chunkSize = totalSize / task.segmentsCount;
                    for (int i = 0; i < task.segmentsCount; ++i) {
                        SegmentProgress seg;
                        seg.id = i;
                        seg.start = i * chunkSize;
                        seg.end = (i == task.segmentsCount - 1) ? (totalSize - 1) : ((i + 1) * chunkSize - 1);
                        seg.downloaded = 0;
                        seg.percent = 0.0;
                        seg.speed = 0.0;
                        task.segments.append(seg);
                    }
                } else {
                    SegmentProgress seg;
                    seg.id = 0;
                    seg.start = 0;
                    seg.end = 0;
                    seg.downloaded = 0;
                    task.segments.append(seg);
                }
            }
        }

        m_tasks.prepend(task);
        saveTasks();
        emit taskAdded(task);

        if (startImmediately) {
            runTask(task.id);
        }
        return task.id;
    }

    void pauseDownload(const QString& taskId) {
        DownloadTask* task = findTask(taskId);
        if (!task) return;

        stopWorkers(taskId);
        task->status = "Paused";
        task->speed = 0.0;
        saveTasks();
        emit taskUpdated(*task);
    }

    void stopDownload(const QString& taskId) {
        DownloadTask* task = findTask(taskId);
        if (!task) return;

        stopWorkers(taskId);
        task->status = "Stopped";
        task->speed = 0.0;
        saveTasks();
        emit taskUpdated(*task);
    }

    void resumeDownload(const QString& taskId) {
        DownloadTask* task = findTask(taskId);
        if (!task || task->status == "Completed") return;

        task->url = MirrorHealer::healUrl(task->url);
        task->status = "Connecting...";
        task->speed = 0.0;
        saveTasks();
        emit taskUpdated(*task);
        runTask(taskId);
    }

    void redownload(const QString& taskId) {
        stopWorkers(taskId);
        DownloadTask* task = findTask(taskId);
        if (!task) return;

        QString sp = task->savePath;
        if (QFile::exists(sp)) QFile::remove(sp);
        if (QFile::exists(sp + ".part")) QFile::remove(sp + ".part");
        if (QFile::exists(sp + ".ytdl")) QFile::remove(sp + ".ytdl");

        task->downloadedSize = 0;
        task->customPercent = -1.0;
        task->speed = 0.0;
        task->etaSeconds = 0;
        task->status = "Connecting...";
        for (auto& seg : task->segments) {
            seg.downloaded = 0;
            seg.percent = 0.0;
            seg.speed = 0.0;
        }

        task->url = MirrorHealer::healUrl(task->url);
        saveTasks();
        emit taskUpdated(*task);
        runTask(taskId);
    }

    void removeDownload(const QString& taskId, bool deleteFile = false) {
        stopWorkers(taskId);
        QString targetSavePath;
        for (int i = 0; i < m_tasks.size(); ++i) {
            if (m_tasks[i].id == taskId) {
                targetSavePath = m_tasks[i].savePath;
                m_tasks.removeAt(i);
                break;
            }
        }
        saveTasks();
        emit taskRemoved(taskId);

        if (deleteFile && !targetSavePath.isEmpty()) {
            QString sp = targetSavePath;
            QString tId = taskId;
            QString tPath = tempPath();
            QThreadPool::globalInstance()->start([sp, tId, tPath]() {
                auto safeDelete = [](const QString& filePath) {
                    if (filePath.isEmpty()) return;
                    for (int attempt = 0; attempt < 5; ++attempt) {
                        if (!QFile::exists(filePath)) break;
                        QFile::setPermissions(filePath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                                                        QFile::ReadUser | QFile::WriteUser | QFile::ExeUser);
                        if (QFile::remove(filePath)) {
                            break;
                        }
#ifdef Q_OS_WIN
                        if (DeleteFileW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(filePath).utf16()))) {
                            break;
                        }
#endif
                        QThread::msleep(80);
                    }
                };

                safeDelete(sp);
                safeDelete(sp + ".part");
                safeDelete(sp + ".ytdl");
                safeDelete(sp + ".temp");

                QFileInfo fi(sp);
                QDir parentDir = fi.dir();
                QString base = fi.fileName();
                QString completeBase = fi.completeBaseName();

                // If target was a video/audio download, clean up any associated intermediate streams
                QString targetExt = fi.suffix().toLower();
                if (targetExt == "mp4" || targetExt == "mkv" || targetExt == "webm" || targetExt == "ts") {
                    safeDelete(parentDir.filePath(completeBase + ".m4a"));
                    safeDelete(parentDir.filePath(completeBase + ".mp3"));
                }

                QStringList filters;
                filters << base + "*.part*" << base + "*.temp*" << base + "*.ytdl*"
                        << base + "*.f[0-9]*" << completeBase + "*.f[0-9]*"
                        << completeBase + "*.temp*" << completeBase + "*.aria2*";
                QStringList extraFiles = parentDir.entryList(filters, QDir::Files);
                for (const QString& ef : extraFiles) {
                    safeDelete(parentDir.filePath(ef));
                }

                QDir tempD(tPath);
                QStringList tempFilters;
                tempFilters << tId + "*" << completeBase + "*";
                QStringList tempExtra = tempD.entryList(tempFilters, QDir::Files);
                for (const QString& te : tempExtra) {
                    safeDelete(tempD.filePath(te));
                }
            });
        }
    }

    void logError(const QString& category, const QString& message, const QString& details = "") {
        QString logPath = configDir() + "/logs";
        QDir().mkpath(logPath);
        QFile file(logPath + "/pdm_error.log");
        if (file.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&file);
            QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
            out << "[" << ts << "] [ERROR] [" << category << "] " << message;
            if (!details.isEmpty()) {
                out << " | Details: " << details;
            }
            out << "\n";
        }
    }

    void saveTasks() {
        if (m_saveTasksTimer) {
            m_saveTasksTimer->start();
        } else {
            flushSaveTasks();
        }
    }

    void flushSaveTasks() {
        if (m_saveTasksTimer && m_saveTasksTimer->isActive()) {
            m_saveTasksTimer->stop();
        }
        QJsonArray jArr;
        for (const auto& t : m_tasks) {
            jArr.append(t.toJson());
        }
        QJsonDocument doc(jArr);
        QString targetPath = configDir() + "/tasks.json";
        QString tmpPath = targetPath + ".tmp";
        QFile file(tmpPath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(doc.toJson(QJsonDocument::Indented));
            file.flush();
            file.close();
            if (QFile::exists(targetPath)) {
                QFile::remove(targetPath);
            }
            QFile::rename(tmpPath, targetPath);
        }
    }

signals:
    void taskAdded(const DownloadTask& task);
    void taskUpdated(const DownloadTask& task);
    void taskRemoved(const QString& taskId);
    void taskFinished(const DownloadTask& task);

private:
    DownloadManager() {
        m_defaultSavePath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        m_categoryFolders["Programs"] = m_defaultSavePath + "/Programs";
        m_categoryFolders["Compressed"] = m_defaultSavePath + "/Compressed";
        m_categoryFolders["Documents"] = m_defaultSavePath + "/Documents";
        m_categoryFolders["Videos"] = m_defaultSavePath + "/Videos";
        m_categoryFolders["Music"] = m_defaultSavePath + "/Music";
        m_categoryFolders["Images"] = m_defaultSavePath + "/Images";
        m_categoryFolders["Others"] = m_defaultSavePath + "/Others";
        m_categoryFolders["Downloads"] = m_defaultSavePath;

        m_saveTasksTimer = new QTimer(this);
        m_saveTasksTimer->setSingleShot(true);
        m_saveTasksTimer->setInterval(1000);
        connect(m_saveTasksTimer, &QTimer::timeout, this, &DownloadManager::flushSaveTasks);
    }

    void runTask(const QString& taskId) {
        DownloadTask* task = findTask(taskId);
        if (!task) return;

        stopWorkers(taskId);

        task->status = "Downloading";
        saveTasks();
        emit taskUpdated(*task);

        // 0. For Instagram reels/posts, always use yt-dlp on the canonical URL so the exact video is downloaded with audio
        if ((task->webpageUrl.contains("instagram.com") || task->url.contains("instagram.com")) && 
            (task->webpageUrl.contains("/reel/") || task->webpageUrl.contains("/reels/") || task->webpageUrl.contains("/p/") ||
             task->url.contains("/reel/") || task->url.contains("/reels/") || task->url.contains("/p/"))) {
            if (!task->webpageUrl.isEmpty() && (task->webpageUrl.contains("/reel/") || task->webpageUrl.contains("/reels/") || task->webpageUrl.contains("/p/"))) {
                task->url = task->webpageUrl;
            }
            task->audioUrl = "";
            saveTasks();
            runVideoTask(taskId);
            return;
        }

        // 1. Separate audio stream present (e.g. Meta / Threads / Instagram DASH video + audio) -> Mux using ffmpeg
        bool isDirectVideo = !CategoryRules::isVideoPortal(task->url) && 
                             (CategoryRules::isDirectMediaStream(task->url) || task->url.contains("fbcdn.net") || task->url.contains("cdninstagram.com") || task->url.contains(".mp4") || task->url.contains(".m3u8"));

        if (!task->audioUrl.isEmpty() && isDirectVideo) {
            runMuxTask(taskId);
            return;
        }

        // Check if video portal download
        bool isDirect = CategoryRules::isDirectMediaStream(task->url);
        bool isVid = !isDirect && (CategoryRules::isVideoPortal(task->url) || 
                     CategoryRules::isVideoPortal(task->webpageUrl) || 
                     task->url.contains(".m3u8", Qt::CaseInsensitive) || 
                     task->url.contains(".mpd", Qt::CaseInsensitive) || 
                     task->quality.contains("bestvideo", Qt::CaseInsensitive) || 
                     task->quality.contains("bestaudio", Qt::CaseInsensitive) ||
                     (task->category == "Videos" && (task->quality.endsWith("p") || task->quality.contains("audio"))));

        if (isVid) {
            runVideoTask(taskId);
            return;
        }

        if (!task->audioUrl.isEmpty()) {
            runMuxTask(taskId);
            return;
        }

        // Standard HTTP segmented download
        QSharedPointer<QMutex> fileMutex = QSharedPointer<QMutex>::create();
        m_taskMutexes[taskId] = fileMutex;

        QFileInfo fi(task->savePath);
        QDir().mkpath(fi.absolutePath());

        QSharedPointer<QFile> sharedFile = QSharedPointer<QFile>::create(task->savePath);
        if (!sharedFile->open(QIODevice::ReadWrite)) {
            task->status = "Error: Cannot open destination file";
            flushSaveTasks();
            emit taskUpdated(*task);
            return;
        }

        if (task->segments.isEmpty()) {
            if (task->totalSize > 0) {
                FastStorageAllocator::allocateFileInstantly(task->savePath, task->totalSize);
                quint64 chunkSize = task->totalSize / task->segmentsCount;
                for (int i = 0; i < task->segmentsCount; ++i) {
                    SegmentProgress seg;
                    seg.id = i;
                    seg.start = i * chunkSize;
                    seg.end = (i == task->segmentsCount - 1) ? (task->totalSize - 1) : ((i + 1) * chunkSize - 1);
                    seg.downloaded = 0;
                    seg.percent = 0.0;
                    seg.speed = 0.0;
                    task->segments.append(seg);
                }
            } else {
                SegmentProgress seg;
                seg.id = 0;
                seg.start = 0;
                seg.end = 0;
                seg.downloaded = 0;
                task->segments.append(seg);
            }
            saveTasks();
        } else if (task->totalSize > 0 && sharedFile->size() < static_cast<qint64>(task->totalSize)) {
            FastStorageAllocator::allocateFileInstantly(task->savePath, task->totalSize);
        }

        m_taskFiles[taskId] = sharedFile;

        QVector<SegmentWorker*> workers;
        for (int i = 0; i < task->segments.size(); ++i) {
            const auto& seg = task->segments[i];
            SegmentWorker* w = new SegmentWorker(
                seg.id, task->url, task->savePath, seg.start, seg.end, seg.downloaded,
                sharedFile, fileMutex, task->eTag, task->lastModified, this
            );

            connect(w, &SegmentWorker::headersDiscovered, this, [this, taskId](const QString& etag, const QString& lmod) {
                DownloadTask* t = findTask(taskId);
                if (t) {
                    if (!etag.isEmpty()) t->eTag = etag;
                    if (!lmod.isEmpty()) t->lastModified = lmod;
                    saveTasks();
                }
            });
            
            connect(w, &SegmentWorker::totalSizeDiscovered, this, [this, taskId](quint64 sz) {
                DownloadTask* t = findTask(taskId);
                if (t && t->totalSize == 0 && sz > 0) {
                    t->totalSize = sz;
                    saveTasks();
                    emit taskUpdated(*t);
                }
            });
            connect(w, &SegmentWorker::progress, this, [this, taskId](int segId, quint64 down, double speed) {
                onSegmentProgress(taskId, segId, down, speed);
            });
            connect(w, &SegmentWorker::finished, this, [this, taskId](int segId, bool ok) {
                onSegmentFinished(taskId, segId, ok);
            });
            connect(w, &SegmentWorker::retrying, this, [this, taskId](int segId, int attempt, int maxAttempts, const QString& reason) {
                Q_UNUSED(reason);
                DownloadTask* t = findTask(taskId);
                if (t && (t->status == "Downloading" || t->status.startsWith("Reconnecting"))) {
                    t->status = QString("Reconnecting (thread %1, try %2/%3)...").arg(segId + 1).arg(attempt).arg(maxAttempts);
                    emit taskUpdated(*t);
                }
            });
            connect(w, &SegmentWorker::error, this, [this, taskId](int segId, const QString& err) {
                onSegmentError(taskId, segId, err);
            });

            workers.append(w);
            w->start();
        }
        m_activeWorkers[taskId] = workers;
    }

    void runMuxTask(const QString& taskId) {
        DownloadTask* task = findTask(taskId);
        if (!task) return;
        task->status = "Downloading...";
        task->downloadedSize = 0;
        emit taskUpdated(*task);

        QString ffmpeg = YtDlpWrapper::findExecutable("ffmpeg");
        if (!QFile::exists(ffmpeg)) {
            task->status = "Error: ffmpeg not found";
            emit taskUpdated(*task);
            return;
        }

        QString tempDir = tempPath();
        QDir().mkpath(tempDir);
        QFileInfo fi(task->savePath);
        QString tempOutputFile = tempDir + "/" + taskId + "_" + fi.fileName();
        if (QFile::exists(tempOutputFile)) {
            QFile::remove(tempOutputFile);
        }

        QProcess* proc = new QProcess(this);
        m_muxProcesses[taskId] = proc;

        auto startFfmpeg = [this, taskId, proc, task, ffmpeg, tempOutputFile](bool retryWithAacEncoding) {
            QString ref = "https://www.instagram.com/";
            if (task->webpageUrl.contains("threads.")) ref = "https://www.threads.net/";
            else if (!task->webpageUrl.isEmpty()) ref = task->webpageUrl;

            QString headers = QString("User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36\r\nReferer: %1\r\nOrigin: %1\r\n").arg(ref);

            QStringList args;
            args << "-y";
            args << "-headers" << headers;
            args << "-i" << task->url;
            args << "-headers" << headers;
            args << "-i" << task->audioUrl;
            if (retryWithAacEncoding) {
                args << "-c:v" << "copy" << "-c:a" << "aac" << "-b:a" << "192k" << "-shortest" << "-movflags" << "+faststart";
            } else {
                args << "-c:v" << "copy" << "-c:a" << "copy" << "-shortest" << "-movflags" << "+faststart";
            }
            args << tempOutputFile;
            proc->start(ffmpeg, args);
#ifdef Q_OS_WIN
            YtDlpWrapper::assignProcessToChildJob(proc->processId());
#endif
        };

        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, taskId, proc, tempOutputFile, startFfmpeg, task](int exitCode, QProcess::ExitStatus) {
            static QSet<QString> muxRetriedTasks;
            if (exitCode != 0 && !muxRetriedTasks.contains(taskId)) {
                muxRetriedTasks.insert(taskId);
                if (QFile::exists(tempOutputFile)) QFile::remove(tempOutputFile);
                // Retry with AAC audio transcoding for seamless codec compatibility
                startFfmpeg(true);
                return;
            }
            muxRetriedTasks.remove(taskId);
            proc->deleteLater();
            m_muxProcesses.remove(taskId);
            DownloadTask* t = findTask(taskId);
            if (!t) {
                if (QFile::exists(tempOutputFile)) QFile::remove(tempOutputFile);
                return;
            }
            if (exitCode == 0 && QFile::exists(tempOutputFile) && QFileInfo(tempOutputFile).size() > 1024) {
                QFileInfo destFi(t->savePath);
                QDir().mkpath(destFi.absolutePath());
                if (QFile::exists(t->savePath)) {
                    QFile::remove(t->savePath);
                }
                bool moved = QFile::rename(tempOutputFile, t->savePath);
                if (!moved) {
                    QFile::copy(tempOutputFile, t->savePath);
                    QFile::remove(tempOutputFile);
                }
                t->status = "Completed";
                t->totalSize = QFileInfo(t->savePath).size();
                t->downloadedSize = t->totalSize;
                saveTasks();
                emit taskUpdated(*t);
                emit taskFinished(*t);
            } else {
                if (QFile::exists(tempOutputFile)) QFile::remove(tempOutputFile);
                // Fallback to yt-dlp if portal page exists so download doesn't fail
                if (!task->webpageUrl.isEmpty() && (CategoryRules::isVideoPortal(task->webpageUrl) || CategoryRules::isVideoPortal(task->url))) {
                    runVideoTask(taskId);
                    return;
                }
                t->status = "Error: Stream muxing failed";
                saveTasks();
                emit taskUpdated(*t);
            }
        });

        startFfmpeg(false);
    }

    void runVideoTask(const QString& taskId) {
        DownloadTask* task = findTask(taskId);
        if (!task) return;

        YtDlpWrapper* ytdl = new YtDlpWrapper(this);
        m_videoWorkers[taskId] = ytdl;

        connect(ytdl, &YtDlpWrapper::progress, this, [this, taskId](double pct, quint64 downBytes, double speed, qint64 eta) {
            DownloadTask* t = findTask(taskId);
            if (!t) return;
            if (t->totalSize == 0 && downBytes > 0 && pct > 0) {
                t->totalSize = static_cast<quint64>((downBytes / pct) * 100.0);
            }
            t->downloadedSize = downBytes;
            t->speed = speed;
            t->etaSeconds = eta;
            t->customPercent = pct;
            emit taskUpdated(*t);
        });

        connect(ytdl, &YtDlpWrapper::downloadFinished, this, [this, taskId](bool ok, const QString& err) {
            DownloadTask* t = findTask(taskId);
            if (!t) return;

            stopWorkers(taskId);
            t->speed = 0.0;
            t->etaSeconds = 0;
            if (ok) {
                t->status = "Completed";
                t->customPercent = 100.0;
                if (!QFile::exists(t->savePath)) {
                    QFileInfo fi(t->savePath);
                    QString dir = fi.absolutePath();
                    QString base = fi.completeBaseName();
                    if (base.endsWith(".unknown_video", Qt::CaseInsensitive)) {
                        base.chop(14);
                    }
                    if (base.endsWith(".image", Qt::CaseInsensitive)) {
                        base.chop(6);
                    }
                    if (QFile::exists(dir + "/" + base + ".mp4")) {
                        t->savePath = dir + "/" + base + ".mp4";
                        t->fileName = QFileInfo(t->savePath).fileName();
                    } else if (QFile::exists(dir + "/" + base + ".mkv")) {
                        t->savePath = dir + "/" + base + ".mkv";
                        t->fileName = QFileInfo(t->savePath).fileName();
                    } else if (QFile::exists(dir + "/" + base + ".webm")) {
                        t->savePath = dir + "/" + base + ".webm";
                        t->fileName = QFileInfo(t->savePath).fileName();
                    }
                } else if (t->savePath.endsWith(".unknown_video", Qt::CaseInsensitive)) {
                    QString newPath = t->savePath.left(t->savePath.length() - 14) + ".mp4";
                    QFile::remove(newPath);
                    if (QFile::rename(t->savePath, newPath)) {
                        t->savePath = newPath;
                        t->fileName = QFileInfo(t->savePath).fileName();
                    }
                } else if (t->savePath.endsWith(".image", Qt::CaseInsensitive)) {
                    QString newPath = t->savePath.left(t->savePath.length() - 6) + ".mp4";
                    QFile::remove(newPath);
                    if (QFile::rename(t->savePath, newPath)) {
                        t->savePath = newPath;
                        t->fileName = QFileInfo(t->savePath).fileName();
                    }
                }
                if (QFile::exists(t->savePath)) {
                    t->totalSize = QFileInfo(t->savePath).size();
                    t->downloadedSize = t->totalSize;
                }
                m_videoRetries.remove(taskId);
            } else {
                int retryCount = m_videoRetries.value(taskId, 0);
                if (retryCount < 3) {
                    m_videoRetries[taskId] = retryCount + 1;
                    t->status = QString("Reconnecting video (try %1/3)...").arg(retryCount + 1);
                    t->speed = 0.0;
                    logError("DOWNLOAD:VIDEO", QString("Task %1 video download interrupted, auto-reconnecting attempt %2").arg(taskId).arg(retryCount + 1));
                    saveTasks();
                    emit taskUpdated(*t);
                    QTimer::singleShot(2500, this, [this, taskId]() {
                        runVideoTask(taskId);
                    });
                    return;
                }
                m_videoRetries.remove(taskId);
                QString displayErr = "Error: Video download failed";
                if (!err.isEmpty()) {
                    QString firstLine = err.split('\n').first().trimmed();
                    if (firstLine.contains("ERROR: [youtube] ")) {
                        int col = firstLine.indexOf("] ");
                        if (col != -1) firstLine = firstLine.mid(col + 2);
                    } else if (firstLine.startsWith("ERROR: ")) {
                        firstLine = firstLine.mid(7);
                    }
                    displayErr = "Error: " + firstLine.left(80);
                }
                t->status = displayErr;
                t->customPercent = -1.0;
                logError("DOWNLOAD:VIDEO", QString("Task %1 video download failed: %2").arg(taskId).arg(err), QString("url=%1, savePath=%2").arg(t->url, t->savePath));
            }
            saveTasks();
            emit taskUpdated(*t);
            if (ok) emit taskFinished(*t);
        });

        QString targetUrl = (!task->url.isEmpty() && !task->url.startsWith("blob:") && !task->url.startsWith("data:"))
            ? task->url
            : task->webpageUrl;
        
        // For video portals (YouTube, X, Threads, Instagram, Facebook, Pinterest, TikTok, Dailymotion, Tumblr):
        // If webpageUrl is an authentic portal page and task->url is a CDN stream, blob, or generic:
        // Always pass the authentic webpageUrl to yt-dlp so it accesses the master format ladder,
        // extracts the exact user-selected resolution, and merges audio losslessly!
        if (!task->webpageUrl.isEmpty() && CategoryRules::isVideoPortal(task->webpageUrl)) {
            QString u = task->url.toLower();
            bool isCdnStream = u.contains("googlevideo.com") || u.contains("twimg.com") ||
                               u.contains("tiktokcdn.com") || u.contains("akamaized.net") ||
                               u.contains("byteoversea.com") || u.contains("fbcdn.net") ||
                               u.contains("cdninstagram.com") || u.contains("pinimg.com") ||
                               u.contains("dmcdn.net") || u.contains("media.tumblr.com") ||
                               u.contains("vtt.tumblr.com") || u.contains(".m3u8") ||
                               u.contains(".mpd") || u.startsWith("blob:") || u.startsWith("data:");
            if (isCdnStream || !CategoryRules::isVideoPortal(task->url)) {
                targetUrl = task->webpageUrl;
            }
        }
        if (targetUrl.isEmpty()) targetUrl = task->webpageUrl;
        ytdl->startDownload(targetUrl, task->quality, task->savePath, task->webpageUrl, tempPath());
    }

    void stopWorkers(const QString& taskId) {
        if (m_muxProcesses.contains(taskId)) {
            auto* p = m_muxProcesses.take(taskId);
            if (p) {
                YtDlpWrapper::directKillProcess(p);
                p->disconnect();
                p->deleteLater();
            }
        }
        // Clean up any temp mux file for this taskId
        QString tempDir = tempPath();
        QDir td(tempDir);
        QStringList tempEntries = td.entryList({taskId + "_*"}, QDir::Files);
        for (const QString& te : tempEntries) {
            QFile::remove(td.filePath(te));
        }
        if (m_videoWorkers.contains(taskId)) {
            auto* yt = m_videoWorkers.take(taskId);
            if (yt) {
                yt->stop();
                yt->disconnect();
                yt->deleteLater();
            }
        }
        m_videoRetries.remove(taskId);

        if (m_activeWorkers.contains(taskId)) {
            auto workers = m_activeWorkers.take(taskId);
            for (auto* w : workers) {
                if (w) {
                    w->stop();
                    w->disconnect();
                    w->deleteLater();
                }
            }
        }
        if (m_taskFiles.contains(taskId)) {
            auto file = m_taskFiles.take(taskId);
            if (file) {
                if (file->isOpen()) {
                    file->flush();
                    file->close();
                }
            }
        }
        if (m_taskMutexes.contains(taskId)) {
            m_taskMutexes.remove(taskId);
        }
        m_lastUpdateTimes.remove(taskId);
    }

    void onSegmentProgress(const QString& taskId, int segId, quint64 down, double speed) {
        DownloadTask* task = findTask(taskId);
        if (!task) return;

        if (segId >= 0 && segId < task->segments.size()) {
            task->segments[segId].downloaded = down;
            task->segments[segId].speed = speed;
        }

        quint64 totalDown = 0;
        double totalSpeed = 0.0;
        for (const auto& s : task->segments) {
            totalDown += s.downloaded;
            totalSpeed += s.speed;
        }

        task->downloadedSize = totalDown;
        task->speed = totalSpeed;
        if (task->status.startsWith("Reconnecting")) {
            task->status = "Downloading";
        }
        if (totalSpeed > 0 && task->totalSize > totalDown) {
            task->etaSeconds = static_cast<qint64>((task->totalSize - totalDown) / totalSpeed);
        } else {
            task->etaSeconds = 0;
        }

        // Throttle task update signals to UI to at most once per 100ms per task
        // Eliminates event loop message queue exhaustion, freeze, and lag!
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - m_lastUpdateTimes.value(taskId, 0) >= 100) {
            m_lastUpdateTimes[taskId] = now;
            emit taskUpdated(*task);
        }
    }

    void onSegmentFinished(const QString& taskId, int segId, bool ok) {
        Q_UNUSED(segId);
        Q_UNUSED(ok);
        DownloadTask* task = findTask(taskId);
        if (!task) return;

        bool allCompleted = true;
        if (m_activeWorkers.contains(taskId)) {
            for (auto* w : m_activeWorkers[taskId]) {
                if (!w->isCompleted()) {
                    allCompleted = false;
                    break;
                }
            }
        }

        if (allCompleted) {
            stopWorkers(taskId);
            task->status = "Completed";
            if (QFile::exists(task->savePath)) {
                quint64 actualSize = QFileInfo(task->savePath).size();
                if (actualSize > 0) {
                    task->totalSize = actualSize;
                }
            }
            task->downloadedSize = task->totalSize;
            task->speed = 0.0;
            task->etaSeconds = 0;
            flushSaveTasks();
            emit taskUpdated(*task);
            emit taskFinished(*task);
        }
    }

    void onSegmentError(const QString& taskId, int segId, const QString& err) {
        DownloadTask* task = findTask(taskId);
        if (!task) return;

        stopWorkers(taskId);
        task->status = "Error: " + err;
        task->speed = 0.0;
        logError("DOWNLOAD:HTTP", QString("Task %1 segment %2 error: %3").arg(taskId).arg(segId).arg(err), QString("url=%1").arg(task->url));
        saveTasks();
        emit taskUpdated(*task);
    }

    QString configDir() const {
        QString localApp = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        QDir d(localApp + "/PrimeDownloadManager");
        if (!d.exists()) d.mkpath(".");
        return d.absolutePath();
    }

    void loadConfig() {
        QFile file(configDir() + "/config.json");
        if (!file.open(QIODevice::ReadOnly)) return;

        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isObject()) {
            QJsonObject j = doc.object();
            if (j.contains("default_save_path")) m_defaultSavePath = j["default_save_path"].toString();
            if (j.contains("temp_path")) m_tempPath = j["temp_path"].toString();
            if (j.contains("default_threads")) m_defaultThreads = j["default_threads"].toInt(8);
            if (j.contains("auto_clipboard")) m_autoClipboard = j["auto_clipboard"].toBool(true);
            if (j.contains("launch_startup")) m_launchStartup = j["launch_startup"].toBool(true);
            if (j.contains("file_types")) m_fileTypes = j["file_types"].toString();
            if (j.contains("blocked_sites")) m_blockedSites = j["blocked_sites"].toString();
            if (j.contains("show_start_dlg")) m_showStartDlg = j["show_start_dlg"].toBool(true);
            if (j.contains("queue_only")) m_queueOnly = j["queue_only"].toBool(false);
            if (j.contains("show_complete_dlg")) m_showCompleteDlg = j["show_complete_dlg"].toBool(true);
            if (j.contains("start_immediately")) m_startImmediately = j["start_immediately"].toBool(true);
            if (j.contains("duplicate_action")) m_duplicateAction = j["duplicate_action"].toString();
            if (j.contains("user_agent")) m_userAgent = j["user_agent"].toString();
            if (j.contains("address_exceptions") && j["address_exceptions"].isArray()) {
                m_addressExceptions.clear();
                for (const auto& it : j["address_exceptions"].toArray()) {
                    m_addressExceptions.append(it.toString());
                }
            }
            // Ensure all 88 default address exceptions are present
            const auto defExceptions = defaultAddressExceptions();
            for (const auto& defEx : defExceptions) {
                if (!m_addressExceptions.contains(defEx, Qt::CaseInsensitive)) {
                    m_addressExceptions.append(defEx);
                }
            }
            if (j.contains("category_folders") && j["category_folders"].isObject()) {
                QJsonObject cats = j["category_folders"].toObject();
                for (auto it = cats.begin(); it != cats.end(); ++it) {
                    m_categoryFolders[it.key()] = it.value().toString();
                }
            }
        }
    }

    void saveConfig() {
        QJsonObject j;
        j["default_save_path"] = m_defaultSavePath;
        j["temp_path"] = m_tempPath;
        j["default_threads"] = m_defaultThreads;
        j["auto_clipboard"] = m_autoClipboard;
        j["launch_startup"] = m_launchStartup;
        j["file_types"] = m_fileTypes;
        j["blocked_sites"] = m_blockedSites;
        j["show_start_dlg"] = m_showStartDlg;
        j["queue_only"] = m_queueOnly;
        j["show_complete_dlg"] = m_showCompleteDlg;
        j["start_immediately"] = m_startImmediately;
        j["duplicate_action"] = m_duplicateAction;
        j["user_agent"] = m_userAgent;

        QJsonArray excArr;
        for (const auto& ex : m_addressExceptions) excArr.append(ex);
        j["address_exceptions"] = excArr;

        QJsonObject cats;
        for (auto it = m_categoryFolders.begin(); it != m_categoryFolders.end(); ++it) {
            cats[it.key()] = it.value();
        }
        j["category_folders"] = cats;

        QFile file(configDir() + "/config.json");
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(j).toJson(QJsonDocument::Indented));
        }
    }

    void loadTasks() {
        QFile file(configDir() + "/tasks.json");
        if (!file.open(QIODevice::ReadOnly)) return;

        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isArray()) {
            m_tasks.clear();
            for (const auto& val : doc.array()) {
                DownloadTask task = DownloadTask::fromJson(val.toObject());
                if (task.status == "Downloading" || task.status == "Connecting...") {
                    task.status = "Paused";
                    task.speed = 0.0;
                    task.etaSeconds = 0;
                }
                m_tasks.append(task);
            }
        }
    }

    QVector<DownloadTask> m_tasks;
    QString m_defaultSavePath;
    QString m_tempPath;
    int m_defaultThreads = 8;
    bool m_autoClipboard = true;
    bool m_launchStartup = true;
    QString m_fileTypes = "3GP 7Z AAC ACE AIF APK ARJ ASF AVI BIN BZ2 EXE GZ GZIP IMG ISO LZH M4A M4V MKV MOV MP3 MP4 MPA MPE MPEG MPG MSI MSU OGG OGV PDF PLJ PPS PPT QT R0* R1* RA RAR RM RMVB SEA SIT SITX TAR TIF TIFF WAV WMA WMV Z ZIP";
    QString m_blockedSites = "*.update.microsoft.com download.windowsupdate.com *.download.windowsupdate.com siteseal.thawte.com ecom.cimetz.com *.voice2page.com";
    bool m_showStartDlg = true;
    bool m_queueOnly = false;
    bool m_showCompleteDlg = true;
    bool m_startImmediately = true;
    QString m_duplicateAction = "Show a dialog and ask what to do.";
    QString m_userAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";
    QStringList m_addressExceptions = defaultAddressExceptions();
    QMap<QString, QString> m_categoryFolders;
    QMap<QString, QVector<SegmentWorker*>> m_activeWorkers;
    QMap<QString, YtDlpWrapper*> m_videoWorkers;
    QMap<QString, int> m_videoRetries;
    QMap<QString, QProcess*> m_muxProcesses;
    QMap<QString, QSharedPointer<QMutex>> m_taskMutexes;
    QMap<QString, QSharedPointer<QFile>> m_taskFiles;
    QMap<QString, qint64> m_lastUpdateTimes;
    QTimer* m_saveTasksTimer = nullptr;
};
