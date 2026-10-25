#pragma once
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>
#include <QPixmap>
#include <QScreen>
#include <QGuiApplication>
#include <QPainter>
#include <QStyleOption>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include "../Version.h"

class AboutDialog : public QDialog {
    Q_OBJECT
public:
    explicit AboutDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_StyledBackground, true);
        setFixedSize(360, 200);
        setStyleSheet(
            "QDialog { background-color: #0b0f19; color: #f8fafc; font-family: 'Segoe UI', sans-serif; border: 1px solid #1e293b; border-radius: 10px; }"
            "QLabel { color: #cbd5e1; border: none; background: transparent; }"
            "QPushButton { background-color: #00d2ff; color: #000000; border-radius: 6px; padding: 6px 20px; font-size: 12px; font-weight: bold; min-width: 80px; }"
            "QPushButton:hover { background-color: #38bdf8; }"
        );

        setupUI();
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
        SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
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
        mainLayout->setContentsMargins(18, 14, 18, 16);
        mainLayout->setSpacing(8);

        // Header with title and close button
        QHBoxLayout* headerLayout = new QHBoxLayout();
        headerLayout->setContentsMargins(0, 0, 0, 0);
        headerLayout->setSpacing(8);

        QLabel* titleLabel = new QLabel("About Prime Download Manager", this);
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

        mainLayout->addSpacing(4);

        // App Logo
        QLabel* lblIcon = new QLabel(this);
        QPixmap logoPix(":/icons/app_logo.png");
        if (logoPix.isNull()) {
            logoPix.load("resources/icons/app_logo.png");
        }
        if (!logoPix.isNull()) {
            lblIcon->setPixmap(logoPix.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        lblIcon->setAlignment(Qt::AlignCenter);
        mainLayout->addWidget(lblIcon);

        // Title
        QLabel* lblTitle = new QLabel("Prime Download Manager", this);
        lblTitle->setStyleSheet("font-size: 16px; font-weight: bold; color: #ffffff;");
        lblTitle->setAlignment(Qt::AlignCenter);
        mainLayout->addWidget(lblTitle);

        // Version
        QLabel* lblVersion = new QLabel(QString("Version: %1").arg(APP_VERSION), this);
        lblVersion->setStyleSheet("font-size: 12px; color: #94a3b8; font-weight: 500;");
        lblVersion->setAlignment(Qt::AlignCenter);
        mainLayout->addWidget(lblVersion);

        // Developer with clickable Telegram link
        QLabel* lblDev = new QLabel(this);
        lblDev->setText("Developer: <a href=\"https://t.me/Mehedi2R\" style=\"color: #00d2ff; text-decoration: none; font-weight: 600;\">@Mehedi2R</a>");
        lblDev->setTextInteractionFlags(Qt::TextBrowserInteraction);
        lblDev->setOpenExternalLinks(true);
        lblDev->setStyleSheet("font-size: 12px; color: #cbd5e1;");
        lblDev->setAlignment(Qt::AlignCenter);
        mainLayout->addWidget(lblDev);

        mainLayout->addSpacing(8);
    }

    QPoint m_dragPos;
};
