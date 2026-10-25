#pragma once
#include <QString>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <io.h>
#include <winioctl.h>
#endif

class FastStorageAllocator {
public:
    static bool allocateFileInstantly(const QString& filePath, quint64 totalSize) {
        QFileInfo fi(filePath);
        QDir().mkpath(fi.absolutePath());

#ifdef Q_OS_WIN
        std::wstring wpath = filePath.toStdWString();
        HANDLE hFile = CreateFileW(
            wpath.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (hFile == INVALID_HANDLE_VALUE) {
            qWarning() << "[FastStorageAllocator] Failed to create handle for" << filePath;
            return false;
        }

        // 1. Enable NTFS Sparse File flag (O(1) operation: < 1ms, zero disk blocking)
        DWORD bytesReturned = 0;
        DeviceIoControl(
            hFile,
            FSCTL_SET_SPARSE,
            NULL, 0,
            NULL, 0,
            &bytesReturned,
            NULL
        );

        // 2. Set the end of file pointer instantly without zero-filling
        if (totalSize > 0) {
            LARGE_INTEGER li;
            li.QuadPart = static_cast<LONGLONG>(totalSize);
            if (SetFilePointerEx(hFile, li, NULL, FILE_BEGIN)) {
                SetEndOfFile(hFile);
            }
        }

        CloseHandle(hFile);
        return true;
#else
        // POSIX / Non-Windows fallback
        QFile file(filePath);
        if (file.open(QIODevice::ReadWrite)) {
            file.resize(totalSize);
            file.close();
            return true;
        }
        return false;
#endif
    }
};
