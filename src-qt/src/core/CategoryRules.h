#pragma once
#include <QString>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDateTime>
#include <QSet>
#include <QUrl>
#include <QUrlQuery>

class CategoryRules {
public:
    static QString sanitizeFilename(const QString& rawName) {
        QString clean = rawName;
        static const QRegularExpression illegalChars(R"([<>:"/\\|?*\x00-\x1F])");
        clean.replace(illegalChars, "_");
        clean = clean.trimmed();
        while (clean.startsWith('.') || clean.startsWith(' ')) {
            clean = clean.mid(1);
        }
        while (clean.endsWith('.') || clean.endsWith(' ')) {
            clean.chop(1);
        }
        if (clean.endsWith(".unknown_video", Qt::CaseInsensitive)) {
            clean = clean.left(clean.length() - 14) + ".mp4";
        }
        if (clean.endsWith(".unknown_video.mp4", Qt::CaseInsensitive)) {
            clean = clean.left(clean.length() - 18) + ".mp4";
        }
        if (clean.endsWith(".image", Qt::CaseInsensitive)) {
            clean = clean.left(clean.length() - 6) + ".jpg";
        }
        if (clean.endsWith(".image.mp4", Qt::CaseInsensitive)) {
            clean = clean.left(clean.length() - 10) + ".mp4";
        }
        if (clean.endsWith(".image.jpg", Qt::CaseInsensitive)) {
            clean = clean.left(clean.length() - 10) + ".jpg";
        }
        if (clean.isEmpty()) {
            clean = "download_file.bin";
        }
        // Prevent Windows reserved device names (CON, PRN, AUX, NUL, COM1-9, LPT1-9)
        static const QRegularExpression reservedNameRx(R"(^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\..*)?$)", QRegularExpression::CaseInsensitiveOption);
        if (reservedNameRx.match(clean).hasMatch()) {
            clean = "download_" + clean;
        }
        return clean;
    }

    static QString categorizeFilename(const QString& filename) {
        QString ext = QFileInfo(filename).suffix().toLower();
        if (ext.isEmpty()) return "Others";
        if (ext == "unknown_video" || ext == "image") return "Videos";

        // Programs
        if (ext == "exe" || ext == "msi" || ext == "msix" || ext == "bat" || ext == "cmd" || ext == "apk" || ext == "appx" || ext == "iso" || ext == "img" || ext == "bin" || ext == "run") {
            return "Programs";
        }
        // Compressed
        if (ext == "zip" || ext == "rar" || ext == "7z" || ext == "tar" || ext == "gz" || ext == "bz2" || ext == "xz" || ext == "tgz" || ext == "tbz2" || ext == "cab") {
            return "Compressed";
        }
        // Documents
        if (ext == "pdf" || ext == "doc" || ext == "docx" || ext == "xls" || ext == "xlsx" || ext == "ppt" || ext == "pptx" || ext == "txt" || ext == "csv" || ext == "epub" || ext == "rtf" || ext == "odt") {
            return "Documents";
        }
        // Videos
        if (ext == "mp4" || ext == "mkv" || ext == "webm" || ext == "avi" || ext == "mov" || ext == "flv" || ext == "wmv" || ext == "ts" || ext == "m4v" || ext == "3gp") {
            return "Videos";
        }
        // Music
        if (ext == "mp3" || ext == "m4a" || ext == "wav" || ext == "flac" || ext == "aac" || ext == "ogg" || ext == "wma" || ext == "opus" || ext == "mka") {
            return "Music";
        }
        // Images
        if (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "gif" || ext == "webp" || ext == "bmp" || ext == "svg" || ext == "ico" || ext == "psd" || ext == "ai" || ext == "tif" || ext == "tiff") {
            return "Images";
        }
        return "Others";
    }

    static bool hasValidExtension(const QString& filename) {
        if (filename.isEmpty()) return false;
        QString ext = QFileInfo(filename).suffix().toLower();
        if (ext.isEmpty()) return false;
        static const QSet<QString> invalidOrGenericExts = {
            "php", "asp", "aspx", "jsp", "cgi", "html", "htm", "action", "do", "part", "crdownload"
        };
        return !invalidOrGenericExts.contains(ext);
    }

    static QString extractFilenameFromUrl(const QString& rawUrl) {
        if (rawUrl.isEmpty()) return "";
        QUrl url(rawUrl.trimmed());
        QString path = url.path();
        while (path.endsWith('/')) path.chop(1);
        int lastSlash = path.lastIndexOf('/');
        QString seg = (lastSlash != -1) ? path.mid(lastSlash + 1) : path;
        seg = QUrl::fromPercentEncoding(seg.toUtf8()).trimmed();

        // Check if path segment has a valid extension
        if (hasValidExtension(seg)) {
            return sanitizeFilename(seg);
        }

        // Check common query parameters (e.g. ?file=book.pdf, ?filename=obs.exe, ?name=win64.zip)
        QUrlQuery q(url);
        static const QStringList fileParams = {"filename", "file", "name", "downfile", "attachment", "f"};
        for (const QString& param : fileParams) {
            if (q.hasQueryItem(param)) {
                QString val = q.queryItemValue(param);
                val = QUrl::fromPercentEncoding(val.toUtf8()).trimmed();
                if (hasValidExtension(val)) {
                    return sanitizeFilename(val);
                }
            }
        }

        return seg.isEmpty() ? "" : sanitizeFilename(seg);
    }

    static QString mimeToExtension(const QString& mimeType) {
        QString m = mimeType.toLower();
        if (m.contains("x-msdownload") || m.contains("portable-executable") || m.contains("x-msdos-program") || m.contains("x-executable") || m.contains("octet-stream; type=exe")) return "exe";
        if (m.contains("x-msi") || m.contains("ms-installer")) return "msi";
        if (m.contains("zip")) return "zip";
        if (m.contains("x-7z")) return "7z";
        if (m.contains("x-rar") || m.contains("vnd.rar")) return "rar";
        if (m.contains("x-tar")) return "tar";
        if (m.contains("gzip") || m.contains("x-gzip")) return "gz";
        if (m.contains("pdf")) return "pdf";
        if (m.contains("msword")) return "doc";
        if (m.contains("wordprocessingml")) return "docx";
        if (m.contains("vnd.ms-excel")) return "xls";
        if (m.contains("spreadsheetml")) return "xlsx";
        if (m.contains("presentationml") || m.contains("powerpoint")) return "pptx";
        if (m.contains("text/plain")) return "txt";
        if (m.contains("text/csv")) return "csv";
        if (m.contains("application/epub")) return "epub";
        if (m.contains("video/mp4")) return "mp4";
        if (m.contains("video/webm")) return "webm";
        if (m.contains("video/x-matroska")) return "mkv";
        if (m.contains("video/quicktime")) return "mov";
        if (m.contains("video/x-msvideo")) return "avi";
        if (m.contains("audio/mpeg") || m.contains("audio/mp3")) return "mp3";
        if (m.contains("audio/mp4") || m.contains("audio/aac")) return "m4a";
        if (m.contains("audio/wav") || m.contains("audio/x-wav")) return "wav";
        if (m.contains("audio/flac")) return "flac";
        if (m.contains("audio/ogg") || m.contains("audio/opus")) return "opus";
        if (m.contains("image/jpeg")) return "jpg";
        if (m.contains("image/png")) return "png";
        if (m.contains("image/gif")) return "gif";
        if (m.contains("image/webp")) return "webp";
        if (m.contains("image/svg")) return "svg";
        if (m.contains("application/x-iso9660-image") || m.contains("x-iso-image")) return "iso";
        if (m.contains("android.package-archive")) return "apk";
        if (m.contains("x-apple-diskimage")) return "dmg";
        if (m.contains("x-debian-package")) return "deb";
        if (m.contains("x-redhat-package-manager")) return "rpm";
        return "";
    }

    static QString ensureFilenameWithExtension(
        const QString& filename,
        const QString& url,
        const QString& mimeType = "",
        const QString& defaultCategory = ""
    ) {
        QString name = sanitizeFilename(filename);
        if (hasValidExtension(name)) {
            return name;
        }

        // 1. Try URL filename and extension
        QString urlFn = extractFilenameFromUrl(url);
        QString urlExt = QFileInfo(urlFn).suffix().toLower();
        if (hasValidExtension(urlFn)) {
            // If current name is generic, short (e.g. "OBS", "win64", "download"), or part of urlFn, prefer the authentic urlFn
            if (name.isEmpty() || name == "downloaded_file" || name == "download_file" || name.length() <= 5 || urlFn.contains(name, Qt::CaseInsensitive)) {
                return urlFn;
            } else {
                return name + "." + urlExt;
            }
        }

        // 2. Try MIME type
        if (!mimeType.isEmpty()) {
            QString mExt = mimeToExtension(mimeType);
            if (!mExt.isEmpty()) {
                return name + "." + mExt;
            }
        }

        // 3. Try Category fallback
        QString cat = defaultCategory;
        if (cat.isEmpty() || cat == "Others") {
            cat = categorizeFilename(urlFn);
        }
        if (cat == "Programs") {
            return name + ".exe";
        } else if (cat == "Documents") {
            return name + ".pdf";
        } else if (cat == "Compressed") {
            return name + ".zip";
        } else if (cat == "Videos") {
            return name + ".mp4";
        } else if (cat == "Music") {
            return name + ".m4a";
        } else if (cat == "Images") {
            return name + ".jpg";
        }

        return name;
    }

    static bool isDirectMediaStream(const QString& url) {
        if (url.isEmpty()) return false;
        QString u = url.toLower();
        if (u.contains(".m3u8") || u.contains(".mpd") || u.contains("/manifest")) return false;
        if (u.contains("youtube.com") || u.contains("youtu.be")) return false;
        if (u.contains("/watch") || u.contains("/reel/") || u.contains("/shorts/") || u.contains("/status/") || u.contains("/comments/") || u.contains("story.php") || u.contains("/pin/") || u.contains("pin.it") || u.contains("/post/")) return false;
        
        // Pinterest, Instagram, and Threads video streams are separated/muted DASH in raw MP4, must be processed via yt-dlp & ffmpeg for full audio!
        if (u.contains("pinterest.com") || u.contains("pin.it") || u.contains("pinimg.com") ||
            u.contains("instagram.com") || u.contains("cdninstagram.com") || u.contains("fbcdn.net") ||
            u.contains("threads.net") || u.contains("threads.com")) return false;
        // Direct media files
        if (u.contains(".mp4") || u.contains(".webm") || u.contains(".mkv") || u.contains(".mov") || u.contains(".avi") || u.contains(".flv")) {
            return true;
        }
        return false;
    }

    static bool isVideoPortal(const QString& url) {
        if (url.isEmpty()) return false;
        if (isDirectMediaStream(url)) return false;
        QString u = url.toLower();

        // Exclude web assets, manifests, images, and non-media file extensions from being treated as video portals
        if (u.endsWith(".webmanifest") || u.endsWith(".manifest") || 
            u.endsWith(".json") || u.endsWith(".js") || u.endsWith(".css") || 
            u.endsWith(".ico") || u.endsWith(".png") || u.endsWith(".jpg") || 
            u.endsWith(".jpeg") || u.endsWith(".gif") || u.endsWith(".svg") ||
            u.endsWith(".webp") || u.endsWith(".exe") || u.endsWith(".zip") ||
            u.endsWith(".pdf") || u.endsWith(".xml") || u.endsWith(".woff") ||
            u.endsWith(".woff2") || u.endsWith(".ttf") ||
            u.contains("/manifest") || u.contains("/favicon") || u.contains("/sw.js")) {
            return false;
        }

        return u.contains("youtube.com") || u.contains("youtu.be") ||
               u.contains("facebook.com") || u.contains("fb.watch") ||
               u.contains("instagram.com") ||
                              u.contains("tiktok.com") ||
               u.contains("pinterest.com") || u.contains("pin.it") || u.contains("pinimg.com") ||
               u.contains("twitter.com") || u.contains("x.com") ||
               u.contains("threads.net") || u.contains("threads.com") ||
               u.contains("vimeo.com") || u.contains("dailymotion.com") || u.contains("dai.ly") ||
               u.contains("reddit.com") || u.contains("v.redd.it") ||
               u.contains("twitch.tv") || u.contains("bilibili.com") ||
               u.contains("linkedin.com") || u.contains("tumblr.com") ||
               u.contains(".m3u8") || u.contains(".mpd");
    }

    static bool isGenericTitle(const QString& title) {
        QString t = title.trimmed().toLower();
        if (t.isEmpty()) return true;
        static const QSet<QString> generics = {
            "video", "watch", "facebook", "facebook video", "reel", "reels",
            "post", "media", "stream", "download", "tiktok", "instagram", "youtube",
            "manifest", "video stream", "media_stream", "dailymotion video player", "dailymotion",
            "x", "twitter", "home", "home / x", "home / twitter", "feed", "timeline"
        };
        if (generics.contains(t)) return true;
        if (t.startsWith("video by ") || t.startsWith("post by ") || t.startsWith("reel by ")) return true;
        return false;
    }

    static QString resolveVideoTitle(
        const QString& rawTitle, 
        const QString& description, 
        const QString& uploader = "", 
        const QString& channel = "",
        const QString& url = ""
    ) {
        QString title = rawTitle.trimmed();
        // Remove common portal brand suffixes
        title.remove(QRegularExpression(R"(\s*\|\s*Facebook$)", QRegularExpression::CaseInsensitiveOption));
        title.remove(QRegularExpression(R"(\s*-\s*Facebook$)", QRegularExpression::CaseInsensitiveOption));
        title.remove(QRegularExpression(R"(\s*-\s*YouTube$)", QRegularExpression::CaseInsensitiveOption));
        title.remove(QRegularExpression(R"(\s*\|\s*Dailymotion$)", QRegularExpression::CaseInsensitiveOption));

        // Check if title is actually a page name or uploader name
        bool isPageName = false;
        if (!uploader.isEmpty() && title.compare(uploader.trimmed(), Qt::CaseInsensitive) == 0) isPageName = true;
        if (!channel.isEmpty() && title.compare(channel.trimmed(), Qt::CaseInsensitive) == 0) isPageName = true;

        // Rule 1: Use actual Video Title if present and NOT page name
        if (!title.isEmpty() && !isPageName && !isGenericTitle(title)) {
            return sanitizeFilename(title);
        }

        // Rule 2: If Title not found or is Page Name, use first 30 characters of Description
        QString cleanDesc = description.simplified();
        if (!cleanDesc.isEmpty() && !isGenericTitle(cleanDesc)) {
            if (cleanDesc.length() > 30) {
                cleanDesc = cleanDesc.left(30).trimmed();
            }
            return sanitizeFilename(cleanDesc);
        }

        // Rule 3: Timelapse fallback
        QString timelapse = QString("Video_%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
        return timelapse;
    }

    static QString formatBytes(quint64 bytes) {
        if (bytes == 0) return "0 B";
        const QStringList units = {"B", "KB", "MB", "GB", "TB"};
        int i = 0;
        double sz = static_cast<double>(bytes);
        while (sz >= 1024.0 && i < units.size() - 1) {
            sz /= 1024.0;
            i++;
        }
        return QString("%1 %2").arg(sz, 0, 'f', (i == 0 ? 0 : 2)).arg(units[i]);
    }

    static QString normalizeQualityTag(const QString& rawQuality, int height = 0) {
        if (height > 0) {
            return QString("%1p").arg(height);
        }
        QString q = rawQuality.trimmed();
        if (q.isEmpty()) return "Best";
        if (q.contains("audio", Qt::CaseInsensitive)) {
            return "Audio";
        }
        if (q.contains("4k", Qt::CaseInsensitive) || q.contains("2160", Qt::CaseInsensitive)) {
            return "2160p";
        }
        if (q.contains("2k", Qt::CaseInsensitive) || q.contains("1440", Qt::CaseInsensitive)) {
            return "1440p";
        }
        static const QRegularExpression pRx(R"((\d{3,4})p?)", QRegularExpression::CaseInsensitiveOption);
        auto match = pRx.match(q);
        if (match.hasMatch()) {
            return match.captured(1) + "p";
        }
        if (q.compare("best", Qt::CaseInsensitive) == 0 || q.contains("bestvideo", Qt::CaseInsensitive)) {
            return "Best";
        }
        return q;
    }

    static QString stripQualityTag(const QString& rawBase) {
        QString base = rawBase.trimmed();
        static const QRegularExpression tagRx(
            R"(\s*[\(\[]\s*(?:\d{3,4}p(?:\s*\d+fps)?|audio(?: only)?|best|4k|2k|uhd|hd|sd)\s*[\)\]]$)",
            QRegularExpression::CaseInsensitiveOption
        );
        while (tagRx.match(base).hasMatch()) {
            base.remove(tagRx);
            base = base.trimmed();
        }
        return base;
    }

    static QString applyQualityToFilename(
        const QString& filename, 
        const QString& quality, 
        int height = 0, 
        const QString& forcedExt = ""
    ) {
        QString cleanName = sanitizeFilename(filename);
        QFileInfo fi(cleanName);
        QString ext = forcedExt.isEmpty() ? fi.suffix().toLower() : forcedExt.toLower();
        if (ext.startsWith('.')) ext = ext.mid(1);

        QString base = fi.suffix().isEmpty() ? cleanName : fi.completeBaseName();
        if (base.endsWith(".unknown_video", Qt::CaseInsensitive)) base.chop(14);
        if (base.endsWith(".image", Qt::CaseInsensitive)) base.chop(6);
        base = base.trimmed();

        base = stripQualityTag(base);
        if (base.isEmpty()) {
            base = (ext == "mp3" || ext == "m4a") ? "audio" : "video";
        }

        QString tag = normalizeQualityTag(quality, height);
        if (!tag.isEmpty()) {
            base += QString(" (%1)").arg(tag);
        }

        if (tag.compare("Audio", Qt::CaseInsensitive) == 0) {
            if (ext.isEmpty() || ext == "mp4" || ext == "mkv" || ext == "webm") {
                ext = "m4a";
            }
        } else {
            if (ext.isEmpty() || ext == "unknown_video" || ext == "image") {
                ext = "mp4";
            }
        }

        return ext.isEmpty() ? base : (base + "." + ext);
    }
};
