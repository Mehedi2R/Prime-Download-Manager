#pragma once
#include <QObject>
#include <QThread>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QFile>
#include <QMutex>
#include <QUrl>
#include <QElapsedTimer>
#include <QSharedPointer>
#include <QTimer>
#include <QDebug>
#include "MirrorHealer.h"

class SegmentWorker : public QObject {
    Q_OBJECT
public:
    SegmentWorker(int id, const QString& url, const QString& savePath, quint64 start, quint64 end, quint64 downloaded,
                  QSharedPointer<QFile> sharedFile, QSharedPointer<QMutex> fileMutex, QObject* parent = nullptr)
        : QObject(parent), m_id(id), m_url(url), m_savePath(savePath), m_start(start), m_end(end),
          m_downloaded(downloaded), m_sharedFile(sharedFile), m_fileMutex(fileMutex) {}

    ~SegmentWorker() {
        stop();
    }

    int id() const { return m_id; }
    quint64 downloaded() const { return m_downloaded; }
    quint64 segmentSize() const { return (m_end >= m_start) ? (m_end - m_start + 1) : 0; }
    bool isCompleted() const { return m_completed; }

public slots:
    void start() {
        if (m_completed || (m_end > 0 && m_start + m_downloaded > m_end)) {
            m_completed = true;
            emit finished(m_id, true);
            return;
        }

        m_stopped = false;
        if (!m_nam) {
            m_nam = new QNetworkAccessManager(this);
        }

        quint64 curStart = m_start + m_downloaded;
        QNetworkRequest req(QUrl(MirrorHealer::healUrl(m_url)));
        req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false); // HTTP/1.1 for guaranteed crash-free streaming
        req.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36");
        req.setRawHeader("Accept", "*/*");
        req.setRawHeader("Accept-Encoding", "identity");

        QString lowerUrl = m_url.toLower();
        if (lowerUrl.contains("cdninstagram.com") || lowerUrl.contains("instagram.com")) {
            req.setRawHeader("Referer", "https://www.instagram.com/");
            req.setRawHeader("Origin", "https://www.instagram.com");
        } else if (lowerUrl.contains("threads.net") || lowerUrl.contains("threads.com")) {
            req.setRawHeader("Referer", "https://www.threads.net/");
            req.setRawHeader("Origin", "https://www.threads.net");
        } else if (lowerUrl.contains("fbcdn.net") || lowerUrl.contains("facebook.com")) {
            req.setRawHeader("Referer", "https://www.facebook.com/");
            req.setRawHeader("Origin", "https://www.facebook.com");
        }
        
        if (m_end > 0) {
            QString rangeHeader = QString("bytes=%1-%2").arg(curStart).arg(m_end);
            req.setRawHeader("Range", rangeHeader.toUtf8());
        }

        m_speedTimer.start();
        m_progressTimer.start();
        m_speedBytes = 0;
        m_currentSpeed = 0.0;

        if (m_reply) {
            m_reply->disconnect();
            m_reply->deleteLater();
            m_reply = nullptr;
        }

        m_reply = m_nam->get(req);
        connect(m_reply, &QNetworkReply::readyRead, this, &SegmentWorker::onReadyRead);
        connect(m_reply, &QNetworkReply::finished, this, &SegmentWorker::onFinished);
        connect(m_reply, &QNetworkReply::errorOccurred, this, &SegmentWorker::onError);
    }

    void stop() {
        m_stopped = true;
        m_retryCount = 0;
        if (m_reply) {
            m_reply->disconnect();
            m_reply->abort();
            m_reply->deleteLater();
            m_reply = nullptr;
        }
    }

signals:
    void progress(int segmentId, quint64 downloadedBytes, double speedBytesPerSec);
    void finished(int segmentId, bool success);
    void retrying(int segmentId, int attempt, int maxAttempts, const QString& reason);
    void error(int segmentId, const QString& errorString);
    void totalSizeDiscovered(quint64 totalBytes);

private slots:
    void onReadyRead() {
        if (!m_reply || m_stopped) return;

        // Reset retry counter on successful data packet arrival
        m_retryCount = 0;

        if (m_start == 0 && m_reply->hasRawHeader("Content-Length")) {
            quint64 cl = m_reply->rawHeader("Content-Length").toULongLong();
            if (cl > 0) emit totalSizeDiscovered(cl);
        }

        int statusCode = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (statusCode >= 400) {
            QString errStr = QString("HTTP %1 Error").arg(statusCode);
            handleFailure(errStr);
            return;
        }

        if (m_start > 0 && statusCode != 206 && statusCode != 0) {
            if (statusCode == 200) {
                m_completed = true;
                stop();
                emit finished(m_id, true);
                return;
            }
            QString errStr = QString("Server returned HTTP %1 instead of 206 Partial Content").arg(statusCode);
            handleFailure(errStr);
            return;
        }

        QByteArray chunk = m_reply->readAll();
        if (chunk.isEmpty()) return;

        quint64 writePos = m_start + m_downloaded;
        qint64 bytesWritten = 0;
        
        if (m_sharedFile && m_fileMutex) {
            QMutexLocker locker(m_fileMutex.data());
            if (m_sharedFile->isOpen()) {
                if (m_sharedFile->seek(writePos)) {
                    bytesWritten = m_sharedFile->write(chunk);
                }
            }
        }

        if (bytesWritten > 0) {
            m_downloaded += bytesWritten;
            m_speedBytes += bytesWritten;
        } else {
            handleFailure("Failed to write downloaded data to disk");
            return;
        }

        qint64 elapsedSpeed = m_speedTimer.elapsed();
        if (elapsedSpeed >= 500) {
            m_currentSpeed = (static_cast<double>(m_speedBytes) / elapsedSpeed) * 1000.0;
            m_speedBytes = 0;
            m_speedTimer.restart();
        }

        if (m_progressTimer.elapsed() >= 100) {
            m_progressTimer.restart();
            emit progress(m_id, m_downloaded, m_currentSpeed);
        }
    }

    void onFinished() {
        if (!m_reply) return;
        auto err = m_reply->error();
        QString errStr = m_reply->errorString();
        m_reply->deleteLater();
        m_reply = nullptr;

        if (m_stopped || m_completed) return;

        if (err != QNetworkReply::NoError) {
            handleFailure(errStr);
            return;
        }

        if (m_end > 0 && (m_start + m_downloaded <= m_end)) {
            handleFailure(QString("Download incomplete: segment received %1 bytes of %2").arg(m_downloaded).arg(segmentSize()));
            return;
        }

        m_completed = true;
        emit progress(m_id, m_downloaded, 0.0);
        emit finished(m_id, true);
    }

    void onError(QNetworkReply::NetworkError code) {
        Q_UNUSED(code);
        if (m_stopped || m_completed) return;
        QString errStr = m_reply ? m_reply->errorString() : "Network error";
        handleFailure(errStr);
    }

private:
    void handleFailure(const QString& reason) {
        if (m_stopped || m_completed) return;

        if (m_reply) {
            m_reply->disconnect();
            m_reply->deleteLater();
            m_reply = nullptr;
        }

        if (m_retryCount < MAX_RETRIES) {
            m_retryCount++;
            int delayMs = qMin(1000 * m_retryCount, 5000);
            qWarning() << "[SegmentWorker" << m_id << "] Interrupted (" << reason 
                       << "), auto-reconnecting attempt" << m_retryCount << "of" << MAX_RETRIES << "in" << delayMs << "ms";
            emit retrying(m_id, m_retryCount, MAX_RETRIES, reason);
            QTimer::singleShot(delayMs, this, [this]() {
                if (!m_stopped && !m_completed) {
                    start();
                }
            });
            return;
        }

        m_stopped = true;
        emit error(m_id, reason);
    }

    int m_id = 0;
    QString m_url;
    QString m_savePath;
    quint64 m_start = 0;
    quint64 m_end = 0;
    quint64 m_downloaded = 0;
    QSharedPointer<QFile> m_sharedFile;
    QSharedPointer<QMutex> m_fileMutex;
    QNetworkAccessManager* m_nam = nullptr;
    QNetworkReply* m_reply = nullptr;
    bool m_stopped = false;
    bool m_completed = false;
    int m_retryCount = 0;
    static const int MAX_RETRIES = 10;
    QElapsedTimer m_speedTimer;
    QElapsedTimer m_progressTimer;
    quint64 m_speedBytes = 0;
    double m_currentSpeed = 0.0;
};