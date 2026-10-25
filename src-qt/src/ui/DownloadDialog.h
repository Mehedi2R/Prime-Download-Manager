#pragma once
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QApplication>
#include <QClipboard>
#include <QFileInfo>
#include <QDir>
#include <QFrame>
#include <QMouseEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QPainter>
#include <QStyleOption>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include "../core/DownloadManager.h"
#include "../core/UrlProbe.h"
#include "../core/CategoryRules.h"

// IDM-Style Step 1: "Enter new address to download" Dialog
class EnterUrlDialog : public QDialog {
    Q_OBJECT
public:
    explicit EnterUrlDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_StyledBackground, true);
        setFixedSize(550, 150);
        setStyleSheet(
            "QDialog { background-color: #0b0f19; color: #f8fafc; font-family: 'Segoe UI', sans-serif; border: 1px solid #1e293b; border-radius: 10px; }"
            "QLabel { color: #94a3b8; font-size: 13px; font-weight: 500; border: none; background: transparent; }"
            "QLineEdit { background-color: #131d2e; color: #f8fafc; border: 1px solid #1e293b; border-radius: 6px; padding: 7px 10px; font-size: 13px; }"
            "QLineEdit:focus { border: 1px solid #00d2ff; }"
            "QPushButton { border-radius: 6px; padding: 6px 18px; font-size: 12px; font-weight: 600; min-width: 80px; }"
        );

        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(18, 14, 18, 14);
        mainLayout->setSpacing(12);

        // Header with title and close.png button
        QHBoxLayout* headerLayout = new QHBoxLayout();
        headerLayout->setContentsMargins(0, 0, 0, 0);
        headerLayout->setSpacing(8);

        QLabel* titleLabel = new QLabel("Enter new address to download", this);
        titleLabel->setStyleSheet("color: #f8fafc; font-size: 13px; font-weight: 600;");

        QPushButton* closeBtn = new QPushButton(this);
        closeBtn->setIcon(QIcon(":/icons/close.png"));
        closeBtn->setIconSize(QSize(18, 18));
        closeBtn->setFixedSize(22, 22);
        closeBtn->setCursor(Qt::PointingHandCursor);
        closeBtn->setToolTip("Close");
        closeBtn->setStyleSheet(
            "QPushButton { background: transparent; border: none; border-radius: 4px; padding: 0px; min-width: 22px; }"
            "QPushButton:hover { background-color: rgba(239, 68, 68, 0.25); }"
            "QPushButton:pressed { background-color: rgba(239, 68, 68, 0.5); }"
        );
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);

        headerLayout->addWidget(titleLabel);
        headerLayout->addStretch();
        headerLayout->addWidget(closeBtn);
        mainLayout->addLayout(headerLayout);

        m_urlEdit = new QLineEdit(this);
        m_urlEdit->setPlaceholderText("https://example.com/file.zip");

        // Auto paste from clipboard if valid URL
        QString clip = QApplication::clipboard()->text().trimmed();
        if (clip.startsWith("http://", Qt::CaseInsensitive) || clip.startsWith("https://", Qt::CaseInsensitive)) {
            m_urlEdit->setText(clip);
            m_urlEdit->selectAll();
        }

        mainLayout->addWidget(m_urlEdit);

        QHBoxLayout* btnLayout = new QHBoxLayout();
        btnLayout->addStretch();

        QPushButton* okBtn = new QPushButton("OK", this);
        okBtn->setStyleSheet("background-color: #00d2ff; color: #000000; border: none; font-weight: bold;");
        okBtn->setDefault(true);
        connect(okBtn, &QPushButton::clicked, this, [this]() {
            if (!m_urlEdit->text().trimmed().isEmpty()) {
                accept();
            }
        });

        QPushButton* cancelBtn = new QPushButton("Cancel", this);
        cancelBtn->setStyleSheet("background-color: #1e293b; color: #94a3b8; border: 1px solid #334155;");
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

        btnLayout->addWidget(okBtn);
        btnLayout->addWidget(cancelBtn);
        mainLayout->addLayout(btnLayout);
    }

    QString url() const { return m_urlEdit->text().trimmed(); }

protected:
    void paintEvent(QPaintEvent*) override {
        QStyleOption opt;
        opt.initFrom(this);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    }

    void showEvent(QShowEvent* event) override {
        QDialog::showEvent(event);
        if (parentWidget()) {
            move(parentWidget()->geometry().center().x() - width() / 2,
                 parentWidget()->geometry().center().y() - height() / 2);
        } else {
            QScreen* screen = QGuiApplication::primaryScreen();
            if (screen) {
                QRect geo = screen->availableGeometry();
                move(geo.center().x() - width() / 2, geo.center().y() - height() / 2);
            }
        }
#ifdef Q_OS_WIN
        HWND hwnd = (HWND)winId();
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        SetForegroundWindow(hwnd);
#endif
    }
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
            event->accept();
        }
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if (event->buttons() & Qt::LeftButton) {
            move(event->globalPosition().toPoint() - m_dragPos);
            event->accept();
        }
    }

private:
    QLineEdit* m_urlEdit = nullptr;
    QPoint m_dragPos;
};

// IDM-Style Step 2: Clean & Modern Frameless "Download File Info" Dialog
class DownloadDialog : public QDialog {
    Q_OBJECT
public:
    explicit DownloadDialog(const QString& initialUrl, QWidget* parent)
        : DownloadDialog(initialUrl, "", "", QJsonArray(), parent) {}

    explicit DownloadDialog(
        const QString& initialUrl = "", 
        const QString& initialTitle = "", 
        const QString& chosenQuality = "", 
        const QJsonArray& passedQualities = QJsonArray(), 
        QWidget* parent = nullptr
    ) : QDialog(parent), m_chosenQuality(chosenQuality) {
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_StyledBackground, true);

        // 1. Determine if this download is a genuine video or another format
        m_isVideo = CategoryRules::isVideoPortal(initialUrl) || CategoryRules::isDirectMediaStream(initialUrl);
        if (!m_isVideo) {
            QString checkName = !initialTitle.isEmpty() ? initialTitle : initialUrl;
            QUrl u(checkName);
            QString path = u.isValid() && !u.path().isEmpty() ? u.path() : checkName;
            int qIdx = path.indexOf('?');
            if (qIdx != -1) path = path.left(qIdx);
            QString ext = QFileInfo(path).suffix().toLower();
            if (ext == "mp4" || ext == "mkv" || ext == "webm" || ext == "avi" || ext == "mov" || ext == "flv" || ext == "wmv" || ext == "ts" || ext == "m4v") {
                m_isVideo = true;
            }
        }
        if (!m_isVideo && !passedQualities.isEmpty()) {
            for (const auto& qVal : passedQualities) {
                if (qVal.toObject()["height"].toInt(0) > 0) {
                    m_isVideo = true;
                    break;
                }
            }
        }

        // 2. Setup distinct UI based on category
        if (m_isVideo) {
            setFixedSize(560, 340);
            setStyleSheet(
                "QDialog { background-color: #0b0f19; color: #f8fafc; font-family: 'Segoe UI', sans-serif; border: 1px solid #1e293b; border-radius: 10px; }"
                "QLabel { color: #94a3b8; font-size: 11.5px; border: none; background: transparent; }"
            );
            setupVideoUI();
        } else {
            setFixedSize(560, 340);
            setStyleSheet(
                "QDialog { background-color: #080d1a; color: #f8fafc; font-family: 'Segoe UI', sans-serif; border: 1px solid #1e293b; border-radius: 10px; }"
                "QLabel { color: #94a3b8; font-size: 11.5px; border: none; background: transparent; }"
            );
            setupOthersUI();
        }

        // 3. Populate URL
        if (!initialUrl.isEmpty()) {
            m_urlEdit->setText(initialUrl);
            updateReferrer(initialUrl);
        }

        // 4. Populate Filename with 100% authentic original extension for non-videos
        if (m_isVideo && CategoryRules::isVideoPortal(initialUrl)) {
            QString cleanTitle = initialTitle;
            if (cleanTitle.endsWith(".unknown_video", Qt::CaseInsensitive)) {
                cleanTitle = cleanTitle.left(cleanTitle.length() - 14);
            }
            if (cleanTitle.endsWith(".unknown_video.mp4", Qt::CaseInsensitive)) {
                cleanTitle = cleanTitle.left(cleanTitle.length() - 18);
            }
            if (cleanTitle.endsWith(".image", Qt::CaseInsensitive)) {
                cleanTitle = cleanTitle.left(cleanTitle.length() - 6);
            }
            if (cleanTitle.endsWith(".image.jpg", Qt::CaseInsensitive)) {
                cleanTitle = cleanTitle.left(cleanTitle.length() - 10);
            }
            QString resolved = CategoryRules::resolveVideoTitle(cleanTitle, "", "", "", initialUrl);
            setFileNameText(resolved);
        } else {
            // Non-video files (.exe, .zip, .pdf, etc.) or direct media stream:
            // Ensure we extract the authentic original filename and format extension
            QString chosenName = CategoryRules::ensureFilenameWithExtension(initialTitle, initialUrl);
            if (chosenName.isEmpty()) {
                chosenName = CategoryRules::extractFilenameFromUrl(initialUrl);
            }
            if (chosenName.isEmpty()) {
                chosenName = "downloaded_file";
            }
            setFileNameText(chosenName);
        }

        updateFileExtensionBadge(m_fileNameEdit->text());

        // 5. Populate Category & Folder
        QString detectedCat;
        if (m_isVideo) {
            detectedCat = "Videos";
        } else {
            detectedCat = CategoryRules::categorizeFilename(m_fileNameEdit->text());
        }
        if (m_categoryCombo) {
            m_categoryCombo->setCurrentText(detectedCat);
            m_saveDirEdit->setText(DownloadManager::instance().categoryFolder(detectedCat));
        }

        // 6. Populate Format/Quality or File Size
        if (m_isVideo) {
            updateFormatOrSizeUI(true, 0);
            bool hasQualities = !passedQualities.isEmpty();
            if (hasQualities) {
                m_formatCombo->clear();
                int matchedIdx = -1;

                for (const auto& qVal : passedQualities) {
                    QJsonObject q = qVal.toObject();
                    QString val = q["value"].toString();
                    QString label = q["label"].toString();
                    int h = q["height"].toInt(0);
                    if (h > 0 && h < 240) continue; // Minimum video quality is 240p
                    quint64 sz = q["size"].toVariant().toULongLong();

                    QString formatId;
                    QString ext = "mp4";
                    if (val == "audio" || label.contains("audio", Qt::CaseInsensitive)) {
                        formatId = "bestaudio/best";
                        ext = "m4a";
                    } else if (q.contains("format_id") && !q["format_id"].toString().isEmpty()) {
                        formatId = q["format_id"].toString();
                    } else if (h > 0) {
                        formatId = QString("%1p").arg(h);
                    } else if (val.endsWith("p")) {
                        formatId = val;
                    } else {
                        formatId = "bestvideo+bestaudio/best";
                    }

                    QString displayLabel = label;
                    if (displayLabel.isEmpty()) {
                        displayLabel = (h > 0) ? QString("%1p").arg(h) : (val == "audio" ? "Audio Only (MP3)" : "Original Stream");
                    }
                    m_formatCombo->addItem(displayLabel, formatId);
                    int lastIdx = m_formatCombo->count() - 1;
                    m_formatCombo->setItemData(lastIdx, ext, Qt::UserRole + 1);
                    m_formatCombo->setItemData(lastIdx, sz, Qt::UserRole + 2);
                    m_formatCombo->setItemData(lastIdx, h, Qt::UserRole + 3);

                    if (!m_chosenQuality.isEmpty()) {
                        if (val == m_chosenQuality || (h > 0 && m_chosenQuality.contains(QString::number(h))) || (val == "audio" && m_chosenQuality.contains("audio", Qt::CaseInsensitive))) {
                            matchedIdx = lastIdx;
                        }
                    }
                }

                int targetIdx = (matchedIdx != -1) ? matchedIdx : (m_formatCombo->count() > 0 ? 0 : -1);
                if (targetIdx != -1) {
                    m_formatCombo->setCurrentIndex(targetIdx);
                    onFormatIndexChanged(targetIdx);
                }
            } else if (!initialUrl.isEmpty()) {
                probeUrl(initialUrl);
            }
        } else {
            quint64 passedSz = 0;
            if (!passedQualities.isEmpty()) {
                passedSz = passedQualities[0].toObject()["size"].toVariant().toULongLong();
            }
            if (passedSz > 0) {
                m_totalSize = passedSz;
            }
            updateFormatOrSizeUI(false, passedSz);
            bool hasValidExt = CategoryRules::hasValidExtension(m_fileNameEdit->text());
            if ((passedSz == 0 || !hasValidExt) && !initialUrl.isEmpty()) {
                probeUrl(initialUrl);
            }
            updateHeroSubtitle();
        }
    }

    QString url() const { return m_urlEdit ? m_urlEdit->text().trimmed() : ""; }
    QString fileName() const {
        if (!m_fileNameEdit) return "";
        QString fn = m_fileNameEdit->text().trimmed();
        if (!m_isVideo) {
            // Guarantee authentic extension (.exe, .zip, .pdf, etc.)
            if (!CategoryRules::hasValidExtension(fn)) {
                fn = CategoryRules::ensureFilenameWithExtension(
                    fn, 
                    m_urlEdit ? m_urlEdit->text().trimmed() : "", 
                    "", 
                    m_categoryCombo ? m_categoryCombo->currentText() : ""
                );
            }
            return fn;
        }
        int idx = m_formatCombo ? m_formatCombo->currentIndex() : -1;
        QString optExt = (idx >= 0) ? m_formatCombo->itemData(idx, Qt::UserRole + 1).toString().trimmed().toLower() : "";
        int optHeight = (idx >= 0) ? m_formatCombo->itemData(idx, Qt::UserRole + 3).toInt() : 0;
        QString formatId = (idx >= 0) ? m_formatCombo->itemData(idx).toString() : "";
        QString itemText = (idx >= 0) ? m_formatCombo->itemText(idx) : "";
        QString qVal = !formatId.isEmpty() ? formatId : itemText;
        if (itemText.contains("audio", Qt::CaseInsensitive) || formatId.contains("audio", Qt::CaseInsensitive)) {
            qVal = "Audio";
            if (optExt.isEmpty() || optExt == "mp4") optExt = "m4a";
        }
        fn = CategoryRules::applyQualityToFilename(fn, qVal, optHeight, optExt);
        return fn;
    }
    quint64 totalSize() const { return m_totalSize; }
    QString category() const { return m_categoryCombo ? m_categoryCombo->currentText() : (m_isVideo ? "Videos" : "Downloads"); }
    QString saveDir() const { return m_saveDirEdit ? m_saveDirEdit->text().trimmed() : DownloadManager::instance().defaultSavePath(); }
    int threads() const { return DownloadManager::instance().defaultThreads(); }
    QString formatQuality() const { return (m_isVideo && m_formatCombo) ? m_formatCombo->currentData().toString() : "best"; }
    bool startImmediately() const { return !m_startLater; }
    QString audioUrl() const { return m_audioUrl; }
    void setAudioUrl(const QString& a) { m_audioUrl = a; }
    bool supportsRanges() const { return m_supportsRanges; }
    QString eTag() const { return m_eTag; }
    QString lastModified() const { return m_lastModified; }

    ~DownloadDialog() {
        if (m_activeProbe) {
            m_activeProbe->disconnect();
            m_activeProbe->deleteLater();
        }
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QStyleOption opt;
        opt.initFrom(this);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    }

    void showEvent(QShowEvent* event) override {
        QDialog::showEvent(event);
        int cx = 100, cy = 100;
        QScreen* screen = QGuiApplication::primaryScreen();
        if (screen) {
            QRect geo = screen->availableGeometry();
            cx = geo.center().x() - width() / 2;
            cy = geo.center().y() - height() / 2;
            move(cx, cy);
        }
#ifdef Q_OS_WIN
        HWND hwnd = (HWND)winId();
        HWND hFore = ::GetForegroundWindow();
        DWORD dwCurThread = ::GetCurrentThreadId();
        DWORD dwForeThread = hFore ? ::GetWindowThreadProcessId(hFore, NULL) : 0;
        if (dwForeThread && dwForeThread != dwCurThread) {
            ::AttachThreadInput(dwCurThread, dwForeThread, TRUE);
            ::SetWindowPos(hwnd, HWND_TOPMOST, cx, cy, width(), height(), SWP_SHOWWINDOW);
            ::SetForegroundWindow(hwnd);
            ::BringWindowToTop(hwnd);
            ::AttachThreadInput(dwCurThread, dwForeThread, FALSE);
        } else {
            ::SetWindowPos(hwnd, HWND_TOPMOST, cx, cy, width(), height(), SWP_SHOWWINDOW);
            ::SetForegroundWindow(hwnd);
            ::BringWindowToTop(hwnd);
        }
#endif
    }
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
            event->accept();
        }
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if (event->buttons() & Qt::LeftButton) {
            move(event->globalPosition().toPoint() - m_dragPos);
            event->accept();
        }
    }

private:
    void setFileNameText(const QString& text) {
        if (!m_fileNameEdit) return;
        m_fileNameEdit->setText(text);
        m_fileNameEdit->setCursorPosition(0);
        m_fileNameEdit->home(false);
        m_fileNameEdit->setToolTip(text);
        updateFileExtensionBadge(text);
    }
    void updateCategoryIcon(const QString& cat) {
        if (!m_formatIcon) return;
        QString c = cat.trimmed().toLower();
        QString iconPath = ":/icons/cat_default.png";
        if (c == "programs") iconPath = ":/icons/cat_programs.png";
        else if (c == "videos") iconPath = ":/icons/cat_videos.png";
        else if (c == "music") iconPath = ":/icons/cat_music.png";
        else if (c == "compressed") iconPath = ":/icons/cat_compressed.png";
        else if (c == "documents") iconPath = ":/icons/cat_documents.png";
        else if (c == "images") iconPath = ":/icons/cat_images.png";

        QPixmap pix(iconPath);
        if (!pix.isNull()) {
            m_formatIcon->setPixmap(pix.scaled(26, 26, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }

    void updateFileExtensionBadge(const QString& filename) {
        QFileInfo fi(filename);
        QString ext = fi.suffix().toUpper();
        if (ext.isEmpty()) ext = m_isVideo ? "MP4" : "FILE";
        if (m_extBadge) {
            m_extBadge->setText(ext);
        }
        QString cat = CategoryRules::categorizeFilename(filename);
        if (m_isVideo) cat = "Videos";
        updateCategoryIcon(cat);
        updateHeroSubtitle();
    }

    void updateReferrer(const QString& url) {
        if (!m_lblReferrer) return;
        QUrl u(url);
        QString host = u.host();
        if (host.isEmpty()) host = "direct download";
        m_lblReferrer->setText("Source:  " + host);
    }

    void updateHeroSubtitle() {
        if (!m_lblDetails) return;
        if (m_isVideo) {
            QString qText = (m_formatCombo && m_formatCombo->currentIndex() >= 0) ? m_formatCombo->currentText() : "Full HD 1080p";
            quint64 sz = m_totalSize;
            QString szStr = (sz > 0) ? CategoryRules::formatBytes(sz) : "~100 MB";
            m_lblDetails->setText(QString("%1   •   %2").arg(qText, szStr));
        } else {
            quint64 sz = m_totalSize;
            QString szStr = (sz > 0) ? CategoryRules::formatBytes(sz) : "Calculating size...";
            QString catStr = m_categoryCombo ? m_categoryCombo->currentText() : "Programs";
            m_lblDetails->setText(QString("%1   •   %2").arg(szStr, catStr));
        }
    }

    // Video Category Specific UI (Concept B: Cinema Stream Card)
    void setupVideoUI() {
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(16, 12, 16, 12);
        mainLayout->setSpacing(7);

        // 1. Header Bar: [App Logo] Prime Download Manager ... [✕ Close]
        QHBoxLayout* headerLayout = new QHBoxLayout();
        headerLayout->setContentsMargins(0, 0, 0, 0);
        headerLayout->setSpacing(8);

        QLabel* appLogoLabel = new QLabel(this);
        appLogoLabel->setFixedSize(22, 22);
        QPixmap logoPix(":/icons/app_logo.png");
        if (!logoPix.isNull()) {
            appLogoLabel->setPixmap(logoPix.scaled(22, 22, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }

        QLabel* titleLabel = new QLabel("Prime Download Manager", this);
        titleLabel->setStyleSheet("color: #ffffff; font-size: 13px; font-weight: 700; font-family: 'Segoe UI', sans-serif;");

        QPushButton* closeBtn = new QPushButton(this);
        closeBtn->setIcon(QIcon(":/icons/close.png"));
        closeBtn->setIconSize(QSize(18, 18));
        closeBtn->setFixedSize(22, 22);
        closeBtn->setCursor(Qt::PointingHandCursor);
        closeBtn->setToolTip("Close");
        closeBtn->setStyleSheet(
            "QPushButton { background: transparent; border: none; border-radius: 4px; padding: 0px; min-width: 22px; }"
            "QPushButton:hover { background-color: rgba(239, 68, 68, 0.25); }"
            "QPushButton:pressed { background-color: rgba(239, 68, 68, 0.5); }"
        );
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);

        headerLayout->addWidget(appLogoLabel);
        headerLayout->addWidget(titleLabel);
        headerLayout->addStretch();
        headerLayout->addWidget(closeBtn);
        mainLayout->addLayout(headerLayout);

        // 2. Hero Video Card (Category Icon on left, File Name, Format Badge on right)
        QFrame* heroCard = new QFrame(this);
        heroCard->setFixedHeight(48);
        heroCard->setStyleSheet("QFrame#heroCard { background-color: #121829; border: 1px solid rgba(168, 85, 247, 0.3); border-radius: 8px; }");
        heroCard->setObjectName("heroCard");
        QHBoxLayout* heroLayout = new QHBoxLayout(heroCard);
        heroLayout->setContentsMargins(8, 6, 8, 6);
        heroLayout->setSpacing(8);

        QFrame* iconFrame = new QFrame(heroCard);
        iconFrame->setFixedSize(32, 32);
        iconFrame->setStyleSheet("background-color: #1a1738; border: 1px solid rgba(168, 85, 247, 0.35); border-radius: 6px;");
        QHBoxLayout* iconInner = new QHBoxLayout(iconFrame);
        iconInner->setContentsMargins(0, 0, 0, 0);
        m_formatIcon = new QLabel(iconFrame);
        m_formatIcon->setAlignment(Qt::AlignCenter);
        QPixmap vidPix(":/icons/cat_videos.png");
        if (!vidPix.isNull()) {
            m_formatIcon->setPixmap(vidPix.scaled(22, 22, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        iconInner->addWidget(m_formatIcon);

        m_fileNameEdit = new QLineEdit(heroCard);
        m_fileNameEdit->setPlaceholderText("video.mp4");
        m_fileNameEdit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_fileNameEdit->setStyleSheet(
            "QLineEdit { background: transparent; color: #f8fafc; border: none; font-size: 12.5px; font-weight: 600; selection-background-color: #0284c7; padding-left: 2px; }"
        );

        m_extBadge = new QLabel("MP4", heroCard);
        m_extBadge->setStyleSheet(
            "QLabel { background-color: rgba(168, 85, 247, 0.18); border: 1px solid rgba(168, 85, 247, 0.4); "
            "color: #c084fc; border-radius: 4px; padding: 2px 8px; font-size: 10.5px; font-weight: 700; }"
        );

        heroLayout->addWidget(iconFrame);
        heroLayout->addWidget(m_fileNameEdit, 1);
        heroLayout->addWidget(m_extBadge);
        mainLayout->addWidget(heroCard);

        // 3. Resolution & Format Selection
        QVBoxLayout* formatCol = new QVBoxLayout();
        formatCol->setSpacing(3);
        m_lblFormat = new QLabel("RESOLUTION & FORMAT", this);
        m_lblFormat->setStyleSheet("color: #64748b; font-size: 9.5px; font-weight: 700; letter-spacing: 0.5px;");

        m_formatCombo = new QComboBox(this);
        m_formatCombo->setFixedHeight(30);
        auto addDefaultFormat = [this](const QString& label, const QString& fId, const QString& ext, quint64 sz, int h) {
            m_formatCombo->addItem(label, fId);
            int idx = m_formatCombo->count() - 1;
            m_formatCombo->setItemData(idx, ext, Qt::UserRole + 1);
            m_formatCombo->setItemData(idx, sz, Qt::UserRole + 2);
            m_formatCombo->setItemData(idx, h, Qt::UserRole + 3);
        };
        addDefaultFormat("Full HD (1080p)", "bestvideo[height<=1080]+bestaudio/best", "mp4", 0, 1080);
        addDefaultFormat("HD (720p)", "bestvideo[height<=720]+bestaudio/best", "mp4", 0, 720);
        addDefaultFormat("Standard (480p)", "bestvideo[height<=480]+bestaudio/best", "mp4", 0, 480);
        addDefaultFormat("Medium (360p)", "bestvideo[height<=360]+bestaudio/best", "mp4", 0, 360);
        addDefaultFormat("Low (240p)", "bestvideo[height<=240]+bestaudio/best", "mp4", 0, 240);
        addDefaultFormat("Audio Only (MP3 / M4A)", "bestaudio/best", "m4a", 0, 0);
        m_formatCombo->setStyleSheet(
            "QComboBox { background-color: #0d1527; color: #f8fafc; border: 1px solid #1e293b; border-radius: 6px; padding: 2px 10px; font-size: 12px; font-weight: 600; }"
            "QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: center right; width: 22px; border: none; }"
            "QComboBox QAbstractItemView { background-color: #0d1527; color: #f8fafc; selection-background-color: #0284c7; border: 1px solid #1e293b; border-radius: 4px; padding: 4px; }"
        );
        formatCol->addWidget(m_lblFormat);
        formatCol->addWidget(m_formatCombo);
        mainLayout->addLayout(formatCol);

        // 4. Save Location & Stream URL
        QVBoxLayout* stCol = new QVBoxLayout();
        stCol->setSpacing(3);
        QLabel* lblSaveTo = new QLabel("SAVE TO", this);
        lblSaveTo->setStyleSheet("color: #64748b; font-size: 9.5px; font-weight: 700; letter-spacing: 0.5px;");

        QHBoxLayout* stLayout = new QHBoxLayout();
        stLayout->setSpacing(8);

        m_saveDirEdit = new QLineEdit(this);
        m_saveDirEdit->setFixedHeight(30);
        m_saveDirEdit->setText(DownloadManager::instance().categoryFolder("Videos"));
        m_saveDirEdit->setStyleSheet(
            "QLineEdit { background-color: #0d1527; color: #f8fafc; border: 1px solid #1e293b; border-radius: 6px; padding: 4px 10px; font-size: 11.5px; }"
            "QLineEdit:focus { border: 1px solid #00d2ff; }"
        );

        QPushButton* browseDirBtn = new QPushButton("Browse...", this);
        browseDirBtn->setFixedHeight(30);
        browseDirBtn->setStyleSheet(
            "QPushButton { background-color: #1e293b; color: #f8fafc; border: 1px solid #334155; border-radius: 6px; min-width: 75px; padding: 0 14px; font-size: 11.5px; font-weight: 600; }"
            "QPushButton:hover { background-color: #334155; }"
            "QPushButton:pressed { background-color: #0f172a; }"
        );
        connect(browseDirBtn, &QPushButton::clicked, this, [this]() {
            QString dir = QFileDialog::getExistingDirectory(this, "Select Save Directory", m_saveDirEdit->text());
            if (!dir.isEmpty()) m_saveDirEdit->setText(dir);
        });

        stLayout->addWidget(m_saveDirEdit, 1);
        stLayout->addWidget(browseDirBtn);
        stCol->addWidget(lblSaveTo);
        stCol->addLayout(stLayout);
        mainLayout->addLayout(stCol);

        // Stream URL row
        QVBoxLayout* urlCol = new QVBoxLayout();
        urlCol->setSpacing(3);
        QLabel* lblUrl = new QLabel("STREAM URL", this);
        lblUrl->setStyleSheet("color: #64748b; font-size: 9.5px; font-weight: 700; letter-spacing: 0.5px;");

        m_urlEdit = new QLineEdit(this);
        m_urlEdit->setFixedHeight(28);
        m_urlEdit->setPlaceholderText("https://...");
        m_urlEdit->setStyleSheet(
            "QLineEdit { background-color: #0d1527; color: #94a3b8; border: 1px solid #1e293b; border-radius: 6px; padding: 2px 10px; font-size: 11px; }"
            "QLineEdit:focus { border: 1px solid #00d2ff; color: #f8fafc; }"
        );
        urlCol->addWidget(lblUrl);
        urlCol->addWidget(m_urlEdit);
        mainLayout->addLayout(urlCol);

        // 5. Action Buttons (Footer)
        QHBoxLayout* footerLayout = new QHBoxLayout();
        footerLayout->setContentsMargins(0, 3, 0, 0);
        footerLayout->setSpacing(8);

        QPushButton* laterBtn = new QPushButton("Queue (Later)", this);
        laterBtn->setFixedSize(116, 29);
        laterBtn->setStyleSheet(
            "QPushButton { background-color: #1e293b; color: #cbd5e1; border: 1px solid #334155; border-radius: 6px; font-size: 11.5px; font-weight: 600; padding: 0 4px; }"
            "QPushButton:hover { background-color: #334155; color: #ffffff; }"
            "QPushButton:pressed { background-color: #0f172a; }"
        );
        connect(laterBtn, &QPushButton::clicked, this, [this]() {
            m_startLater = true;
            accept();
        });

        QPushButton* startBtn = new QPushButton("Start Download", this);
        startBtn->setFixedSize(116, 29);
        startBtn->setStyleSheet(
            "QPushButton { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #00d2ff, stop:1 #0284c7); color: #ffffff; border: none; border-radius: 6px; font-weight: 700; font-size: 11.5px; padding: 0 4px; }"
            "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #38bdf8, stop:1 #0369a1); }"
            "QPushButton:pressed { background-color: #0284c7; }"
        );
        startBtn->setDefault(true);
        connect(startBtn, &QPushButton::clicked, this, [this]() {
            m_startLater = false;
            accept();
        });

        QPushButton* cancelBtn = new QPushButton("Cancel", this);
        cancelBtn->setFixedSize(116, 29);
        cancelBtn->setStyleSheet(
            "QPushButton { background-color: #1e293b; color: #cbd5e1; border: 1px solid #334155; border-radius: 6px; font-size: 11.5px; font-weight: 600; padding: 0 4px; }"
            "QPushButton:hover { background-color: #334155; color: #ffffff; }"
            "QPushButton:pressed { background-color: #0f172a; }"
        );
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

        footerLayout->addStretch();
        footerLayout->addWidget(laterBtn);
        footerLayout->addWidget(startBtn);
        footerLayout->addWidget(cancelBtn);
        footerLayout->addStretch();
        mainLayout->addLayout(footerLayout);

        // Dummy category combo for video
        m_categoryCombo = new QComboBox(this);
        m_categoryCombo->addItem("Videos");
        m_categoryCombo->setVisible(false);

        // Connections
        connect(m_fileNameEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
            updateFileExtensionBadge(text);
        });

        connect(m_urlEdit, &QLineEdit::editingFinished, this, [this]() {
            probeUrl(m_urlEdit->text().trimmed());
        });

        connect(m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DownloadDialog::onFormatIndexChanged);
    }

    // Others Category Specific UI (Concept B: Modular Pro Card)
    void setupOthersUI() {
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(16, 12, 16, 12);
        mainLayout->setSpacing(7);

        // 1. Header Bar: [App Logo] Prime Download Manager ... [✕ Close]
        QHBoxLayout* headerLayout = new QHBoxLayout();
        headerLayout->setContentsMargins(0, 0, 0, 0);
        headerLayout->setSpacing(8);

        QLabel* appLogoLabel = new QLabel(this);
        appLogoLabel->setFixedSize(22, 22);
        QPixmap logoPix(":/icons/app_logo.png");
        if (!logoPix.isNull()) {
            appLogoLabel->setPixmap(logoPix.scaled(22, 22, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }

        QLabel* titleLabel = new QLabel("Prime Download Manager", this);
        titleLabel->setStyleSheet("color: #ffffff; font-size: 13px; font-weight: 700; font-family: 'Segoe UI', sans-serif;");

        QPushButton* closeBtn = new QPushButton(this);
        closeBtn->setIcon(QIcon(":/icons/close.png"));
        closeBtn->setIconSize(QSize(18, 18));
        closeBtn->setFixedSize(22, 22);
        closeBtn->setCursor(Qt::PointingHandCursor);
        closeBtn->setToolTip("Close");
        closeBtn->setStyleSheet(
            "QPushButton { background: transparent; border: none; border-radius: 4px; padding: 0px; min-width: 22px; }"
            "QPushButton:hover { background-color: rgba(239, 68, 68, 0.25); }"
            "QPushButton:pressed { background-color: rgba(239, 68, 68, 0.5); }"
        );
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);

        headerLayout->addWidget(appLogoLabel);
        headerLayout->addWidget(titleLabel);
        headerLayout->addStretch();
        headerLayout->addWidget(closeBtn);
        mainLayout->addLayout(headerLayout);

        // 2. Hero File Card (Category Icon on left, File Name, Format Badge on right)
        QFrame* heroCard = new QFrame(this);
        heroCard->setFixedHeight(44);
        heroCard->setStyleSheet("QFrame#heroCard { background-color: #111827; border: 1px solid rgba(6, 182, 212, 0.25); border-radius: 8px; }");
        heroCard->setObjectName("heroCard");
        QHBoxLayout* heroLayout = new QHBoxLayout(heroCard);
        heroLayout->setContentsMargins(8, 5, 8, 5);
        heroLayout->setSpacing(8);

        QFrame* iconFrame = new QFrame(heroCard);
        iconFrame->setFixedSize(32, 32);
        iconFrame->setStyleSheet("background-color: #0d1527; border: 1px solid #1e293b; border-radius: 6px;");
        QHBoxLayout* iconInner = new QHBoxLayout(iconFrame);
        iconInner->setContentsMargins(0, 0, 0, 0);
        m_formatIcon = new QLabel(iconFrame);
        m_formatIcon->setAlignment(Qt::AlignCenter);
        QPixmap defaultPix(":/icons/cat_programs.png");
        if (!defaultPix.isNull()) {
            m_formatIcon->setPixmap(defaultPix.scaled(22, 22, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        iconInner->addWidget(m_formatIcon);

        m_fileNameEdit = new QLineEdit(heroCard);
        m_fileNameEdit->setPlaceholderText("filename.ext");
        m_fileNameEdit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_fileNameEdit->setStyleSheet(
            "QLineEdit { background: transparent; color: #f8fafc; border: none; font-size: 12.5px; font-weight: 600; selection-background-color: #0284c7; padding-left: 2px; }"
        );

        m_extBadge = new QLabel("FILE", heroCard);
        m_extBadge->setStyleSheet(
            "QLabel { background-color: rgba(6, 182, 212, 0.18); border: 1px solid rgba(6, 182, 212, 0.4); "
            "color: #38bdf8; border-radius: 4px; padding: 2px 8px; font-size: 10.5px; font-weight: 700; }"
        );

        heroLayout->addWidget(iconFrame);
        heroLayout->addWidget(m_fileNameEdit, 1);
        heroLayout->addWidget(m_extBadge);
        mainLayout->addWidget(heroCard);

        // 3. Side-by-Side Pro Options: Format Category & File Size
        QHBoxLayout* cardsLayout = new QHBoxLayout();
        cardsLayout->setSpacing(8);

        // Left Card: Format Category
        QFrame* catCard = new QFrame(this);
        catCard->setFixedHeight(50);
        catCard->setStyleSheet("QFrame { background-color: #0d1527; border: 1px solid #1e293b; border-radius: 7px; }");
        QVBoxLayout* catCardLayout = new QVBoxLayout(catCard);
        catCardLayout->setContentsMargins(10, 5, 10, 5);
        catCardLayout->setSpacing(1);

        QLabel* lblCatTitle = new QLabel("FORMAT CATEGORY", catCard);
        lblCatTitle->setStyleSheet("color: #64748b; font-size: 9.5px; font-weight: 700; letter-spacing: 0.5px; border: none; background: transparent;");

        m_categoryCombo = new QComboBox(catCard);
        m_categoryCombo->addItems({"Programs", "Compressed", "Documents", "Music", "Videos", "Images", "Others"});
        m_categoryCombo->setStyleSheet(
            "QComboBox { background: transparent; color: #38bdf8; border: none; font-size: 12.5px; font-weight: 700; padding: 0; }"
            "QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: center right; width: 16px; border: none; }"
            "QComboBox QAbstractItemView { background-color: #0d1527; color: #f8fafc; selection-background-color: #0284c7; border: 1px solid #1e293b; border-radius: 4px; padding: 4px; }"
        );
        catCardLayout->addWidget(lblCatTitle);
        catCardLayout->addWidget(m_categoryCombo);

        // Right Card: File Size
        QFrame* sizeCard = new QFrame(this);
        sizeCard->setFixedHeight(50);
        sizeCard->setStyleSheet("QFrame { background-color: #0d1527; border: 1px solid #1e293b; border-radius: 7px; }");
        QVBoxLayout* sizeCardLayout = new QVBoxLayout(sizeCard);
        sizeCardLayout->setContentsMargins(10, 5, 10, 5);
        sizeCardLayout->setSpacing(1);

        m_lblFormat = new QLabel("FILE SIZE", sizeCard);
        m_lblFormat->setStyleSheet("color: #64748b; font-size: 9.5px; font-weight: 700; letter-spacing: 0.5px; border: none; background: transparent;");

        m_formatCombo = new QComboBox(sizeCard);
        m_formatCombo->addItem("Calculating size...", "best");
        m_formatCombo->setEnabled(false);
        m_formatCombo->setStyleSheet(
            "QComboBox { background: transparent; color: #00d2ff; border: none; font-size: 13px; font-weight: 700; padding: 0; }"
            "QComboBox:disabled { background: transparent; color: #00d2ff; border: none; font-size: 13px; font-weight: 700; }"
            "QComboBox::drop-down { width: 0px; border: none; }"
        );
        sizeCardLayout->addWidget(m_lblFormat);
        sizeCardLayout->addWidget(m_formatCombo);

        cardsLayout->addWidget(catCard, 1);
        cardsLayout->addWidget(sizeCard, 1);
        mainLayout->addLayout(cardsLayout);

        // 4. Save Directory Row
        QVBoxLayout* stCol = new QVBoxLayout();
        stCol->setSpacing(3);
        QLabel* lblSaveTo = new QLabel("SAVE TO", this);
        lblSaveTo->setStyleSheet("color: #64748b; font-size: 9.5px; font-weight: 700; letter-spacing: 0.5px;");

        QHBoxLayout* pathLayout = new QHBoxLayout();
        pathLayout->setSpacing(8);

        m_saveDirEdit = new QLineEdit(this);
        m_saveDirEdit->setFixedHeight(30);
        m_saveDirEdit->setText(DownloadManager::instance().defaultSavePath());
        m_saveDirEdit->setStyleSheet(
            "QLineEdit { background-color: #0d1527; color: #f8fafc; border: 1px solid #1e293b; border-radius: 6px; padding: 4px 10px; font-size: 11.5px; }"
            "QLineEdit:focus { border: 1px solid #00d2ff; }"
        );

        QPushButton* browseBtn = new QPushButton("Browse...", this);
        browseBtn->setFixedHeight(30);
        browseBtn->setStyleSheet(
            "QPushButton { background-color: #1e293b; color: #f8fafc; border: 1px solid #334155; border-radius: 6px; min-width: 75px; padding: 0 14px; font-size: 11.5px; font-weight: 600; }"
            "QPushButton:hover { background-color: #334155; }"
            "QPushButton:pressed { background-color: #0f172a; }"
        );
        connect(browseBtn, &QPushButton::clicked, this, [this]() {
            QString dir = QFileDialog::getExistingDirectory(this, "Select Save Directory", m_saveDirEdit->text());
            if (!dir.isEmpty()) m_saveDirEdit->setText(dir);
        });

        pathLayout->addWidget(m_saveDirEdit, 1);
        pathLayout->addWidget(browseBtn);
        stCol->addWidget(lblSaveTo);
        stCol->addLayout(pathLayout);
        mainLayout->addLayout(stCol);

        // 5. Clean Single-Row Download URL
        QVBoxLayout* urlCol = new QVBoxLayout();
        urlCol->setSpacing(3);
        QLabel* lblUrl = new QLabel("DOWNLOAD URL", this);
        lblUrl->setStyleSheet("color: #64748b; font-size: 9.5px; font-weight: 700; letter-spacing: 0.5px;");

        m_urlEdit = new QLineEdit(this);
        m_urlEdit->setFixedHeight(28);
        m_urlEdit->setPlaceholderText("https://...");
        m_urlEdit->setStyleSheet(
            "QLineEdit { background-color: #0d1527; color: #94a3b8; border: 1px solid #1e293b; border-radius: 6px; padding: 2px 10px; font-size: 11px; }"
            "QLineEdit:focus { color: #f8fafc; border: 1px solid #00d2ff; }"
        );
        urlCol->addWidget(lblUrl);
        urlCol->addWidget(m_urlEdit);
        mainLayout->addLayout(urlCol);

        // 6. Action Buttons (Footer)
        QHBoxLayout* footerLayout = new QHBoxLayout();
        footerLayout->setContentsMargins(0, 3, 0, 0);
        footerLayout->setSpacing(8);

        QPushButton* laterBtn = new QPushButton("Queue (Later)", this);
        laterBtn->setFixedSize(116, 29);
        laterBtn->setStyleSheet(
            "QPushButton { background-color: #1e293b; color: #cbd5e1; border: 1px solid #334155; border-radius: 6px; font-size: 11.5px; font-weight: 600; padding: 0 4px; }"
            "QPushButton:hover { background-color: #334155; color: #ffffff; }"
            "QPushButton:pressed { background-color: #0f172a; }"
        );
        connect(laterBtn, &QPushButton::clicked, this, [this]() {
            m_startLater = true;
            accept();
        });

        QPushButton* startBtn = new QPushButton("Start Download", this);
        startBtn->setFixedSize(116, 29);
        startBtn->setStyleSheet(
            "QPushButton { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #00d2ff, stop:1 #0284c7); color: #ffffff; border: none; border-radius: 6px; font-weight: 700; font-size: 11.5px; padding: 0 4px; }"
            "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #38bdf8, stop:1 #0369a1); }"
            "QPushButton:pressed { background-color: #0284c7; }"
        );
        startBtn->setDefault(true);
        connect(startBtn, &QPushButton::clicked, this, [this]() {
            m_startLater = false;
            accept();
        });

        QPushButton* cancelBtn = new QPushButton("Cancel", this);
        cancelBtn->setFixedSize(116, 29);
        cancelBtn->setStyleSheet(
            "QPushButton { background-color: #1e293b; color: #cbd5e1; border: 1px solid #334155; border-radius: 6px; font-size: 11.5px; font-weight: 600; padding: 0 4px; }"
            "QPushButton:hover { background-color: #334155; color: #ffffff; }"
            "QPushButton:pressed { background-color: #0f172a; }"
        );
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

        footerLayout->addStretch();
        footerLayout->addWidget(laterBtn);
        footerLayout->addWidget(startBtn);
        footerLayout->addWidget(cancelBtn);
        footerLayout->addStretch();
        mainLayout->addLayout(footerLayout);

        // Connections
        connect(m_categoryCombo, &QComboBox::currentTextChanged, this, [this](const QString& cat) {
            m_saveDirEdit->setText(DownloadManager::instance().categoryFolder(cat));
            updateCategoryIcon(cat);
        });

        connect(m_fileNameEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
            updateFileExtensionBadge(text);
            QString cat = CategoryRules::categorizeFilename(text);
            if (m_categoryCombo && m_categoryCombo->currentText() != cat) {
                m_categoryCombo->setCurrentText(cat);
            }
        });

        connect(m_urlEdit, &QLineEdit::editingFinished, this, [this]() {
            probeUrl(m_urlEdit->text().trimmed());
        });
    }

    void onFormatIndexChanged(int idx) {
        if (!m_isVideo || idx < 0) return;
        QString optExt = m_formatCombo->itemData(idx, Qt::UserRole + 1).toString().trimmed().toLower();
        quint64 optSize = m_formatCombo->itemData(idx, Qt::UserRole + 2).toULongLong();
        int optHeight = m_formatCombo->itemData(idx, Qt::UserRole + 3).toInt();
        QString formatId = m_formatCombo->itemData(idx).toString();
        QString itemText = m_formatCombo->itemText(idx);

        if (optSize > 0) {
            m_totalSize = optSize;
        }

        QString currentName = m_fileNameEdit->text().trimmed();
        QFileInfo fi(currentName);
        QString currentExt = fi.suffix().toLower();

        // If no explicit format extension was provided on this item, keep authentic file extension
        if (optExt.isEmpty()) {
            optExt = currentExt;
        }
        if (optExt == "unknown_video" || optExt == "image") {
            optExt = "mp4";
        }
        if (optExt.isEmpty() && CategoryRules::isVideoPortal(m_urlEdit->text())) {
            optExt = "mp4";
        }

        if (currentName.endsWith(".unknown_video", Qt::CaseInsensitive)) {
            currentName = currentName.left(currentName.length() - 14);
        }
        if (currentName.endsWith(".unknown_video.mp4", Qt::CaseInsensitive)) {
            currentName = currentName.left(currentName.length() - 18);
        }
        if (currentName.endsWith(".image", Qt::CaseInsensitive)) {
            currentName = currentName.left(currentName.length() - 6);
        }
        if (currentName.endsWith(".image.mp4", Qt::CaseInsensitive)) {
            currentName = currentName.left(currentName.length() - 10);
        }

        // Only modify extension and quality tag if it's a video/audio quality option
        if (CategoryRules::isVideoPortal(m_urlEdit->text()) || optExt == "mp4" || optExt == "m4a" || optExt == "mkv" || optExt == "webm") {
            if (optExt.isEmpty() || optExt == "unknown_video" || optExt == "image") {
                optExt = "mp4";
            }
            QFileInfo cleanFi(currentName);
            QString base = cleanFi.suffix().isEmpty() ? currentName : cleanFi.completeBaseName();
            if (base.endsWith(".unknown_video", Qt::CaseInsensitive)) {
                base = base.left(base.length() - 14);
            }
            if (base.endsWith(".image", Qt::CaseInsensitive)) {
                base = base.left(base.length() - 6);
            }
            if (base.isEmpty()) {
                base = (optExt == "mp4" || optExt == "mkv" || optExt == "webm") ? "video" : "download";
            }

            QString qVal = !formatId.isEmpty() ? formatId : itemText;
            if (itemText.contains("audio", Qt::CaseInsensitive) || formatId.contains("audio", Qt::CaseInsensitive)) {
                qVal = "Audio";
                if (optExt.isEmpty() || optExt == "mp4") optExt = "m4a";
            }
            QString newName = CategoryRules::applyQualityToFilename(base, qVal, optHeight, optExt);
            setFileNameText(newName);
        }

        // Dynamically update category and save directory based on resulting file extension
        if (m_categoryCombo) {
            QString detectedCat = CategoryRules::categorizeFilename(m_fileNameEdit->text());
            m_categoryCombo->setCurrentText(detectedCat);
            m_saveDirEdit->setText(DownloadManager::instance().categoryFolder(detectedCat));
        }
        updateHeroSubtitle();
    }

    void updateFormatOrSizeUI(bool isVideo, quint64 size = 0) {
        if (isVideo) {
            if (m_lblFormat) m_lblFormat->setText("Resolution / Quality");
            if (m_formatCombo) {
                m_formatCombo->setEnabled(true);
                m_formatCombo->setStyleSheet(
                    "QComboBox { background-color: #0d1527; color: #f8fafc; border: 1px solid #1e293b; border-radius: 6px; padding: 2px 10px; font-size: 12px; font-weight: 600; }"
                    "QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: center right; width: 20px; border: none; }"
                    "QComboBox QAbstractItemView { background-color: #0d1527; color: #f8fafc; selection-background-color: #0284c7; border: 1px solid #1e293b; border-radius: 4px; padding: 4px; }"
                );
            }
        } else {
            if (m_lblFormat) m_lblFormat->setText("File Size");
            if (m_formatCombo) {
                m_formatCombo->setEnabled(false);
                m_formatCombo->clear();
                quint64 sz = (size > 0) ? size : m_totalSize;
                QString sText;
                if (sz > 0) {
                    sText = CategoryRules::formatBytes(sz);
                } else {
                    sText = "Calculating size...";
                }
                m_formatCombo->addItem(sText, "best");
                m_formatCombo->setStyleSheet(
                    "QComboBox { background: transparent; color: #00d2ff; border: none; font-size: 13.5px; font-weight: 700; padding: 0; }"
                    "QComboBox:disabled { background: transparent; color: #00d2ff; border: none; font-size: 13.5px; font-weight: 700; }"
                    "QComboBox::drop-down { width: 0px; border: none; }"
                );
            }
        }
    }

    void probeUrl(const QString& rawUrl) {
        if (rawUrl.isEmpty()) return;
        if (m_activeProbe) {
            m_activeProbe->disconnect();
            m_activeProbe->deleteLater();
        }

        UrlProbe* probe = new UrlProbe(this);
        m_activeProbe = probe;
        QPointer<DownloadDialog> self(this);

        connect(probe, &UrlProbe::videoTitleDiscovered, this, [self](const QString& title) {
            if (!self) return;
            if (!title.isEmpty()) {
                self->setFileNameText(CategoryRules::sanitizeFilename(title + ".mp4"));
            }
        });

        connect(probe, &UrlProbe::probeFinished, this, [self, probe](const ProbeResult& res) {
            probe->deleteLater();
            if (!self) return; // Dialog was closed/destroyed, abort safely to prevent crash

            if (self->m_formatCombo) self->m_formatCombo->blockSignals(true);

            self->m_supportsRanges = res.supportsRanges;
            self->m_eTag = res.eTag;
            self->m_lastModified = res.lastModified;

            if (self->m_isVideo) {
                self->updateFormatOrSizeUI(true);
                if (!res.fileName.isEmpty()) {
                    self->setFileNameText(res.fileName);
                }
                if (self->m_categoryCombo) {
                    self->m_categoryCombo->setCurrentText("Videos");
                    self->m_saveDirEdit->setText(DownloadManager::instance().categoryFolder("Videos"));
                }
                self->m_totalSize = res.totalSize;

                if (!res.videoFormats.isEmpty()) {
                    self->m_formatCombo->clear();
                    int matchedIdx = -1;
                    for (int i = 0; i < res.videoFormats.size(); ++i) {
                        const auto& f = res.videoFormats[i];
                        if (f.height > 0 && f.height < 240) continue; // Minimum video quality is 240p
                        self->m_formatCombo->addItem(f.label, f.formatId);
                        int lastIdx = self->m_formatCombo->count() - 1;
                        self->m_formatCombo->setItemData(lastIdx, (f.ext.isEmpty() || f.ext == "unknown_video") ? "mp4" : f.ext, Qt::UserRole + 1);
                        self->m_formatCombo->setItemData(lastIdx, f.filesize, Qt::UserRole + 2);
                        self->m_formatCombo->setItemData(lastIdx, f.height, Qt::UserRole + 3);

                        if (!self->m_chosenQuality.isEmpty()) {
                            if (f.formatId.contains(self->m_chosenQuality, Qt::CaseInsensitive) || 
                                f.label.contains(self->m_chosenQuality, Qt::CaseInsensitive) ||
                                (f.height > 0 && self->m_chosenQuality.contains(QString::number(f.height))) ||
                                (f.height == 0 && self->m_chosenQuality.contains("audio", Qt::CaseInsensitive))) {
                                matchedIdx = lastIdx;
                            }
                        }
                    }
                    self->m_formatCombo->blockSignals(false);
                    int selIdx = (matchedIdx != -1) ? matchedIdx : (self->m_formatCombo->count() > 0 ? 0 : -1);
                    if (selIdx != -1) {
                        self->m_formatCombo->setCurrentIndex(selIdx);
                        self->onFormatIndexChanged(selIdx);
                    }
                } else {
                    self->m_formatCombo->blockSignals(false);
                }
            } else {
                // Non-video files (.exe, .zip, .rar, .pdf, documents, etc.)
                if (!res.fileName.isEmpty()) {
                    self->setFileNameText(res.fileName);
                }
                QString cat = CategoryRules::categorizeFilename(self->m_fileNameEdit->text());
                if (self->m_categoryCombo) {
                    self->m_categoryCombo->setCurrentText(cat);
                    self->m_saveDirEdit->setText(DownloadManager::instance().categoryFolder(cat));
                }
                self->updateFormatOrSizeUI(false, res.totalSize);
                QString authenticExt = QFileInfo(self->m_fileNameEdit->text()).suffix().toLower();
                if (self->m_formatCombo) {
                    self->m_formatCombo->setItemData(0, authenticExt, Qt::UserRole + 1);
                    self->m_formatCombo->setItemData(0, res.totalSize, Qt::UserRole + 2);
                    self->m_formatCombo->blockSignals(false);
                }
            }
            self->updateHeroSubtitle();
            self->updateCategoryIcon(self->m_categoryCombo ? self->m_categoryCombo->currentText() : (self->m_isVideo ? "Videos" : "Programs"));
        });
        probe->probe(rawUrl);
    }

    QLineEdit* m_urlEdit = nullptr;
    QComboBox* m_categoryCombo = nullptr;
    QLabel* m_lblFormat = nullptr;
    QLabel* m_formatIcon = nullptr;
    QLabel* m_extBadge = nullptr;
    QLabel* m_lblReferrer = nullptr;
    QLabel* m_lblDetails = nullptr;
    QLineEdit* m_fileNameEdit = nullptr;
    QComboBox* m_formatCombo = nullptr;
    QLineEdit* m_saveDirEdit = nullptr;
    QPointer<UrlProbe> m_activeProbe;
    quint64 m_totalSize = 0;
    bool m_startLater = false;
    bool m_isVideo = false;
    QString m_chosenQuality;
    QString m_audioUrl;
    bool m_supportsRanges = true;
    QString m_eTag;
    QString m_lastModified;
    QPoint m_dragPos;
};
