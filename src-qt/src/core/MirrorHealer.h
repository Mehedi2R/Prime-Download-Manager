#pragma once
#include <QString>
#include <QUrl>
#include <QUrlQuery>

class MirrorHealer {
public:
    static QString healUrl(const QString& rawUrl) {
        QString clean = rawUrl.trimmed();
        if (clean.contains("download.pcfreetime.com")) {
            clean.replace("download.pcfreetime.com", "public.pcfreetime.com");
        }

        // 1. YouTube video playlist stripping: extract genuine single video URL
        if (clean.contains("youtube.com/watch", Qt::CaseInsensitive) && clean.contains("v=")) {
            QUrl qurl(clean);
            QUrlQuery q(qurl);
            if (q.hasQueryItem("v")) {
                QString videoId = q.queryItemValue("v");
                clean = "https://www.youtube.com/watch?v=" + videoId;
            }
        }

        // 2. AnyDesk landing / thank-you page: resolve to authentic installer executable
        if (clean.contains("anydesk.com", Qt::CaseInsensitive) && 
            (clean.contains("thank-you") || clean.contains("dv=win_exe") || clean.contains("downloads/windows"))) {
            clean = "https://download.anydesk.com/AnyDesk.exe";
        }

        // 3. Meta (Instagram & Threads) CDN byte-range chunk stripping:
        // Meta appends bytestart and byteend representing chunk slices.
        // Stripping bytestart and byteend yields the full-length video and enables 206 Partial Content range streaming!
        if (clean.contains("cdninstagram.com", Qt::CaseInsensitive) || clean.contains("fbcdn.net", Qt::CaseInsensitive) || clean.contains("scontent", Qt::CaseInsensitive)) {
            QUrl qurl(clean);
            if (qurl.hasQuery()) {
                QUrlQuery query(qurl);
                if (query.hasQueryItem("bytestart") || query.hasQueryItem("byteend")) {
                    query.removeQueryItem("bytestart");
                    query.removeQueryItem("byteend");
                    qurl.setQuery(query);
                    clean = qurl.toString();
                }
            }
        }

        return clean;
    }
};
