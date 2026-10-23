#pragma once
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QListWidget>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QInputDialog>
#include <QMessageBox>
#include <QFrame>
#include <QScrollArea>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOption>
#include "../core/DownloadManager.h"

// ========================================================
// Address Exceptions Modal Dialog
// ========================================================
class AddressExceptionsDialog : public QDialog {
    Q_OBJECT
public:
    explicit AddressExceptionsDialog(const QStringList& initialList, QWidget* parent = nullptr) 
        : QDialog(parent), m_list(initialList) 
    {
        setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_StyledBackground, true);
        setFixedSize(480, 420);
        setStyleSheet(
            "QDialog { background-color: #0b101d; color: #f8fafc; font-family: 'Segoe UI', sans-serif; border: 1px solid #1e293b; border-radius: 10px; }"
            "QLabel { color: #94a3b8; font-size: 12px; border: none; background: transparent; }"
            "QListWidget { background-color: #070b14; color: #38bdf8; border: 1px solid #1e293b; border-radius: 6px; padding: 4px; font-family: monospace; font-size: 11px; }"
            "QListWidget::item { padding: 4px 6px; border-radius: 4px; }"
            "QListWidget::item:selected { background-color: #0284c7; color: #ffffff; }"
            "QPushButton { border-radius: 6px; padding: 6px 14px; font-size: 12px; font-weight: 600; }"
        );

        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(18, 14, 18, 14);
        mainLayout->setSpacing(10);

        // Header with title and close.png button
        QHBoxLayout* headerLayout = new QHBoxLayout();
        headerLayout->setContentsMargins(0, 0, 0, 0);
        headerLayout->setSpacing(8);

        QLabel* titleLabel = new QLabel("Address Exceptions List", this);
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

        QLabel* lblPrompt = new QLabel("Don't start downloading automatically from these addresses:", this);
        lblPrompt->setStyleSheet("color: #cbd5e1; font-weight: 500;");
        mainLayout->addWidget(lblPrompt);

        m_listWidget = new QListWidget(this);
        for (const auto& item : m_list) {
            m_listWidget->addItem(item);
        }
        mainLayout->addWidget(m_listWidget);

        QLabel* lblHint = new QLabel("💡 You may use the asterisk (*) wildcard for address patterns.", this);
        lblHint->setStyleSheet("color: #64748b; font-size: 11px;");
        mainLayout->addWidget(lblHint);

        QHBoxLayout* btnLayout = new QHBoxLayout();
        QPushButton* addBtn = new QPushButton("Add...", this);
        addBtn->setStyleSheet("background-color: #0284c7; color: #ffffff; border: none;");
        connect(addBtn, &QPushButton::clicked, this, [this]() {
            bool ok;
            QString text = QInputDialog::getText(this, "Add Address Exception", "URL pattern or domain (e.g. *.example.com/*):", QLineEdit::Normal, "", &ok);
            if (ok && !text.trimmed().isEmpty()) {
                m_listWidget->addItem(text.trimmed());
            }
        });

        QPushButton* delBtn = new QPushButton("Delete", this);
        delBtn->setStyleSheet("background-color: #881337; color: #fecdd3; border: 1px solid #9f1239;");
        connect(delBtn, &QPushButton::clicked, this, [this]() {
            auto sel = m_listWidget->selectedItems();
            for (auto* item : sel) {
                delete m_listWidget->takeItem(m_listWidget->row(item));
            }
        });

        QPushButton* defBtn = new QPushButton("Default", this);
        defBtn->setStyleSheet("background-color: #1e293b; color: #94a3b8; border: 1px solid #334155;");
        connect(defBtn, &QPushButton::clicked, this, [this]() {
            m_listWidget->clear();
            for (const auto& item : DownloadManager::defaultAddressExceptions()) {
                m_listWidget->addItem(item);
            }
        });

        QPushButton* okBtn = new QPushButton("OK", this);
        okBtn->setStyleSheet("background-color: #00d2ff; color: #000000; border: none; font-weight: bold; min-width: 80px;");
        connect(okBtn, &QPushButton::clicked, this, [this]() {
            m_list.clear();
            for (int i = 0; i < m_listWidget->count(); ++i) {
                m_list.append(m_listWidget->item(i)->text().trimmed());
            }
            accept();
        });

        btnLayout->addWidget(addBtn);
        btnLayout->addWidget(delBtn);
        btnLayout->addWidget(defBtn);
        btnLayout->addStretch();
        btnLayout->addWidget(okBtn);
        mainLayout->addLayout(btnLayout);
    }

    QStringList exceptions() const { return m_list; }

protected:
    void paintEvent(QPaintEvent*) override {
        QStyleOption opt;
        opt.initFrom(this);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
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
    QStringList m_list;
    QListWidget* m_listWidget = nullptr;
    QPoint m_dragPos;
};

// ========================================================
// Main OptionsDialog with all 5 Full Feature Tabs
// ========================================================
class OptionsDialog : public QDialog {
    Q_OBJECT
public:
    explicit OptionsDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_StyledBackground, true);
        setFixedSize(500, 520);
        setStyleSheet(
            "QDialog { background-color: #0b101d; color: #f8fafc; font-family: 'Segoe UI', system-ui, sans-serif; border: 1px solid #1e293b; border-radius: 10px; }"
            "QFrame#settingCard { background-color: #111827; border: 1px solid rgba(255, 255, 255, 0.08); border-radius: 8px; }"
            "QLabel { color: #94a3b8; font-size: 11.5px; background: transparent; border: none; padding: 0px; margin: 0px; }"
            "QLineEdit { background-color: #070b14; color: #f8fafc; border: 1px solid #1e293b; border-radius: 6px; padding: 5px 8px; font-size: 11.5px; }"
            "QLineEdit:focus { border: 1px solid #00d2ff; background-color: #090f1d; }"
            "QPlainTextEdit { background-color: #070b14; color: #38bdf8; border: 1px solid #1e293b; border-radius: 6px; padding: 5px 8px; font-size: 11px; font-family: 'Consolas', monospace; }"
            "QPlainTextEdit:focus { border: 1px solid #00d2ff; background-color: #090f1d; }"
            "QComboBox { background-color: #121a2d; color: #f8fafc; border: 1px solid #1e293b; border-radius: 6px; padding: 4px 10px; font-size: 11.5px; min-height: 20px; }"
            "QComboBox:focus { border: 1px solid #00d2ff; }"
            "QComboBox QAbstractItemView { background-color: #121a2d; color: #f8fafc; selection-background-color: #00d2ff; selection-color: #000; border: 1px solid #1e293b; outline: none; }"
            "QCheckBox { color: #cbd5e1; font-size: 11.5px; spacing: 8px; background: transparent; border: none; }"
            "QCheckBox::indicator { width: 15px; height: 15px; border: 1px solid #334155; border-radius: 4px; background: #070b14; }"
            "QCheckBox::indicator:hover { border-color: #00d2ff; }"
            "QCheckBox::indicator:checked { background: #00d2ff; border-color: #00d2ff; }"
            "QPushButton { border-radius: 6px; padding: 6px 14px; font-size: 11.5px; font-weight: 600; }"
            "QTabWidget::pane { border: 1px solid rgba(255, 255, 255, 0.08); background-color: #0c1220; border-radius: 8px; }"
            "QTabBar::tab { background: #131b2e; color: #94a3b8; padding: 6px 11px; border-radius: 5px; margin: 1px 1px; font-weight: 600; font-size: 11px; }"
            "QTabBar::tab:hover { background: #1a253e; color: #f8fafc; }"
            "QTabBar::tab:selected { background: #00d2ff; color: #000000; font-weight: bold; }"
        );

        setupUI();
        loadCurrentSettings();
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
    void setupUI() {
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(14, 12, 14, 14);
        mainLayout->setSpacing(10);

        // Header Title Banner with close.png on right
        QHBoxLayout* headerLayout = new QHBoxLayout();
        headerLayout->setContentsMargins(0, 0, 0, 0);
        headerLayout->setSpacing(8);

        QLabel* lblPulse = new QLabel("⚙️", this);
        lblPulse->setStyleSheet("font-size: 16px;");
        QLabel* lblTitle = new QLabel("Options - Configuration", this);
        lblTitle->setStyleSheet("color: #ffffff; font-weight: bold; font-size: 13px;");

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

        headerLayout->addWidget(lblPulse);
        headerLayout->addWidget(lblTitle);
        headerLayout->addStretch();
        headerLayout->addWidget(closeBtn);
        mainLayout->addLayout(headerLayout);

        // Tab Widget
        QTabWidget* tabs = new QTabWidget(this);

        // Tab 1: General
        tabs->addTab(createGeneralTab(tabs), "🌐 General");

        // Tab 2: File types
        tabs->addTab(createFileTypesTab(tabs), "📁 File types");

        // Tab 3: Save to
        tabs->addTab(createSaveToTab(tabs), "💾 Save to");

        // Tab 4: Downloads
        tabs->addTab(createDownloadsTab(tabs), "⚡ Downloads");

        // Tab 5: Connection
        tabs->addTab(createConnectionTab(tabs), "📶 Connection");

        mainLayout->addWidget(tabs);

        // Bottom Action Bar
        QHBoxLayout* btnLayout = new QHBoxLayout();
        btnLayout->addStretch();

        QPushButton* cancelBtn = new QPushButton("Cancel", this);
        cancelBtn->setStyleSheet("background-color: #1e293b; color: #94a3b8; border: 1px solid #334155; min-width: 80px; padding: 7px 16px;");
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

        QPushButton* saveBtn = new QPushButton("Save Changes", this);
        saveBtn->setStyleSheet("background-color: #00d2ff; color: #000000; border: none; font-weight: bold; min-width: 120px; padding: 7px 16px;");
        saveBtn->setDefault(true);
        connect(saveBtn, &QPushButton::clicked, this, [this]() {
            saveAllSettings();
            accept();
        });

        btnLayout->addWidget(cancelBtn);
        btnLayout->addWidget(saveBtn);
        mainLayout->addLayout(btnLayout);
    }

    static QString ensureExtensionExtracted() {
        QString docLoc = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        QString targetPath = docLoc + "/PDM Extension";
        QDir targetDir(targetPath);
        if (!targetDir.exists()) {
            targetDir.mkpath(".");
        }

        QStringList candidates = {
            QCoreApplication::applicationDirPath() + "/PDM Extension",
            QCoreApplication::applicationDirPath() + "/../PDM Extension",
            QCoreApplication::applicationDirPath() + "/../../PDM Extension",
            QCoreApplication::applicationDirPath() + "/../../../PDM Extension"
        };

        QString srcPath;
        for (const QString& c : candidates) {
            if (QDir(c).exists()) {
                srcPath = QDir::cleanPath(c);
                break;
            }
        }

        if (!srcPath.isEmpty()) {
            QDir sDir(srcPath);
            for (const QFileInfo& fi : sDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
                QString destItem = targetPath + "/" + fi.fileName();
                if (fi.isDir()) {
                    QDir().mkpath(destItem);
                    QDir subDir(fi.absoluteFilePath());
                    for (const QFileInfo& subFi : subDir.entryInfoList(QDir::Files)) {
                        QString destSub = destItem + "/" + subFi.fileName();
                        if (!QFile::exists(destSub) || subFi.lastModified() > QFileInfo(destSub).lastModified()) {
                            QFile::remove(destSub);
                            QFile::copy(subFi.absoluteFilePath(), destSub);
                        }
                    }
                } else {
                    if (!QFile::exists(destItem) || fi.lastModified() > QFileInfo(destItem).lastModified()) {
                        QFile::remove(destItem);
                        QFile::copy(fi.absoluteFilePath(), destItem);
                    }
                }
            }
        }
        return targetPath;
    }

    QWidget* createGeneralTab(QWidget* parent) {
        QWidget* page = new QWidget(parent);
        QVBoxLayout* layout = new QVBoxLayout(page);
        layout->setContentsMargins(14, 14, 14, 14);
        layout->setSpacing(10);

        // 1. Extension Card
        QFrame* extCard = createCard(page);
        QVBoxLayout* extLayout = new QVBoxLayout(extCard);
        extLayout->setContentsMargins(12, 10, 12, 10);
        extLayout->setSpacing(6);

        QHBoxLayout* extHeader = new QHBoxLayout();
        QLabel* extTitle = new QLabel("🧩 Chrome & Browser Extension", extCard);
        extTitle->setStyleSheet("color: #ffffff; font-weight: bold; font-size: 12px;");
        QLabel* extBadge = new QLabel("Active", extCard);
        extBadge->setStyleSheet("background-color: rgba(16, 185, 129, 0.2); color: #34d399; border: 1px solid rgba(16, 185, 129, 0.4); border-radius: 8px; padding: 2px 8px; font-size: 10px; font-weight: bold;");
        extHeader->addWidget(extTitle);
        extHeader->addWidget(extBadge);
        extHeader->addStretch();

        QString extPath = ensureExtensionExtracted();

        QPushButton* btnOpenExt = new QPushButton("📂 Open Folder", extCard);
        btnOpenExt->setStyleSheet("background-color: #00d2ff; color: #000000; font-weight: bold; border: none; padding: 4px 12px;");
        connect(btnOpenExt, &QPushButton::clicked, this, [extPath]() {
            QDesktopServices::openUrl(QUrl::fromLocalFile(extPath));
        });
        extHeader->addWidget(btnOpenExt);
        extLayout->addLayout(extHeader);

        QLabel* extSub = new QLabel("Auto-extracted extension for YouTube 4K & direct video capture", extCard);
        extSub->setStyleSheet("color: #94a3b8; font-size: 11px;");
        extLayout->addWidget(extSub);

        QLineEdit* extPathDisplay = new QLineEdit(QDir::toNativeSeparators(extPath), extCard);
        extPathDisplay->setReadOnly(true);
        extPathDisplay->setFixedHeight(28);
        extPathDisplay->setStyleSheet("background-color: #070b14; color: #38bdf8; border: 1px solid #1e293b; border-radius: 4px; padding: 2px 8px; font-family: monospace; font-size: 11px;");
        extLayout->addWidget(extPathDisplay);

        QLabel* extTip = new QLabel("👉 Browser Setup: Open Chrome/Edge/Brave → chrome://extensions → Enable Developer Mode → Load unpacked.", extCard);
        extTip->setStyleSheet("color: #64748b; font-size: 10.5px;");
        extLayout->addWidget(extTip);
        layout->addWidget(extCard);

        // 2. System Integration Card
        QFrame* intCard = createCard(page);
        QVBoxLayout* intLayout = new QVBoxLayout(intCard);
        intLayout->setContentsMargins(12, 10, 12, 10);
        intLayout->setSpacing(8);

        QLabel* intTitle = new QLabel("🖥️ System Integration", intCard);
        intTitle->setStyleSheet("color: #ffffff; font-weight: bold; font-size: 12px;");
        intLayout->addWidget(intTitle);

        m_chkLaunchStartup = new QCheckBox("Launch Prime Download Manager on Windows startup", intCard);
        m_chkAutoClipboard = new QCheckBox("Automatically intercept & start downloading URLs copied to clipboard", intCard);
        intLayout->addWidget(m_chkLaunchStartup);
        intLayout->addWidget(m_chkAutoClipboard);
        layout->addWidget(intCard);

        // 3. Supported Browsers Card
        QFrame* browCard = createCard(page);
        QVBoxLayout* browLayout = new QVBoxLayout(browCard);
        browLayout->setContentsMargins(12, 10, 12, 10);
        browLayout->setSpacing(8);

        QHBoxLayout* browHeader = new QHBoxLayout();
        QLabel* browTitle = new QLabel("Capture downloads from supported browsers:", browCard);
        browTitle->setStyleSheet("color: #ffffff; font-weight: bold; font-size: 12px;");
        QLabel* browStatus = new QLabel("All Integrated", browCard);
        browStatus->setStyleSheet("color: #38bdf8; font-size: 11px; font-weight: 500;");
        browHeader->addWidget(browTitle);
        browHeader->addStretch();
        browHeader->addWidget(browStatus);
        browLayout->addLayout(browHeader);

        QHBoxLayout* browGrid = new QHBoxLayout();
        QLabel* b1 = new QLabel("✓ Google Chrome\n✓ Microsoft Edge\n✓ Brave Browser", browCard);
        QLabel* b2 = new QLabel("✓ Mozilla Firefox\n✓ Opera / Opera GX\n✓ Vivaldi & Chromium", browCard);
        b1->setStyleSheet("color: #10b981; font-size: 11px; line-height: 1.4;");
        b2->setStyleSheet("color: #10b981; font-size: 11px; line-height: 1.4;");
        browGrid->addWidget(b1);
        browGrid->addWidget(b2);
        browLayout->addLayout(browGrid);
        layout->addWidget(browCard);

        layout->addStretch();
        return page;
    }

    QWidget* createFileTypesTab(QWidget* parent) {
        QWidget* page = new QWidget(parent);
        QVBoxLayout* layout = new QVBoxLayout(page);
        layout->setContentsMargins(14, 14, 14, 14);
        layout->setSpacing(10);

        // 1. File Types Card
        QFrame* ftCard = createCard(page);
        QVBoxLayout* ftLayout = new QVBoxLayout(ftCard);
        ftLayout->setContentsMargins(12, 10, 12, 10);
        ftLayout->setSpacing(6);

        QHBoxLayout* ftHeader = new QHBoxLayout();
        QLabel* ftTitle = new QLabel("⚡ Automatically start downloading file types:", ftCard);
        ftTitle->setStyleSheet("color: #ffffff; font-weight: bold; font-size: 12px;");
        QPushButton* ftDefaultBtn = new QPushButton("Default", ftCard);
        ftDefaultBtn->setStyleSheet("background-color: #1e293b; color: #94a3b8; border: 1px solid #334155; padding: 2px 8px; font-size: 11px;");
        connect(ftDefaultBtn, &QPushButton::clicked, this, [this]() {
            m_txtFileTypes->setPlainText("3GP 7Z AAC ACE AIF APK ARJ ASF AVI BIN BZ2 EXE GZ GZIP IMG ISO LZH M4A M4V MKV MOV MP3 MP4 MPA MPE MPEG MPG MSI MSU OGG OGV PDF PLJ PPS PPT QT R0* R1* RA RAR RM RMVB SEA SIT SITX TAR TIF TIFF WAV WMA WMV Z ZIP");
        });
        ftHeader->addWidget(ftTitle);
        ftHeader->addStretch();
        ftHeader->addWidget(ftDefaultBtn);
        ftLayout->addLayout(ftHeader);

        m_txtFileTypes = new QPlainTextEdit(ftCard);
        m_txtFileTypes->setFixedHeight(60);
        ftLayout->addWidget(m_txtFileTypes);
        layout->addWidget(ftCard);

        // 2. Blocked Sites Card
        QFrame* bsCard = createCard(page);
        QVBoxLayout* bsLayout = new QVBoxLayout(bsCard);
        bsLayout->setContentsMargins(12, 10, 12, 10);
        bsLayout->setSpacing(6);

        QHBoxLayout* bsHeader = new QHBoxLayout();
        QLabel* bsTitle = new QLabel("🚫 Do not capture downloads from sites:", bsCard);
        bsTitle->setStyleSheet("color: #ffffff; font-weight: bold; font-size: 12px;");
        QPushButton* bsDefaultBtn = new QPushButton("Default", bsCard);
        bsDefaultBtn->setStyleSheet("background-color: #1e293b; color: #94a3b8; border: 1px solid #334155; padding: 2px 8px; font-size: 11px;");
        connect(bsDefaultBtn, &QPushButton::clicked, this, [this]() {
            m_txtBlockedSites->setPlainText("*.update.microsoft.com download.windowsupdate.com *.download.windowsupdate.com siteseal.thawte.com ecom.cimetz.com *.voice2page.com");
        });
        bsHeader->addWidget(bsTitle);
        bsHeader->addStretch();
        bsHeader->addWidget(bsDefaultBtn);
        bsLayout->addLayout(bsHeader);

        m_txtBlockedSites = new QPlainTextEdit(bsCard);
        m_txtBlockedSites->setFixedHeight(48);
        bsLayout->addWidget(m_txtBlockedSites);
        layout->addWidget(bsCard);

        // 3. Address Exceptions Card
        QFrame* excCard = createCard(page);
        QHBoxLayout* excLayout = new QHBoxLayout(excCard);
        excLayout->setContentsMargins(12, 10, 12, 10);

        QVBoxLayout* excTextLayout = new QVBoxLayout();
        QLabel* excTitle = new QLabel("Specific Address Exceptions", excCard);
        excTitle->setStyleSheet("color: #ffffff; font-weight: bold; font-size: 12px;");
        QLabel* excSub = new QLabel("Define precise URL patterns and wildcards to bypass auto-capture", excCard);
        excSub->setStyleSheet("color: #94a3b8; font-size: 11px;");
        excTextLayout->addWidget(excTitle);
        excTextLayout->addWidget(excSub);

        QPushButton* btnEditExceptions = new QPushButton("Edit list ...", excCard);
        btnEditExceptions->setStyleSheet("background-color: #1e293b; color: #38bdf8; border: 1px solid rgba(56, 189, 248, 0.4); min-width: 90px;");
        connect(btnEditExceptions, &QPushButton::clicked, this, [this]() {
            AddressExceptionsDialog dlg(m_exceptionsList, this);
            if (dlg.exec() == QDialog::Accepted) {
                m_exceptionsList = dlg.exceptions();
            }
        });

        excLayout->addLayout(excTextLayout);
        excLayout->addWidget(btnEditExceptions);
        layout->addWidget(excCard);

        layout->addStretch();
        return page;
    }

    QWidget* createSaveToTab(QWidget* parent) {
        QWidget* page = new QWidget(parent);
        QVBoxLayout* layout = new QVBoxLayout(page);
        layout->setContentsMargins(14, 14, 14, 14);
        layout->setSpacing(10);

        // 1. Default Location Card
        QFrame* locCard = createCard(page);
        QVBoxLayout* locLayout = new QVBoxLayout(locCard);
        locLayout->setContentsMargins(12, 10, 12, 10);
        locLayout->setSpacing(8);

        QLabel* locTitle = new QLabel("💾 Default Download Location", locCard);
        locTitle->setStyleSheet("color: #ffffff; font-weight: bold; font-size: 12px;");
        locLayout->addWidget(locTitle);

        QHBoxLayout* defRow = new QHBoxLayout();
        m_defSavePathEdit = new QLineEdit(locCard);
        m_defSavePathEdit->setFixedHeight(32);
        QPushButton* defBrowse = new QPushButton("Browse...", locCard);
        defBrowse->setFixedHeight(32);
        defBrowse->setStyleSheet("background-color: #00d2ff; color: #000000; font-weight: bold; border: none; min-width: 80px;");
        connect(defBrowse, &QPushButton::clicked, this, [this]() {
            QString dir = QFileDialog::getExistingDirectory(this, "Select Default Directory", m_defSavePathEdit->text());
            if (!dir.isEmpty()) m_defSavePathEdit->setText(dir);
        });
        defRow->addWidget(m_defSavePathEdit);
        defRow->addWidget(defBrowse);
        locLayout->addLayout(defRow);

        QLabel* locSub = new QLabel("All downloads & videos will be saved directly into this directory by default.", locCard);
        locSub->setStyleSheet("color: #94a3b8; font-size: 11px;");
        locLayout->addWidget(locSub);

        m_chkRememberFolder = new QCheckBox("Remember last selected download folder", locCard);
        locLayout->addWidget(m_chkRememberFolder);
        layout->addWidget(locCard);

        // 2. Temp Directory Card
        QFrame* tmpCard = createCard(page);
        QVBoxLayout* tmpLayout = new QVBoxLayout(tmpCard);
        tmpLayout->setContentsMargins(12, 10, 12, 10);
        tmpLayout->setSpacing(8);

        QLabel* tmpTitle = new QLabel("⚡ Turbo Multi-Socket Temporary Directory", tmpCard);
        tmpTitle->setStyleSheet("color: #f59e0b; font-weight: bold; font-size: 12px;");
        tmpLayout->addWidget(tmpTitle);

        QHBoxLayout* tmpRow = new QHBoxLayout();
        m_tempPathEdit = new QLineEdit(tmpCard);
        m_tempPathEdit->setFixedHeight(32);
        QPushButton* tmpBrowse = new QPushButton("Browse...", tmpCard);
        tmpBrowse->setFixedHeight(32);
        tmpBrowse->setStyleSheet("background-color: #1e293b; color: #cbd5e1; border: 1px solid #334155; min-width: 80px;");
        connect(tmpBrowse, &QPushButton::clicked, this, [this]() {
            QString dir = QFileDialog::getExistingDirectory(this, "Select Temporary Assembly Directory", m_tempPathEdit->text());
            if (!dir.isEmpty()) m_tempPathEdit->setText(dir);
        });
        tmpRow->addWidget(m_tempPathEdit);
        tmpRow->addWidget(tmpBrowse);
        tmpLayout->addLayout(tmpRow);

        QLabel* tmpSub = new QLabel("Required for multi-socket parallel assembly cache. Using an SSD gives maximum speed.", tmpCard);
        tmpSub->setStyleSheet("color: #94a3b8; font-size: 11px;");
        tmpLayout->addWidget(tmpSub);
        layout->addWidget(tmpCard);

        m_chkSetServerDate = new QCheckBox("Set file creation date as provided by the remote server", page);
        layout->addWidget(m_chkSetServerDate);

        layout->addStretch();
        return page;
    }

    QWidget* createDownloadsTab(QWidget* parent) {
        QWidget* page = new QWidget(parent);
        QVBoxLayout* layout = new QVBoxLayout(page);
        layout->setContentsMargins(14, 14, 14, 14);
        layout->setSpacing(8);

        // 1. Dialog Preferences
        QFrame* dlgCard = createCard(page);
        QVBoxLayout* dlgLayout = new QVBoxLayout(dlgCard);
        dlgLayout->setContentsMargins(12, 8, 12, 8);
        dlgLayout->setSpacing(5);

        QLabel* dlgTitle = new QLabel("🔔 Dialog & UI Preferences", dlgCard);
        dlgTitle->setStyleSheet("color: #ffffff; font-weight: bold; font-size: 12px;");
        dlgLayout->addWidget(dlgTitle);

        m_chkShowStartDlg = new QCheckBox("Show start download dialog", dlgCard);
        m_chkQueueOnly = new QCheckBox("Do not start downloading, only add files to queue", dlgCard);
        m_chkShowCompleteDlg = new QCheckBox("Show download complete notification dialog", dlgCard);

        dlgLayout->addWidget(m_chkShowStartDlg);
        dlgLayout->addWidget(m_chkQueueOnly);
        dlgLayout->addWidget(m_chkShowCompleteDlg);
        layout->addWidget(dlgCard);

        // 2. Execution Behavior
        QFrame* execCard = createCard(page);
        QVBoxLayout* execLayout = new QVBoxLayout(execCard);
        execLayout->setContentsMargins(12, 8, 12, 8);
        execLayout->setSpacing(5);

        QLabel* execTitle = new QLabel("⚙️ Execution Behavior", execCard);
        execTitle->setStyleSheet("color: #ffffff; font-weight: bold; font-size: 12px;");
        execLayout->addWidget(execTitle);

        m_chkStartImmediately = new QCheckBox("Start downloading immediately while displaying 'File Info' dialog", execCard);
        m_chkQueuePanelLater = new QCheckBox("Show queue selection panel on pressing 'Download Later'", execCard);
        m_chkIgnoreModTime = new QCheckBox("Ignore file modification time changes when resuming a download", execCard);

        execLayout->addWidget(m_chkStartImmediately);
        execLayout->addWidget(m_chkQueuePanelLater);
        execLayout->addWidget(m_chkIgnoreModTime);
        layout->addWidget(execCard);

        // 3. Duplicates & User Agent
        QFrame* dupCard = createCard(page);
        QVBoxLayout* dupLayout = new QVBoxLayout(dupCard);
        dupLayout->setContentsMargins(12, 8, 12, 8);
        dupLayout->setSpacing(5);

        QLabel* dupTitle = new QLabel("If duplicate link is added:", dupCard);
        dupTitle->setStyleSheet("color: #ffffff; font-weight: bold; font-size: 12px;");
        dupLayout->addWidget(dupTitle);

        m_cmbDuplicateAction = new QComboBox(dupCard);
        m_cmbDuplicateAction->setFixedHeight(28);
        m_cmbDuplicateAction->addItems({
            "Show a dialog and ask what to do.",
            "Add the duplicate with a numbered file name",
            "Add the duplicate and overwrite the existing file",
            "If existing file is complete, show download complete dialog; otherwise resume it."
        });
        dupLayout->addWidget(m_cmbDuplicateAction);

        QLabel* uaTitle = new QLabel("User-Agent for manually added downloads:", dupCard);
        uaTitle->setStyleSheet("color: #ffffff; font-weight: bold; font-size: 12px; margin-top: 2px;");
        dupLayout->addWidget(uaTitle);

        m_txtUserAgent = new QLineEdit(dupCard);
        m_txtUserAgent->setFixedHeight(28);
        dupLayout->addWidget(m_txtUserAgent);
        layout->addWidget(dupCard);

        layout->addStretch();
        return page;
    }

    QWidget* createConnectionTab(QWidget* parent) {
        QWidget* page = new QWidget(parent);
        QVBoxLayout* layout = new QVBoxLayout(page);
        layout->setContentsMargins(14, 14, 14, 14);
        layout->setSpacing(10);

        QFrame* conCard = createCard(page);
        QVBoxLayout* conLayout = new QVBoxLayout(conCard);
        conLayout->setContentsMargins(12, 12, 12, 12);
        conLayout->setSpacing(12);

        QHBoxLayout* conHeader = new QHBoxLayout();
        QLabel* conIcon = new QLabel("📶", conCard);
        conIcon->setStyleSheet("font-size: 20px; color: #00d2ff;");
        QVBoxLayout* conTitleLayout = new QVBoxLayout();
        conTitleLayout->setSpacing(2);
        QLabel* conTitle = new QLabel("High-Speed Multi-Thread Engine", conCard);
        conTitle->setStyleSheet("color: #ffffff; font-weight: bold; font-size: 13px;");
        QLabel* conSub = new QLabel("Turbo range socket allocation", conCard);
        conSub->setStyleSheet("color: #94a3b8; font-size: 11px;");
        conTitleLayout->addWidget(conTitle);
        conTitleLayout->addWidget(conSub);
        conHeader->addWidget(conIcon);
        conHeader->addLayout(conTitleLayout);
        conHeader->addStretch();
        conLayout->addLayout(conHeader);

        QHBoxLayout* thRow = new QHBoxLayout();
        QLabel* thTitle = new QLabel("Default max. connections:", conCard);
        thTitle->setStyleSheet("color: #ffffff; font-weight: bold; font-size: 12px;");
        m_cmbThreads = new QComboBox(conCard);
        m_cmbThreads->setFixedHeight(30);
        m_cmbThreads->addItems({"32 Threads", "24 Threads", "16 Threads", "8 Threads", "4 Threads", "2 Threads", "1 Thread"});
        m_cmbThreads->setStyleSheet("background-color: #070b14; color: #00d2ff; font-weight: bold; font-family: monospace; border: 1px solid rgba(0, 210, 255, 0.4);");
        thRow->addWidget(thTitle);
        thRow->addStretch();
        thRow->addWidget(m_cmbThreads);
        conLayout->addLayout(thRow);

        QFrame* turboNote = new QFrame(conCard);
        turboNote->setObjectName("turboNote");
        turboNote->setStyleSheet(
            "#turboNote { background-color: rgba(6, 182, 212, 0.08); border: 1px solid rgba(6, 182, 212, 0.25); border-radius: 6px; padding: 8px; }"
            "QLabel { background: transparent; border: none; padding: 0px; }"
        );
        QHBoxLayout* noteLayout = new QHBoxLayout(turboNote);
        QLabel* noteIcon = new QLabel("💡", turboNote);
        noteIcon->setStyleSheet("font-size: 16px;");
        QLabel* noteText = new QLabel("Turbo Sockets: 32 connections splits large videos and files into 32 simultaneous parallel byte ranges, saturating 100% of your internet bandwidth.", turboNote);
        noteText->setWordWrap(true);
        noteText->setStyleSheet("color: #7dd3fc; font-size: 11px;");
        noteLayout->addWidget(noteIcon);
        noteLayout->addWidget(noteText, 1);
        conLayout->addWidget(turboNote);

        layout->addWidget(conCard);
        layout->addStretch();
        return page;
    }

    QFrame* createCard(QWidget* parent) {
        QFrame* card = new QFrame(parent);
        card->setObjectName("settingCard");
        return card;
    }

    void loadCurrentSettings() {
        auto& mgr = DownloadManager::instance();
        m_defSavePathEdit->setText(mgr.defaultSavePath());
        m_tempPathEdit->setText(mgr.tempPath());

        // Threads
        int th = mgr.defaultThreads();
        QString thText = QString("%1 Threads").arg(th);
        int idx = m_cmbThreads->findText(thText);
        if (idx != -1) m_cmbThreads->setCurrentIndex(idx);
        else m_cmbThreads->setCurrentIndex(0);

        m_chkLaunchStartup->setChecked(mgr.launchStartup());
        m_chkAutoClipboard->setChecked(mgr.autoClipboard());
        m_txtFileTypes->setPlainText(mgr.fileTypes());
        m_txtBlockedSites->setPlainText(mgr.blockedSites());
        m_chkShowStartDlg->setChecked(mgr.showStartDlg());
        m_chkQueueOnly->setChecked(mgr.queueOnly());
        m_chkShowCompleteDlg->setChecked(mgr.showCompleteDlg());
        m_chkStartImmediately->setChecked(mgr.startImmediately());
        m_chkQueuePanelLater->setChecked(true);
        m_chkRememberFolder->setChecked(true);
        m_chkSetServerDate->setChecked(false);
        m_chkIgnoreModTime->setChecked(false);

        int dupIdx = m_cmbDuplicateAction->findText(mgr.duplicateAction());
        if (dupIdx != -1) m_cmbDuplicateAction->setCurrentIndex(dupIdx);

        m_txtUserAgent->setText(mgr.userAgent());
        m_exceptionsList = mgr.addressExceptions();
    }

    void saveAllSettings() {
        auto& mgr = DownloadManager::instance();
        mgr.setDefaultSavePath(m_defSavePathEdit->text().trimmed());
        mgr.setTempPath(m_tempPathEdit->text().trimmed());

        QString thStr = m_cmbThreads->currentText();
        int th = thStr.split(" ").first().toInt();
        if (th <= 0) th = 8;
        mgr.setDefaultThreads(th);

        mgr.setLaunchStartup(m_chkLaunchStartup->isChecked());
        mgr.setAutoClipboard(m_chkAutoClipboard->isChecked());
        mgr.setFileTypes(m_txtFileTypes->toPlainText().trimmed());
        mgr.setBlockedSites(m_txtBlockedSites->toPlainText().trimmed());
        mgr.setShowStartDlg(m_chkShowStartDlg->isChecked());
        mgr.setQueueOnly(m_chkQueueOnly->isChecked());
        mgr.setShowCompleteDlg(m_chkShowCompleteDlg->isChecked());
        mgr.setStartImmediately(m_chkStartImmediately->isChecked());
        mgr.setDuplicateAction(m_cmbDuplicateAction->currentText());
        mgr.setUserAgent(m_txtUserAgent->text().trimmed());
        mgr.setAddressExceptions(m_exceptionsList);
    }

    // UI Controls
    QLineEdit* m_defSavePathEdit = nullptr;
    QLineEdit* m_tempPathEdit = nullptr;
    QCheckBox* m_chkRememberFolder = nullptr;
    QCheckBox* m_chkSetServerDate = nullptr;

    QCheckBox* m_chkLaunchStartup = nullptr;
    QCheckBox* m_chkAutoClipboard = nullptr;

    QPlainTextEdit* m_txtFileTypes = nullptr;
    QPlainTextEdit* m_txtBlockedSites = nullptr;
    QStringList m_exceptionsList;

    QCheckBox* m_chkShowStartDlg = nullptr;
    QCheckBox* m_chkQueueOnly = nullptr;
    QCheckBox* m_chkShowCompleteDlg = nullptr;
    QCheckBox* m_chkStartImmediately = nullptr;
    QCheckBox* m_chkQueuePanelLater = nullptr;
    QCheckBox* m_chkIgnoreModTime = nullptr;
    QComboBox* m_cmbDuplicateAction = nullptr;
    QLineEdit* m_txtUserAgent = nullptr;

    QComboBox* m_cmbThreads = nullptr;
    QPoint m_dragPos;
};
