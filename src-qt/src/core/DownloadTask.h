#pragma once
#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QVariant>

struct SegmentProgress {
    int id = 0;
    quint64 start = 0;
    quint64 end = 0;
    quint64 downloaded = 0;
    double percent = 0.0;
    double speed = 0.0;
};

class DownloadTask {
public:
    QString id;
    QString url;
    QString fileName;
    quint64 totalSize = 0;
    quint64 downloadedSize = 0;
    double speed = 0.0;
    qint64 etaSeconds = 0;
    QString status = "Paused"; // "Downloading", "Paused", "Completed", "Error", "Connecting..."
    int segmentsCount = 32;
    QVector<SegmentProgress> segments;
    QString category = "Downloads";
    QString savePath;
    QString quality = "best";
    QString webpageUrl;
    QString audioUrl;
    QString referer;
    QString description;
    double customPercent = -1.0;

    double progressPercent() const {
        if (status == "Completed") return 100.0;
        if (customPercent >= 0.0) return qBound(0.0, customPercent, 100.0);
        if (totalSize == 0) return 0.0;
        return qBound(0.0, (static_cast<double>(downloadedSize) / static_cast<double>(totalSize)) * 100.0, 100.0);
    }

    QJsonObject toJson() const {
        QJsonObject j;
        j["id"] = id;
        j["url"] = url;
        j["file_name"] = fileName;
        j["total_size"] = static_cast<qint64>(totalSize);
        j["downloaded_size"] = static_cast<qint64>(downloadedSize);
        j["speed"] = speed;
        j["eta_seconds"] = etaSeconds;
        j["status"] = status;
        j["segments_count"] = segmentsCount;
        j["category"] = category;
        j["save_path"] = savePath;
        j["quality"] = quality;
        j["webpage_url"] = webpageUrl;
        j["audio_url"] = audioUrl;
        j["referer"] = referer;
        j["description"] = description;

        QJsonArray segArr;
        for (const auto& s : segments) {
            QJsonObject sj;
            sj["id"] = s.id;
            sj["start"] = static_cast<qint64>(s.start);
            sj["end"] = static_cast<qint64>(s.end);
            sj["downloaded"] = static_cast<qint64>(s.downloaded);
            sj["percent"] = s.percent;
            sj["speed"] = s.speed;
            segArr.append(sj);
        }
        j["segments"] = segArr;
        return j;
    }

    static DownloadTask fromJson(const QJsonObject& j) {
        DownloadTask t;
        t.id = j["id"].toString();
        t.url = j["url"].toString();
        t.fileName = j["file_name"].toString();
        t.totalSize = static_cast<quint64>(j["total_size"].toVariant().toULongLong());
        t.downloadedSize = static_cast<quint64>(j["downloaded_size"].toVariant().toULongLong());
        t.speed = j["speed"].toDouble();
        t.etaSeconds = j["eta_seconds"].toVariant().toLongLong();
        t.status = j["status"].toString("Paused");
        t.segmentsCount = j["segments_count"].toInt(32);
        t.category = j["category"].toString("Downloads");
        t.savePath = j["save_path"].toString();
        t.quality = j["quality"].toString("best");
        t.webpageUrl = j["webpage_url"].toString();
        t.audioUrl = j["audio_url"].toString();
        t.referer = j["referer"].toString();
        t.description = j["description"].toString();

        QJsonArray segArr = j["segments"].toArray();
        for (const auto& val : segArr) {
            QJsonObject sj = val.toObject();
            SegmentProgress s;
            s.id = sj["id"].toInt();
            s.start = static_cast<quint64>(sj["start"].toVariant().toULongLong());
            s.end = static_cast<quint64>(sj["end"].toVariant().toULongLong());
            s.downloaded = static_cast<quint64>(sj["downloaded"].toVariant().toULongLong());
            s.percent = sj["percent"].toDouble();
            s.speed = sj["speed"].toDouble();
            t.segments.append(s);
        }
        return t;
    }
};
