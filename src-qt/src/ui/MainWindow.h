#pragma once
#include <QMainWindow>
#include <QDialog>
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QListWidget>
#include <QTableView>
#include <QHeaderView>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QProgressBar>
#include <QPainter>
#include <QPixmap>
#include <QIcon>
#include <QMenu>
#include <QDesktopServices>
#include <QProcess>
#include <QUrl>
#include <QMessageBox>
#include <QMouseEvent>
#include <QClipboard>
#include <QShortcut>
#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QTimer>
#include <QFileInfo>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QCloseEvent>
#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#endif
#include "../core/DownloadManager.h"
#include "../core/CategoryRules.h"
#include "../ipc/ExtensionServer.h"
#include "TaskTableModel.h"
#include "ProgressBarDelegate.h"
#include "DownloadDialog.h"
#include "OptionsDialog.h"
#include "AboutDialog.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("Prime Download Manager 3.0.46");
        setWindowIcon(QIcon(":/icons/app_logo.png"));
        setMinimumSize(935, 520);
        resize(935, 520);
        setAttribute(Qt::WA_OpaquePaintEvent, true);

        DownloadManager::instance().init();
        setupUI();
        setupTrayIcon();
        setupExtensionServer();
        updateSidebarCounts();
        checkEmptyState();
    }

    ~MainWindow() override {
        if (m_extServer) {
            m_extServer->stop();
        }
        DownloadManager::instance().shutdown();
    }

    void showWindow() {
        if (isMinimized()) {
            showNormal();
        }
        resize(935, 520);
        show();
        raise();
        activateWindow();
#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(winId());
        if (hwnd) {
            ::ShowWindow(hwnd, SW_SHOWNORMAL);
            ::SetWindowPos(hwnd, HWND_TOP, 0, 0, 935, 520, SWP_NOMOVE | SWP_SHOWWINDOW);
            ::SetForegroundWindow(hwnd);
            ::BringWindowToTop(hwnd);
        }
#endif
        onTick();
        updateSidebarCounts();
        checkEmptyState();
        update();
    }

    void exitApplication() {
        m_reallyQuit = true;
        if (m_extServer) {
            m_extServer->stop();
        }
        DownloadManager::instance().shutdown();
        if (m_trayIcon) {
            m_trayIcon->hide();
        }
        qApp->quit();
    }

protected:
    void closeEvent(QCloseEvent* event) override {
        if (!m_reallyQuit) {
            event->ignore();
            hide();
            static bool s_balloonShown = false;
            if (!s_balloonShown && m_trayIcon && m_trayIcon->isVisible()) {
                m_trayIcon->showMessage(
                    "Prime Download Manager",
                    "Application is minimized to the system tray and running in the background.\nClick or double-click the tray icon to restore.",
                    QSystemTrayIcon::Information,
                    2500
                );
                s_balloonShown = true;
            }
        } else {
            DownloadManager::instance().flushSaveTasks();
            if (m_trayIcon) {
                m_trayIcon->hide();
            }
            event->accept();
        }
    }

private:
    static QIcon createGlyphIcon(const QString& glyph, const QColor& color) {
        QPixmap pix(48, 48);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::TextAntialiasing);
        p.setPen(color);
        QFont f = p.font();
        f.setPointSize(16);
        f.setBold(true);
        p.setFont(f);
        p.drawText(pix.rect(), Qt::AlignCenter, glyph);
        return QIcon(pix);
    }

    static QIcon getToolbarIcon(const QString& iconName, const QString& fallbackGlyph, const QColor& fallbackColor) {
        if (!iconName.isEmpty()) {
            // Check Qt embedded resource
            QString resPath = QString(":/icons/%1").arg(iconName);
            if (QFile::exists(resPath)) {
                return QIcon(resPath);
            }
            // Check applicationDirPath / resources / icons
            QString appDirPath = QCoreApplication::applicationDirPath() + "/resources/icons/" + iconName;
            if (QFile::exists(appDirPath)) {
                return QIcon(appDirPath);
            }
            // Check working directory relative path
            QString cwdPath = "resources/icons/" + iconName;
            if (QFile::exists(cwdPath)) {
                return QIcon(cwdPath);
            }
        }
        return createGlyphIcon(fallbackGlyph, fallbackColor);
    }

    QToolButton* createToolbarButton(const QString& text, const QString& iconName, const QString& fallbackGlyph, const QString& hexColor, const char* slot, const QString& tooltip = "") {
        QToolButton* btn = new QToolButton(this);
        btn->setText(text);
        btn->setToolTip(tooltip.isEmpty() ? text : tooltip);
        btn->setIcon(getToolbarIcon(iconName, fallbackGlyph, QColor(hexColor)));
        btn->setIconSize(QSize(24, 24));
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedSize(68, 56);
        btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        btn->setStyleSheet(QString(
            "QToolButton { color: #e2e8f0; font-size: 10px; font-weight: 600; border: 1px solid transparent; border-radius: 6px; padding: 2px 1px; background: transparent; text-align: center; }"
            "QToolButton:hover { color: #ffffff; background: rgba(255, 255, 255, 0.08); border: 1px solid rgba(255, 255, 255, 0.12); }"
            "QToolButton:pressed { background: rgba(255, 255, 255, 0.16); }"
        ));
        connect(btn, SIGNAL(clicked()), this, slot);
        return btn;
    }

    static QFrame* createToolbarSeparator(QWidget* parent) {
        QFrame* sep = new QFrame(parent);
        sep->setFrameShape(QFrame::VLine);
        sep->setFrameShadow(QFrame::Plain);
        sep->setFixedHeight(36);
        sep->setStyleSheet("background-color: rgba(255, 255, 255, 0.12); width: 1px; margin: 0px 4px;");
        return sep;
    }

    void setupUI() {
        QWidget* central = new QWidget(this);
        central->setStyleSheet("background-color: #0b0f19;");
        QVBoxLayout* rootLayout = new QVBoxLayout(central);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(0);

        // ================= TOP TOOLBAR =================
        QWidget* toolbarWidget = new QWidget(this);
        toolbarWidget->setFixedHeight(66);
        toolbarWidget->setStyleSheet("background-color: #101624; border-bottom: 1px solid rgba(255, 255, 255, 0.08);");
        QHBoxLayout* tbLayout = new QHBoxLayout(toolbarWidget);
        tbLayout->setContentsMargins(12, 4, 16, 4);
        tbLayout->setSpacing(4);
        tbLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        // --- Category 1: Download Tasks / Control ---
        tbLayout->addWidget(createToolbarButton("Add URL", "add_url.png", "+", "#38bdf8", SLOT(onAddUrl())));
        tbLayout->addWidget(createToolbarButton("Resume", "resume.png", "▶", "#34d399", SLOT(onResumeSelected())));
        tbLayout->addWidget(createToolbarButton("Stop", "stop.png", "⏹", "#f87171", SLOT(onStopSelected())));
        tbLayout->addWidget(createToolbarButton("Stop All", "stop_all.png", "✕", "#ef4444", SLOT(onStopAll())));

        // Divider
        tbLayout->addWidget(createToolbarSeparator(this));

        // --- Category 2: Task Management ---
        tbLayout->addWidget(createToolbarButton("Delete", "delete.png", "🗑", "#f43f5e", SLOT(onDeleteSelected())));
        tbLayout->addWidget(createToolbarButton("Delete Co...", "delete_clear.png", "🧹", "#f59e0b", SLOT(onClearDone()), "Delete Completed"));

        // Divider
        tbLayout->addWidget(createToolbarSeparator(this));

        // --- Category 3: System & Tools ---
        tbLayout->addWidget(createToolbarButton("Options", "options.png", "⚙", "#f97316", SLOT(onOptions())));
        tbLayout->addWidget(createToolbarButton("About", "", "ⓘ", "#38bdf8", SLOT(onAbout())));

        tbLayout->addStretch();

        // ================= TOP RIGHT CORNER: PREMIUM SPEED & THREADS DOCK =================
        QFrame* statusBox = new QFrame(this);
        statusBox->setObjectName("statusDock");
        statusBox->setFixedHeight(48);
        statusBox->setStyleSheet(
            "QFrame#statusDock { "
            "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #131c2e, stop:1 #0a101d); "
            "    border: 1px solid rgba(56, 189, 248, 0.28); "
            "    border-radius: 8px; "
            "} "
        );
        QHBoxLayout* dockLayout = new QHBoxLayout(statusBox);
        dockLayout->setContentsMargins(12, 4, 12, 4);
        dockLayout->setSpacing(10);
        dockLayout->setAlignment(Qt::AlignVCenter);

        // --- 1. Active Threads Section ---
        QWidget* threadsWidget = new QWidget(statusBox);
        threadsWidget->setStyleSheet("background: transparent; border: none;");
        QVBoxLayout* threadsLayout = new QVBoxLayout(threadsWidget);
        threadsLayout->setContentsMargins(0, 0, 0, 0);
        threadsLayout->setSpacing(3);

        QHBoxLayout* thHeader = new QHBoxLayout();
        thHeader->setSpacing(4);
        QLabel* lblThIcon = new QLabel("⚡", threadsWidget);
        lblThIcon->setStyleSheet("color: #38bdf8; font-size: 11px; border: none; background: transparent;");
        QLabel* lblThTitle = new QLabel("THREADS", threadsWidget);
        lblThTitle->setStyleSheet("color: #94a3b8; font-size: 9px; font-weight: 700; letter-spacing: 1px; border: none; background: transparent;");
        m_lblThreadsCount = new QLabel(QString("0 / %1").arg(DownloadManager::instance().defaultThreads()), threadsWidget);
        m_lblThreadsCount->setStyleSheet("color: #64748b; font-size: 11px; font-weight: bold; font-family: 'Segoe UI', monospace; border: none; background: transparent;");

        thHeader->addWidget(lblThIcon);
        thHeader->addWidget(lblThTitle);
        thHeader->addSpacing(4);
        thHeader->addWidget(m_lblThreadsCount);

        m_threadsProgress = new QProgressBar(threadsWidget);
        m_threadsProgress->setRange(0, DownloadManager::instance().defaultThreads());
        m_threadsProgress->setValue(0);
        m_threadsProgress->setTextVisible(false);
        m_threadsProgress->setFixedHeight(3);
        m_threadsProgress->setFixedWidth(95);
        m_threadsProgress->setStyleSheet(
            "QProgressBar { background: #1e293b; border: none; border-radius: 1px; }"
            "QProgressBar::chunk { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #06b6d4, stop:1 #38bdf8); border-radius: 1px; }"
        );

        threadsLayout->addLayout(thHeader);
        threadsLayout->addWidget(m_threadsProgress);
        dockLayout->addWidget(threadsWidget);

        // --- Vertical Divider inside Dock ---
        QFrame* dockDivider = new QFrame(statusBox);
        dockDivider->setFrameShape(QFrame::VLine);
        dockDivider->setFrameShadow(QFrame::Plain);
        dockDivider->setFixedHeight(28);
        dockDivider->setStyleSheet("background-color: rgba(255, 255, 255, 0.12); width: 1px; margin: 0px 2px;");
        dockLayout->addWidget(dockDivider);

        // --- 2. Total Speed Section ---
        QWidget* speedWidget = new QWidget(statusBox);
        speedWidget->setStyleSheet("background: transparent; border: none;");
        QVBoxLayout* speedVLayout = new QVBoxLayout(speedWidget);
        speedVLayout->setContentsMargins(0, 0, 0, 0);
        speedVLayout->setSpacing(1);

        QHBoxLayout* spHeader = new QHBoxLayout();
        spHeader->setSpacing(4);
        QLabel* lblSpIcon = new QLabel("🚀", speedWidget);
        lblSpIcon->setStyleSheet("font-size: 10px; border: none; background: transparent;");
        QLabel* lblSpeedTitle = new QLabel("TOTAL SPEED", speedWidget);
        lblSpeedTitle->setStyleSheet("color: #94a3b8; font-size: 9px; font-weight: 700; letter-spacing: 1px; border: none; background: transparent;");
        spHeader->addWidget(lblSpIcon);
        spHeader->addWidget(lblSpeedTitle);
        spHeader->addStretch();

        m_lblTotalSpeed = new QLabel("0 KB/s", speedWidget);
        m_lblTotalSpeed->setStyleSheet("color: #64748b; font-weight: bold; font-family: 'Segoe UI', monospace; font-size: 13px; border: none; background: transparent;");

        speedVLayout->addLayout(spHeader);
        speedVLayout->addWidget(m_lblTotalSpeed);
        dockLayout->addWidget(speedWidget);

        tbLayout->addWidget(statusBox);

        rootLayout->addWidget(toolbarWidget);

        // ================= MAIN SPLIT VIEW =================
        QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
        splitter->setStyleSheet(
            "QSplitter::handle { background-color: rgba(255, 255, 255, 0.06); width: 1px; }"
        );

        // LEFT SIDEBAR
        QWidget* sidebarWidget = new QWidget(this);
        sidebarWidget->setFixedWidth(145);
        sidebarWidget->setMinimumWidth(145);
        sidebarWidget->setMaximumWidth(145);
        sidebarWidget->setStyleSheet("background-color: #0b0f19; border-right: 1px solid rgba(255, 255, 255, 0.08);");
        QVBoxLayout* sideLayout = new QVBoxLayout(sidebarWidget);
        sideLayout->setContentsMargins(6, 10, 6, 6);
        sideLayout->setSpacing(4);

        // Tasks Header
        QLabel* navHeader = new QLabel("TASKS", sidebarWidget);
        navHeader->setStyleSheet("color: #64748b; font-size: 10px; font-weight: bold; letter-spacing: 1px; padding-left: 6px;");
        sideLayout->addWidget(navHeader);

        m_statusList = new QListWidget(sidebarWidget);
        m_statusList->setStyleSheet(
            "QListWidget { background: transparent; border: none; outline: none; }"
            "QListWidget::item { color: #cbd5e1; font-size: 11px; padding: 6px 6px; border-radius: 4px; margin-bottom: 2px; }"
            "QListWidget::item:hover { background: rgba(255, 255, 255, 0.05); color: #ffffff; }"
            "QListWidget::item:selected { background: rgba(6, 182, 212, 0.18); color: #22d3ee; font-weight: bold; }"
        );

        addSidebarItem(m_statusList, "📁 All Downloads", "all", "#22d3ee");
        addSidebarItem(m_statusList, "⏳ Incomplete", "Incomplete", "#f59e0b");
        addSidebarItem(m_statusList, "✓ Finished", "Finished", "#10b981");
        connect(m_statusList, &QListWidget::currentRowChanged, this, [this](int row) {
            if (row < 0 || !m_model) return;
            QListWidgetItem* it = m_statusList->item(row);
            if (it) {
                QString filter = it->data(Qt::UserRole).toString();
                m_model->setFilter(filter);
                checkEmptyState();
            }
        });

        sideLayout->addWidget(m_statusList);
        sideLayout->addStretch();

        splitter->addWidget(sidebarWidget);

        // RIGHT MAIN CONTENT AREA (STACKED WIDGET: EMPTY STATE OR TABLE)
        m_stackedWidget = new QStackedWidget(splitter);
        m_stackedWidget->setStyleSheet("background-color: #0d121d;");

        // 1. Empty State Page
        m_emptyStateWidget = new QWidget(m_stackedWidget);
        QVBoxLayout* emptyLayout = new QVBoxLayout(m_emptyStateWidget);
        emptyLayout->setAlignment(Qt::AlignCenter);
        emptyLayout->setSpacing(10);

        QLabel* emptyIcon = new QLabel(m_emptyStateWidget);
        emptyIcon->setPixmap(createGlyphIcon("📥", QColor("#f43f5e")).pixmap(52, 52));
        emptyIcon->setAlignment(Qt::AlignCenter);

        QLabel* emptyTitle = new QLabel("No active downloads", m_emptyStateWidget);
        emptyTitle->setStyleSheet("color: #94a3b8; font-size: 16px; font-weight: bold;");
        emptyTitle->setAlignment(Qt::AlignCenter);

        QLabel* emptySubtitle = new QLabel("Click \"Add URL\" above or copy a download link to get started", m_emptyStateWidget);
        emptySubtitle->setStyleSheet("color: #475569; font-size: 12px;");
        emptySubtitle->setAlignment(Qt::AlignCenter);

        emptyLayout->addStretch();
        emptyLayout->addWidget(emptyIcon);
        emptyLayout->addWidget(emptyTitle);
        emptyLayout->addWidget(emptySubtitle);
        emptyLayout->addStretch();

        m_stackedWidget->addWidget(m_emptyStateWidget);

        // 2. Table View Page
        m_table = new QTableView(m_stackedWidget);
        m_model = new TaskTableModel(this);
        m_table->setModel(m_model);
        m_table->setItemDelegate(new ProgressBarDelegate(this));
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setSelectionMode(QAbstractItemView::SingleSelection);
        m_table->setAlternatingRowColors(true);
        m_table->setShowGrid(true);
        m_table->setGridStyle(Qt::SolidLine);
        m_table->verticalHeader()->setVisible(false);
        m_table->verticalHeader()->setDefaultSectionSize(30);
        m_table->setContextMenuPolicy(Qt::CustomContextMenu);
        m_table->setFocusPolicy(Qt::NoFocus);

        m_table->setStyleSheet(
            "QTableView { background-color: #0b101b; alternate-background-color: #0f1626; border: 1px solid rgba(255, 255, 255, 0.08); gridline-color: rgba(255, 255, 255, 0.08); outline: none; color: #e2e8f0; font-size: 12px; selection-background-color: rgba(6, 182, 212, 0.22); selection-color: #ffffff; }"
            "QHeaderView { background-color: #0e1524; border: none; }"
            "QHeaderView::section { background-color: #0e1524; color: #94a3b8; font-size: 10px; font-weight: bold; border: none; border-right: 1px solid rgba(255, 255, 255, 0.08); border-bottom: 1px solid rgba(255, 255, 255, 0.14); padding: 7px 10px; text-transform: uppercase; text-align: left; }"
            "QTableView::item { padding: 0px 8px; border: none; outline: none; }"
            "QTableView::item:focus { border: none; outline: none; }"
            "QTableView::item:selected { border: none; outline: none; background-color: rgba(6, 182, 212, 0.22); }"
        );

        QHeaderView* header = m_table->horizontalHeader();
        header->setSectionsMovable(true);
        header->setFirstSectionMovable(true);
        header->setSectionsClickable(true);
        header->setHighlightSections(false);
        header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        header->setTextElideMode(Qt::ElideNone);
        header->setMinimumSectionSize(0);
        header->setStretchLastSection(false);

        auto updateStretchPolicy = [header]() {
            if (header->isSectionHidden(0)) {
                header->setStretchLastSection(true);
            } else {
                header->setStretchLastSection(false);
                header->setSectionResizeMode(0, QHeaderView::Stretch);
            }
        };

        header->setSectionResizeMode(0, QHeaderView::Stretch);
        for (int i = 1; i < 6; ++i) {
            header->setSectionResizeMode(i, QHeaderView::Interactive);
        }
        m_table->setColumnWidth(1, 150); // SIZE (fits downloaded / total)
        m_table->setColumnWidth(2, 100); // STATUS
        m_table->setColumnWidth(3, 95);  // SPEED
        m_table->setColumnWidth(4, 90);  // TIME LEFT
        m_table->setColumnWidth(5, 95);  // PROGRESS (compact)

        // Restore saved column layout (positions and widths)
        QSettings headerSettings("PrimeDownloadManager", "PDM");
        int headerVer = headerSettings.value("tableHeaderVersion", 1).toInt();
        if (headerVer < 2) {
            // Upgrade to version 2 (harmonious layout with compact progress and stretching File Name)
            headerSettings.remove("tableHeaderState");
            headerSettings.setValue("tableHeaderVersion", 2);
        } else {
            QByteArray savedHeaderState = headerSettings.value("tableHeaderState").toByteArray();
            if (!savedHeaderState.isEmpty()) {
                header->restoreState(savedHeaderState);
                for (int i = 0; i < 6; ++i) {
                    if (!header->isSectionHidden(i) && m_table->columnWidth(i) <= 25) {
                        header->setSectionHidden(i, true);
                    }
                }
            }
        }
        updateStretchPolicy();

        // Auto-save column order whenever user drags/swaps or resizes columns
        connect(header, &QHeaderView::sectionMoved, this, [header]() {
            QSettings s("PrimeDownloadManager", "PDM");
            s.setValue("tableHeaderState", header->saveState());
        });
        connect(header, &QHeaderView::sectionResized, this, [this, header, updateStretchPolicy](int logicalIndex, int oldSize, int newSize) {
            if (newSize <= 25 && !header->isSectionHidden(logicalIndex)) {
                header->setSectionHidden(logicalIndex, true);
                updateStretchPolicy();
            }
            QSettings s("PrimeDownloadManager", "PDM");
            s.setValue("tableHeaderState", header->saveState());
        });

        // Double-clicking column separator restores hidden column like IDM
        connect(header, &QHeaderView::sectionHandleDoubleClicked, this, [this, header, updateStretchPolicy](int logicalIndex) {
            bool changed = false;
            for (int i = 0; i < 6; ++i) {
                if (header->isSectionHidden(i)) {
                    header->setSectionHidden(i, false);
                    int defaultWidths[] = {320, 150, 100, 95, 90, 95};
                    if (i != 0) {
                        m_table->setColumnWidth(i, defaultWidths[i]);
                    }
                    changed = true;
                    break;
                }
            }
            if (changed) {
                updateStretchPolicy();
                QSettings s("PrimeDownloadManager", "PDM");
                s.setValue("tableHeaderState", header->saveState());
            }
        });

        // Context menu on table header allowing user to toggle column visibility or reset layout anytime
        header->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(header, &QHeaderView::customContextMenuRequested, this, [this, header, updateStretchPolicy](const QPoint& pos) {
            QMenu menu(this);
            menu.setStyleSheet(
                "QMenu { background-color: #101625; color: #f8fafc; border: 1px solid #1e293b; border-radius: 6px; padding: 4px; }"
                "QMenu::item { padding: 6px 14px; border-radius: 4px; font-size: 11px; }"
                "QMenu::item:selected { background-color: #0284c7; color: #ffffff; }"
            );

            const QStringList colNames = {"File Name", "Size", "Status", "Speed", "Time Left", "Progress"};
            for (int i = 0; i < colNames.size(); ++i) {
                QAction* act = menu.addAction(colNames[i]);
                act->setCheckable(true);
                act->setChecked(!header->isSectionHidden(i) && m_table->columnWidth(i) > 25);
                connect(act, &QAction::toggled, this, [this, header, i, updateStretchPolicy](bool visible) {
                    if (visible) {
                        header->setSectionHidden(i, false);
                        if (m_table->columnWidth(i) <= 25) {
                            int defaultWidths[] = {320, 150, 100, 95, 90, 95};
                            if (i != 0) {
                                m_table->setColumnWidth(i, defaultWidths[i]);
                            }
                        }
                    } else {
                        header->setSectionHidden(i, true);
                    }
                    updateStretchPolicy();
                    QSettings s("PrimeDownloadManager", "PDM");
                    s.setValue("tableHeaderState", header->saveState());
                });
            }

            menu.addSeparator();

            menu.addAction("Reset Column Order & Sizes", [this, header, updateStretchPolicy]() {
                for (int i = 0; i < 6; ++i) {
                    header->setSectionHidden(i, false);
                    header->moveSection(header->visualIndex(i), i);
                }
                header->setStretchLastSection(false);
                header->setSectionResizeMode(0, QHeaderView::Stretch);
                for (int i = 1; i < 6; ++i) {
                    header->setSectionResizeMode(i, QHeaderView::Interactive);
                }
                int defaultWidths[] = {320, 150, 100, 95, 90, 95};
                for (int i = 1; i < 6; ++i) {
                    m_table->setColumnWidth(i, defaultWidths[i]);
                }
                updateStretchPolicy();
                QSettings s("PrimeDownloadManager", "PDM");
                s.remove("tableHeaderState");
                s.setValue("tableHeaderVersion", 2);
            });
            menu.exec(header->mapToGlobal(pos));
        });

        connect(m_table, &QTableView::customContextMenuRequested, this, &MainWindow::onTableContextMenu);
        connect(m_table, &QTableView::doubleClicked, this, &MainWindow::onTableDoubleClicked);

        QShortcut* deleteKeyShortcut = new QShortcut(QKeySequence::Delete, this);
        connect(deleteKeyShortcut, &QShortcut::activated, this, &MainWindow::onDeleteSelected);

        m_stackedWidget->addWidget(m_table);
        splitter->addWidget(m_stackedWidget);
        splitter->setCollapsible(0, false);
        splitter->setCollapsible(1, false);
        splitter->setStretchFactor(0, 0);
        splitter->setStretchFactor(1, 1);
        splitter->setSizes({145, 790});

        rootLayout->addWidget(splitter);
        setCentralWidget(central);

        // Update timer for speeds and counts
        QTimer* timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &MainWindow::onTick);
        timer->start(1000);

        // Connect manager events
        auto& mgr = DownloadManager::instance();
        connect(&mgr, &DownloadManager::taskAdded, this, [this](const DownloadTask&) {
            checkEmptyState();
            updateSidebarCounts();
        });
        connect(&mgr, &DownloadManager::taskFinished, this, [this](const DownloadTask&) {
            updateSidebarCounts();
        });
        connect(&mgr, &DownloadManager::taskRemoved, this, [this](const QString&) {
            checkEmptyState();
            updateSidebarCounts();
        });

        m_statusList->setCurrentRow(0);
    }

    void addSidebarItem(QListWidget* list, const QString& title, const QString& filterKey, const QString& customColor = "") {
        QListWidgetItem* item = new QListWidgetItem(list);
        item->setText(title);
        item->setData(Qt::UserRole, filterKey);
        if (!customColor.isEmpty()) {
            item->setForeground(QColor(customColor));
        }
    }

    void updateSidebarCounts() {
        const auto& tasks = DownloadManager::instance().tasks();
        int countAll = tasks.size();
        int countIncomplete = 0, countFinished = 0;

        for (const auto& t : tasks) {
            if (t.status == "Completed") countFinished++;
            else countIncomplete++;
        }

        updateItemCount(m_statusList, 0, "📁 All Downloads", countAll);
        updateItemCount(m_statusList, 1, "⏳ Incomplete", countIncomplete);
        updateItemCount(m_statusList, 2, "✓ Finished", countFinished);
    }

    void updateItemCount(QListWidget* list, int row, const QString& baseName, int count) {
        if (row < 0 || row >= list->count()) return;
        QListWidgetItem* it = list->item(row);
        it->setText(QString("%1  %2").arg(baseName).arg(count));
    }

    void checkEmptyState() {
        if (!m_model || !m_stackedWidget) return;
        if (m_model->rowCount() == 0) {
            m_stackedWidget->setCurrentIndex(0); // Empty State
        } else {
            m_stackedWidget->setCurrentIndex(1); // Table View
        }
    }

    void onTick() {
        double totalSpeed = 0.0;
        int activeThreads = 0;
        for (const auto& t : DownloadManager::instance().tasks()) {
            if (t.status == "Downloading") {
                totalSpeed += t.speed;
                activeThreads += t.segmentsCount;
            }
        }
        activeThreads = qBound(0, activeThreads, 32);

        // Only update UI widgets when window is visible (saves CPU/power while running in background system tray)
        if (isVisible()) {
            if (totalSpeed > 0) {
                m_lblTotalSpeed->setText(QString("%1/s").arg(CategoryRules::formatBytes(static_cast<quint64>(totalSpeed))));
                m_lblTotalSpeed->setStyleSheet("color: #00ffff; font-weight: bold; font-family: 'Segoe UI', monospace; font-size: 13px; border: none; background: transparent;");
            } else {
                m_lblTotalSpeed->setText("0 KB/s");
                m_lblTotalSpeed->setStyleSheet("color: #64748b; font-weight: bold; font-family: 'Segoe UI', monospace; font-size: 13px; border: none; background: transparent;");
            }

            int maxTh = qMax(activeThreads, DownloadManager::instance().defaultThreads());
            m_lblThreadsCount->setText(QString("%1 / %2").arg(activeThreads).arg(maxTh));
            m_threadsProgress->setRange(0, maxTh);
            if (activeThreads > 0) {
                m_lblThreadsCount->setStyleSheet("color: #38bdf8; font-size: 11px; font-weight: bold; font-family: 'Segoe UI', monospace; border: none; background: transparent;");
            } else {
                m_lblThreadsCount->setStyleSheet("color: #64748b; font-size: 11px; font-weight: bold; font-family: 'Segoe UI', monospace; border: none; background: transparent;");
            }
            m_threadsProgress->setValue(activeThreads);
            updateSidebarCounts();
        }

        if (m_trayIcon) {
            if (totalSpeed > 0) {
                m_trayIcon->setToolTip(QString("Prime Download Manager 3.0.46\nSpeed: %1/s | Active Threads: %2")
                    .arg(CategoryRules::formatBytes(static_cast<quint64>(totalSpeed)))
                    .arg(activeThreads));
            } else {
                m_trayIcon->setToolTip("Prime Download Manager 3.0.46");
            }
        }
    }

    void setupTrayIcon() {
        if (!QSystemTrayIcon::isSystemTrayAvailable()) return;

        m_trayIcon = new QSystemTrayIcon(QIcon(":/icons/app_logo.png"), this);
        m_trayIcon->setToolTip("Prime Download Manager 3.0.46");

        m_trayMenu = new QMenu(this);
        m_trayMenu->setStyleSheet(
            "QMenu { background-color: #101624; color: #ffffff; border: 1px solid rgba(255,255,255,0.12); padding: 5px; border-radius: 6px; font-size: 12px; }"
            "QMenu::item { padding: 6px 24px; border-radius: 4px; }"
            "QMenu::item:selected { background-color: #007acc; color: #ffffff; }"
            "QMenu::separator { height: 1px; background: rgba(255,255,255,0.1); margin: 4px 6px; }"
        );

        QAction* actOpen = m_trayMenu->addAction(QIcon(":/icons/app_logo.png"), "Open Prime Download Manager");
        connect(actOpen, &QAction::triggered, this, &MainWindow::showWindow);

        m_trayMenu->addSeparator();

        QAction* actAdd = m_trayMenu->addAction("Add Download...");
        connect(actAdd, &QAction::triggered, this, &MainWindow::onAddUrl);

        QAction* actResumeAll = m_trayMenu->addAction("Resume All");
        connect(actResumeAll, &QAction::triggered, this, &MainWindow::onResumeAll);

        QAction* actStopAll = m_trayMenu->addAction("Stop All");
        connect(actStopAll, &QAction::triggered, this, &MainWindow::onStopAll);

        m_trayMenu->addSeparator();

        QAction* actOptions = m_trayMenu->addAction("Options...");
        connect(actOptions, &QAction::triggered, this, &MainWindow::onOptions);

        QAction* actAbout = m_trayMenu->addAction("About...");
        connect(actAbout, &QAction::triggered, this, &MainWindow::onAbout);

        m_trayMenu->addSeparator();

        QAction* actExit = m_trayMenu->addAction("Exit");
        connect(actExit, &QAction::triggered, this, &MainWindow::exitApplication);

        m_trayIcon->setContextMenu(m_trayMenu);

        connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
                showWindow();
            }
        });

        m_trayIcon->show();
    }

    void setupExtensionServer() {
        m_extServer = new ExtensionServer(this);
        connect(m_extServer, &ExtensionServer::downloadRequestedFull, this, [this](const QString& url, const QString& fname, const QString& quality, const QJsonArray& qualities, int threads, const QString& wpUrl, bool startNow, const QString& audioUrl) {
            if (DownloadManager::instance().isUrlBlocked(url, wpUrl)) {
                return;
            }
            QMetaObject::invokeMethod(this, [this, url, fname, quality, qualities, threads, wpUrl, startNow, audioUrl]() {
                if (startNow && !DownloadManager::instance().showStartDlg()) {
                    bool isVid = CategoryRules::isVideoPortal(url) || CategoryRules::isVideoPortal(wpUrl) || CategoryRules::isDirectMediaStream(url);
                    if (!isVid) {
                        QString ext = QFileInfo(fname).suffix().toLower();
                        if (ext == "mp4" || ext == "mkv" || ext == "webm" || ext == "avi" || ext == "mov" || ext == "flv" || ext == "wmv" || ext == "ts") isVid = true;
                    }
                    QString cat = isVid ? "Videos" : CategoryRules::categorizeFilename(fname);
                    DownloadManager::instance().startDownload(
                        url, fname, 0, cat, "", threads, wpUrl, quality, true, audioUrl
                    );
                    checkEmptyState();
                    return;
                }

                DownloadDialog* dlg = new DownloadDialog(url, fname, quality, qualities, nullptr);
                dlg->setAudioUrl(audioUrl);
                dlg->setAttribute(Qt::WA_DeleteOnClose);

                connect(dlg, &QDialog::accepted, this, [this, dlg, wpUrl]() {
                    DownloadManager::instance().startDownload(
                        dlg->url(),
                        dlg->fileName(),
                        dlg->totalSize(),
                        dlg->category(),
                        dlg->saveDir(),
                        dlg->threads(),
                        wpUrl,
                        dlg->formatQuality(),
                        dlg->startImmediately(),
                        dlg->audioUrl()
                    );
                    checkEmptyState();
                });

                dlg->show();
                dlg->raise();
                dlg->activateWindow();
            }, Qt::QueuedConnection);
        });
        m_extServer->start(41245);
    }

public slots:
    void onAddUrl() {
        EnterUrlDialog enterDlg(this);
        if (enterDlg.exec() != QDialog::Accepted) return;

        QString targetUrl = MirrorHealer::healUrl(enterDlg.url());
        if (targetUrl.isEmpty()) return;

        DownloadDialog* dlg = new DownloadDialog(targetUrl, "", "", QJsonArray(), nullptr);
        dlg->setAttribute(Qt::WA_DeleteOnClose);

        connect(dlg, &QDialog::accepted, this, [this, dlg]() {
            DownloadManager::instance().startDownload(
                dlg->url(),
                dlg->fileName(),
                dlg->totalSize(),
                dlg->category(),
                dlg->saveDir(),
                dlg->threads(),
                "",
                dlg->formatQuality(),
                dlg->startImmediately()
            );
            checkEmptyState();
        });

        dlg->show();
        dlg->raise();
        dlg->activateWindow();
    }

    void onResumeSelected() {
        int row = selectedRow();
        if (row >= 0) {
            const auto* t = m_model->getTaskAt(row);
            if (t) {
                DownloadManager::instance().resumeDownload(t->id);
                return;
            }
        }
        // Fallback: resume first paused, stopped, or error task in queue
        for (const auto& t : DownloadManager::instance().tasks()) {
            if (t.status == "Paused" || t.status == "Stopped" || t.status.startsWith("Error")) {
                DownloadManager::instance().resumeDownload(t.id);
                break;
            }
        }
    }

    void onPauseSelected() {
        int row = selectedRow();
        if (row >= 0) {
            const auto* t = m_model->getTaskAt(row);
            if (t) {
                DownloadManager::instance().pauseDownload(t->id);
                return;
            }
        }
        // Fallback: pause first downloading task
        for (const auto& t : DownloadManager::instance().tasks()) {
            if (t.status == "Downloading" || t.status == "Connecting...") {
                DownloadManager::instance().pauseDownload(t.id);
                break;
            }
        }
    }

    void onStopSelected() {
        int row = selectedRow();
        if (row >= 0) {
            const auto* t = m_model->getTaskAt(row);
            if (t) {
                DownloadManager::instance().stopDownload(t->id);
                return;
            }
        }
        // Fallback: stop first downloading task
        for (const auto& t : DownloadManager::instance().tasks()) {
            if (t.status == "Downloading" || t.status == "Connecting...") {
                DownloadManager::instance().stopDownload(t.id);
                break;
            }
        }
    }

    void onResumeAll() {
        auto& mgr = DownloadManager::instance();
        for (const auto& t : mgr.tasks()) {
            if (t.status == "Paused" || t.status == "Stopped" || t.status.startsWith("Error")) {
                mgr.resumeDownload(t.id);
            }
        }
    }

    void onStopAll() {
        auto& mgr = DownloadManager::instance();
        for (const auto& t : mgr.tasks()) {
            if (t.status == "Downloading" || t.status == "Connecting...") {
                mgr.stopDownload(t.id);
            }
        }
        m_lblTotalSpeed->setText("0 KB/s");
        m_lblTotalSpeed->setStyleSheet("color: #64748b; font-weight: bold; font-family: 'Segoe UI', monospace; font-size: 13px; border: none; background: transparent;");
        m_lblThreadsCount->setText(QString("0 / %1").arg(DownloadManager::instance().defaultThreads()));
        m_lblThreadsCount->setStyleSheet("color: #64748b; font-size: 11px; font-weight: bold; font-family: 'Segoe UI', monospace; border: none; background: transparent;");
        m_threadsProgress->setRange(0, DownloadManager::instance().defaultThreads());
        m_threadsProgress->setValue(0);
    }

    void onDeleteSelected() {
        auto sel = m_table->selectionModel()->selectedRows();
        if (!sel.isEmpty()) {
            QList<QString> taskIds;
            for (const auto& idx : sel) {
                const DownloadTask* t = m_model->getTaskAt(idx.row());
                if (t) taskIds.append(t->id);
            }
            for (const auto& id : taskIds) {
                DownloadManager::instance().removeDownload(id, true);
            }
            checkEmptyState();
            updateSidebarCounts();
            return;
        }

        int row = selectedRow();
        const DownloadTask* task = (row >= 0) ? m_model->getTaskAt(row) : nullptr;
        if (!task && !DownloadManager::instance().tasks().isEmpty()) {
            task = &DownloadManager::instance().tasks().first();
        }
        if (!task) return;

        DownloadManager::instance().removeDownload(task->id, true);
        checkEmptyState();
        updateSidebarCounts();
    }

    void onRemoveFromListSelected() {
        auto sel = m_table->selectionModel()->selectedRows();
        if (!sel.isEmpty()) {
            QList<QString> taskIds;
            for (const auto& idx : sel) {
                const DownloadTask* t = m_model->getTaskAt(idx.row());
                if (t) taskIds.append(t->id);
            }
            for (const auto& id : taskIds) {
                DownloadManager::instance().removeDownload(id, false);
            }
            checkEmptyState();
            updateSidebarCounts();
            return;
        }

        int row = selectedRow();
        const DownloadTask* task = (row >= 0) ? m_model->getTaskAt(row) : nullptr;
        if (!task && !DownloadManager::instance().tasks().isEmpty()) {
            task = &DownloadManager::instance().tasks().first();
        }
        if (!task) return;

        DownloadManager::instance().removeDownload(task->id, false);
        checkEmptyState();
        updateSidebarCounts();
    }

    void onClearDone() {
        auto& mgr = DownloadManager::instance();
        auto tasksCopy = mgr.tasks();
        for (const auto& t : tasksCopy) {
            if (t.status == "Completed") {
                mgr.removeDownload(t.id, false);
            }
        }
        checkEmptyState();
        updateSidebarCounts();
    }

    void onOptions() {
        OptionsDialog dlg(this);
        dlg.exec();
    }

    void onAbout() {
        AboutDialog dlg(this);
        dlg.exec();
    }

    static void openFolderAndSelectFile(const QString& rawPath) {
        QString path = rawPath;
        QFileInfo fi(path);
        if (!fi.exists()) {
            QString dir = fi.absolutePath();
            QString base = fi.completeBaseName();
            QDir qDir(dir);
            QStringList entries = qDir.entryList(QStringList() << (base + ".*"), QDir::Files);
            if (!entries.isEmpty()) {
                path = dir + "/" + entries.first();
                fi.setFile(path);
            } else {
                for (const QString& ext : {"exe", "pdf", "zip", "mp4", "mkv", "webm", "msi", "rar", "7z", "docx", "iso"}) {
                    QString cand = dir + "/" + base + "." + ext;
                    if (QFile::exists(cand)) {
                        path = cand;
                        fi.setFile(path);
                        break;
                    }
                }
            }
        }

        QString nativePath = QDir::toNativeSeparators(fi.absoluteFilePath());
        bool opened = false;

#ifdef Q_OS_WIN
        if (fi.exists()) {
            CoInitialize(NULL);
            PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(reinterpret_cast<LPCWSTR>(nativePath.utf16()));
            if (pidl) {
                HRESULT hr = SHOpenFolderAndSelectItems(pidl, 0, NULL, 0);
                ILFree(pidl);
                if (SUCCEEDED(hr)) {
                    opened = true;
                }
            }
        }
#endif

        if (!opened) {
            if (fi.exists()) {
                QStringList args;
                args << QString("/select,%1").arg(nativePath);
                opened = QProcess::startDetached("explorer.exe", args);
            } else if (QDir(fi.absolutePath()).exists()) {
                QString nativeDir = QDir::toNativeSeparators(fi.absolutePath());
                QProcess::startDetached("explorer.exe", QStringList() << nativeDir);
            }
        }
    }

    void onTableContextMenu(const QPoint& pos) {
        int row = selectedRow();
        const auto* t = (row >= 0) ? m_model->getTaskAt(row) : nullptr;

        QMenu menu(this);
        menu.setStyleSheet("QMenu { background-color: #101625; color: #ffffff; border: 1px solid rgba(255,255,255,0.1); padding: 4px; }"
                           "QMenu::item { padding: 6px 20px; border-radius: 4px; font-size: 11px; }"
                           "QMenu::item:selected { background-color: #0284c7; }");

        if (t) {
            menu.addAction("Redownload", this, [t]() {
                DownloadManager::instance().redownload(t->id);
            });
            menu.addAction("Open Folder", this, [t]() {
                openFolderAndSelectFile(t->savePath);
            });
            menu.addAction("Remove from list", this, &MainWindow::onRemoveFromListSelected);
            menu.addSeparator();
        }

        // Columns Submenu
        QMenu* colMenu = menu.addMenu("Columns");
        colMenu->setStyleSheet(menu.styleSheet());
        const QStringList colNames = {"File Name", "Size", "Status", "Speed", "Time Left", "Progress"};
        QHeaderView* header = m_table->horizontalHeader();
        for (int i = 0; i < colNames.size(); ++i) {
            QAction* act = colMenu->addAction(colNames[i]);
            act->setCheckable(true);
            act->setChecked(!header->isSectionHidden(i) && m_table->columnWidth(i) > 25);
            connect(act, &QAction::toggled, this, [this, header, i](bool visible) {
                if (visible) {
                    header->setSectionHidden(i, false);
                    if (m_table->columnWidth(i) <= 25) {
                        int defaultWidths[] = {320, 95, 105, 100, 95, 140};
                        m_table->setColumnWidth(i, defaultWidths[i]);
                    }
                } else {
                    header->setSectionHidden(i, true);
                }
                QSettings s("PrimeDownloadManager", "PDM");
                s.setValue("tableHeaderState", header->saveState());
            });
        }
        colMenu->addSeparator();
        colMenu->addAction("Reset Columns to Default", [this, header]() {
            for (int i = 0; i < 6; ++i) {
                header->setSectionHidden(i, false);
                header->moveSection(header->visualIndex(i), i);
                header->setSectionResizeMode(i, QHeaderView::Interactive);
            }
            int defaultWidths[] = {320, 95, 105, 100, 95, 140};
            for (int i = 0; i < 6; ++i) {
                m_table->setColumnWidth(i, defaultWidths[i]);
            }
            QSettings s("PrimeDownloadManager", "PDM");
            s.remove("tableHeaderState");
        });

        menu.exec(m_table->viewport()->mapToGlobal(pos));
    }

    void onTableDoubleClicked(const QModelIndex& index) {
        const auto* t = m_model->getTaskAt(index.row());
        if (!t) return;
        if (t->status == "Completed") {
            if (QFile::exists(t->savePath)) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(t->savePath));
            } else {
                QFileInfo fi(t->savePath);
                QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
            }
        } else if (t->status == "Paused" || t->status == "Stopped" || t->status.startsWith("Error")) {
            DownloadManager::instance().resumeDownload(t->id);
        } else if (t->status == "Downloading" || t->status == "Connecting...") {
            DownloadManager::instance().pauseDownload(t->id);
        }
    }

private:
    int selectedRow() const {
        auto sel = m_table->selectionModel()->selectedRows();
        if (sel.isEmpty()) return -1;
        return sel.first().row();
    }

    QLabel* m_lblTotalSpeed = nullptr;
    QListWidget* m_statusList = nullptr;
    QLabel* m_lblThreadsCount = nullptr;
    QProgressBar* m_threadsProgress = nullptr;

    QStackedWidget* m_stackedWidget = nullptr;
    QWidget* m_emptyStateWidget = nullptr;
    QTableView* m_table = nullptr;
    TaskTableModel* m_model = nullptr;
    ExtensionServer* m_extServer = nullptr;

    QSystemTrayIcon* m_trayIcon = nullptr;
    QMenu* m_trayMenu = nullptr;
    bool m_reallyQuit = false;
};
