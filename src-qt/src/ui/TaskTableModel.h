#pragma once
#include <QAbstractTableModel>
#include "../core/DownloadManager.h"
#include "../core/CategoryRules.h"

class TaskTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit TaskTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {
        auto& mgr = DownloadManager::instance();
        connect(&mgr, &DownloadManager::taskAdded, this, [this](const DownloadTask&) {
            updateFilter();
        });
        connect(&mgr, &DownloadManager::taskUpdated, this, [this](const DownloadTask& task) {
            int modelRow = findModelRow(task.id);
            if (modelRow >= 0) {
                emit dataChanged(index(modelRow, 0), index(modelRow, columnCount() - 1));
            }
        });
        connect(&mgr, &DownloadManager::taskRemoved, this, [this](const QString&) {
            updateFilter();
        });
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        if (parent.isValid()) return 0;
        return m_filteredIndices.isEmpty() && m_currentFilter == "all" 
            ? DownloadManager::instance().tasks().size() 
            : m_filteredIndices.size();
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override {
        if (parent.isValid()) return 0;
        return 6; // FILE NAME, SIZE, STATUS, SPEED, TIME LEFT, PROGRESS
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        if (orientation == Qt::Horizontal) {
            if (role == Qt::DisplayRole) {
                switch (section) {
                    case 0: return "FILE NAME";
                    case 1: return "SIZE";
                    case 2: return "STATUS";
                    case 3: return "SPEED";
                    case 4: return "TIME LEFT";
                    case 5: return "PROGRESS";
                }
            } else if (role == Qt::TextAlignmentRole) {
                return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
            }
        }
        return QVariant();
    }

    const DownloadTask* getTaskAt(int row) const {
        const auto& tasks = DownloadManager::instance().tasks();
        int realIndex = getRealIndex(row);
        if (realIndex >= 0 && realIndex < tasks.size()) {
            return &tasks[realIndex];
        }
        return nullptr;
    }

    int findModelRow(const QString& taskId) const {
        int count = rowCount();
        for (int r = 0; r < count; ++r) {
            const auto* t = getTaskAt(r);
            if (t && t->id == taskId) return r;
        }
        return -1;
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (!index.isValid()) return QVariant();

        const auto* t = getTaskAt(index.row());
        if (!t) return QVariant();

        if (role == Qt::TextAlignmentRole) {
            return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
        }

        if (role == Qt::DisplayRole) {
            switch (index.column()) {
                case 0: return t->fileName;
                case 1: {
                    QString downStr = CategoryRules::formatBytes(t->downloadedSize);
                    if (t->totalSize > 0) {
                        return QString("%1 / %2").arg(downStr, CategoryRules::formatBytes(t->totalSize));
                    }
                    return QString("%1 / Unknown").arg(downStr);
                }
                case 2: return t->status;
                case 3: {
                    if (t->status == "Downloading" && t->speed > 0) {
                        return QString("%1/s").arg(CategoryRules::formatBytes(static_cast<quint64>(t->speed)));
                    }
                    return "-";
                }
                case 4: {
                    if (t->status == "Downloading" && t->etaSeconds > 0) {
                        qint64 mins = t->etaSeconds / 60;
                        qint64 secs = t->etaSeconds % 60;
                        return QString("%1m %2s").arg(mins).arg(secs);
                    }
                    return "-";
                }
                case 5: return QString::asprintf("%.1f%%", t->progressPercent());
            }
        } else if (role == Qt::UserRole && index.column() == 5) {
            return t->progressPercent();
        } else if (role == Qt::ForegroundRole && index.column() == 2) {
            if (t->status == "Downloading") return QColor("#00d2ff");
            if (t->status == "Completed") return QColor("#10b981");
            if (t->status == "Paused") return QColor("#f59e0b");
            if (t->status.startsWith("Error")) return QColor("#f43f5e");
            return QColor("#94a3b8");
        } else if (role == Qt::ToolTipRole) {
            return QString("File: %1\nURL: %2\nPath: %3").arg(t->fileName).arg(t->url).arg(t->savePath);
        }

        return QVariant();
    }

    void setFilter(const QString& filter) {
        m_currentFilter = filter;
        updateFilter();
    }

    void updateFilter() {
        beginResetModel();
        m_filteredIndices.clear();
        const auto& tasks = DownloadManager::instance().tasks();
        
        for (int i = 0; i < tasks.size(); ++i) {
            const auto& t = tasks[i];
            if (m_currentFilter == "all") {
                m_filteredIndices.append(i);
            } else if (m_currentFilter == "Incomplete" || m_currentFilter == "incomplete") {
                if (t.status != "Completed") m_filteredIndices.append(i);
            } else if (m_currentFilter == "Finished" || m_currentFilter == "finished" || m_currentFilter == "Completed") {
                if (t.status == "Completed") m_filteredIndices.append(i);
            } else if (t.category.compare(m_currentFilter, Qt::CaseInsensitive) == 0) {
                m_filteredIndices.append(i);
            }
        }
        endResetModel();
    }

private:
    int getRealIndex(int row) const {
        if (m_currentFilter == "all" && m_filteredIndices.isEmpty()) return row;
        if (row >= 0 && row < m_filteredIndices.size()) return m_filteredIndices[row];
        return -1;
    }

    QString m_currentFilter = "all";
    QVector<int> m_filteredIndices;
};
