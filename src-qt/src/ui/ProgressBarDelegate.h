#pragma once
#include <QStyledItemDelegate>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>

class ProgressBarDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit ProgressBarDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override {
        QStyledItemDelegate::initStyleOption(option, index);
        option->state &= ~QStyle::State_HasFocus;
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        if (index.column() == 5) { // Progress column
            double percent = index.data(Qt::UserRole).toDouble();
            
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);

            QRect rect = option.rect.adjusted(6, 6, -6, -6);
            
            // Background track
            painter->setBrush(QColor("#1e293b"));
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(rect, 4, 4);

            // Progress fill
            int fillWidth = static_cast<int>(rect.width() * (percent / 100.0));
            if (fillWidth > 0) {
                QRect fillRect = rect;
                fillRect.setWidth(fillWidth);

                QLinearGradient grad(fillRect.topLeft(), fillRect.topRight());
                grad.setColorAt(0.0, QColor("#00d2ff"));
                grad.setColorAt(1.0, QColor("#0088cc"));

                painter->setBrush(grad);
                painter->drawRoundedRect(fillRect, 4, 4);
            }

            // Percentage Text
            painter->setPen(QColor("#ffffff"));
            QFont f = option.font;
            f.setPointSize(9);
            f.setBold(true);
            painter->setFont(f);

            QString text = QString::asprintf("%.1f%%", percent);
            painter->drawText(rect, Qt::AlignCenter, text);

            painter->restore();
            return;
        }
        QStyleOptionViewItem opt = option;
        opt.state &= ~QStyle::State_HasFocus;
        QStyledItemDelegate::paint(painter, opt, index);
    }
};
