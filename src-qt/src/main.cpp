#include <QApplication>
#include <QStyleFactory>
#include <QLocalServer>
#include <QLocalSocket>
#include <QByteArray>
#include <QString>
#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif
#include "ui/MainWindow.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>

static QMutex s_logMutex;

static void logHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    Q_UNUSED(context);
#ifdef NDEBUG
    if (type == QtDebugMsg || type == QtInfoMsg) return;
#endif
    QMutexLocker locker(&s_logMutex);
    QString tempDir = QDir::tempPath() + "/PrimeDownloadManager";
    QDir().mkpath(tempDir);
    QFile f(tempDir + "/app.log");
    if (f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz ") << msg << "\n";
        ts.flush();
        f.flush();
    }
}

int main(int argc, char *argv[]) {
    // High-DPI scaling configuration for crisp rendering on modern displays
    qputenv("QT_ENABLE_HIGHDPI_SCALING", "1");

    qInstallMessageHandler(logHandler);
    qWarning() << "[MAIN] Prime Download Manager starting, args:" << argc;
#ifdef Q_OS_WIN
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // 1. Primary Mutex check (Kernel-level single instance check: dead processes cannot hold a mutex)
    SetLastError(0);
    HANDLE hMutex = CreateMutexW(NULL, FALSE, L"Local\\PrimeDownloadManager_SingleInstance_Mutex_v3");
    DWORD mErr = GetLastError();
    qWarning() << "[MAIN] CreateMutexW result:" << hMutex << "mErr:" << mErr;
    if (hMutex != NULL && mErr == ERROR_ALREADY_EXISTS) {
        HWND hwnd = FindWindowW(NULL, L"Prime Download Manager 3.0.46");
        if (hwnd) {
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
            BringWindowToTop(hwnd);
            CloseHandle(hMutex);
            return 0;
        }
    }
#endif

    QApplication app(argc, argv);
    qWarning() << "[MAIN] QApplication created";

#ifdef Q_OS_WIN
    if (hMutex != NULL && mErr == ERROR_ALREADY_EXISTS) {
        QLocalSocket socket;
        socket.connectToServer("PrimeDownloadManager_IPC_Server");
        if (socket.waitForConnected(400)) {
            socket.write("SHOW\n");
            socket.flush();
            socket.waitForBytesWritten(400);
            CloseHandle(hMutex);
            return 0;
        }
    }
#endif
    qWarning() << "[MAIN] Initializing MainWindow...";
    app.setApplicationName("PrimeDownloadManager");
    app.setOrganizationName("@Mehedi2R");
    app.setApplicationVersion("3.0.46");
    app.setWindowIcon(QIcon(":/icons/app_logo.png"));
    // Ensure the application stays active in the background tray when main window is closed
    app.setQuitOnLastWindowClosed(false);

    app.setStyle(QStyleFactory::create("Fusion"));
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor("#0b0f19"));
    darkPalette.setColor(QPalette::WindowText, QColor("#cbd5e1"));
    darkPalette.setColor(QPalette::Base, QColor("#0d121d"));
    darkPalette.setColor(QPalette::AlternateBase, QColor("#101625"));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, QColor("#e2e8f0"));
    darkPalette.setColor(QPalette::Button, QColor("#161f30"));
    darkPalette.setColor(QPalette::ButtonText, QColor("#cbd5e1"));
    darkPalette.setColor(QPalette::Highlight, QColor("#007acc"));
    darkPalette.setColor(QPalette::HighlightedText, Qt::white);
    app.setPalette(darkPalette);

    app.setStyleSheet(R"(
        QMainWindow { background-color: #0b0f19; }
        QScrollBar:vertical { background: #0b0f19; width: 6px; margin: 0px; }
        QScrollBar::handle:vertical { background: #1e293b; min-height: 20px; border-radius: 3px; }
        QScrollBar::handle:vertical:hover { background: #334155; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
        QDialog { background-color: #101625; color: #ffffff; }
        QLabel { color: #cbd5e1; }
        QLineEdit { background-color: #0b0f19; color: #ffffff; border: 1px solid #1e293b; border-radius: 4px; padding: 6px; }
        QLineEdit:focus { border: 1px solid #00d2ff; }
        QComboBox { background-color: #0b0f19; color: #ffffff; border: 1px solid #1e293b; border-radius: 4px; padding: 6px; }
        QSpinBox { background-color: #0b0f19; color: #ffffff; border: 1px solid #1e293b; border-radius: 4px; padding: 6px; }
        QPushButton { background-color: #007acc; color: #ffffff; border: none; border-radius: 4px; padding: 7px 16px; font-weight: bold; }
        QPushButton:hover { background-color: #0094f0; }
        QPushButton:pressed { background-color: #005f9e; }
    )");

    MainWindow w;
    qWarning() << "[MAIN] MainWindow constructed successfully";

#ifdef Q_OS_WIN
    // Enable Windows Immersive Dark Mode on native window frame & set dark background brush
    // to completely eliminate white frame flash/lag during window initialization
    HWND mainHwnd = reinterpret_cast<HWND>(w.winId());
    if (mainHwnd) {
        BOOL dark = TRUE;
        ::DwmSetWindowAttribute(mainHwnd, 20, &dark, sizeof(dark)); // DWMWA_USE_IMMERSIVE_DARK_MODE (Win10 20H1+ & Win11)
        ::DwmSetWindowAttribute(mainHwnd, 19, &dark, sizeof(dark)); // Win10 1809
        HBRUSH darkBrush = ::CreateSolidBrush(RGB(11, 15, 25)); // #0b0f19
        ::SetClassLongPtr(mainHwnd, GCLP_HBRBACKGROUND, reinterpret_cast<LONG_PTR>(darkBrush));
    }
#endif

    // Check if launched silently in background (e.g. system startup)
    bool startInBackground = false;
    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]).toLower();
        if (arg == "--background" || arg == "--tray" || arg == "-b" || arg == "/background" || arg == "--silent") {
            startInBackground = true;
            break;
        }
    }

    if (!startInBackground) {
        w.show();
    }

    // Set up single instance IPC server to restore window when another instance is launched
    QLocalServer localServer;
    QLocalServer::removeServer("PrimeDownloadManager_IPC_Server");
    if (localServer.listen("PrimeDownloadManager_IPC_Server")) {
        QObject::connect(&localServer, &QLocalServer::newConnection, [&localServer, &w]() {
            while (QLocalSocket* client = localServer.nextPendingConnection()) {
                QObject::connect(client, &QLocalSocket::readyRead, [client, &w]() {
                    QByteArray cmd = client->readAll();
                    if (cmd.contains("SHOW")) {
                        w.showWindow();
                    }
                });
                if (client->bytesAvailable() > 0) {
                    QByteArray cmd = client->readAll();
                    if (cmd.contains("SHOW")) {
                        w.showWindow();
                    }
                }
            }
        });
    }

    qWarning() << "[MAIN] Entering app.exec()...";
    int ret = app.exec();
    qWarning() << "[MAIN] app.exec() returned:" << ret;

#ifdef Q_OS_WIN
    if (hMutex) {
        CloseHandle(hMutex);
    }
    WSACleanup();
#endif
    return ret;
}
