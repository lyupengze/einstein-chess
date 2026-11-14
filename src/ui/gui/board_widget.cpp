#include "board_widget.h"

#include <QFont>
#include <QPainter>
#include <QRadialGradient>
#include <algorithm>

namespace ui {

BoardWidget::BoardWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(400, 400);
}

void BoardWidget::update_state(const core::GameState& state) {
    current_state_ = state;
    update();
}

void BoardWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int board_size = std::min(width(), height());
    int offset_x = (width() - board_size) / 2;
    int offset_y = (height() - board_size) / 2;
    int cell_size = board_size / 5;

    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 5; ++col) {
            QColor cell_color = ((row + col) % 2 == 0) ? QColor("#ecf0f1") : QColor("#bdc3c7");
            painter.fillRect(offset_x + col * cell_size, offset_y + row * cell_size, cell_size,
                             cell_size, cell_color);
        }
    }

    painter.setPen(Qt::NoPen);

    painter.setBrush(QColor(231, 76, 60, 50));
    QPolygon red_region;
    red_region << QPoint(offset_x, offset_y) << QPoint(offset_x + 3 * cell_size, offset_y)
               << QPoint(offset_x + 2 * cell_size, offset_y + cell_size)
               << QPoint(offset_x + cell_size, offset_y + 2 * cell_size)
               << QPoint(offset_x, offset_y + 3 * cell_size);
    painter.drawPolygon(red_region);

    painter.setBrush(QColor(52, 152, 219, 50));
    QPolygon blue_region;
    blue_region << QPoint(offset_x + board_size, offset_y + board_size)
                << QPoint(offset_x + board_size - 3 * cell_size, offset_y + board_size)
                << QPoint(offset_x + board_size - 2 * cell_size, offset_y + board_size - cell_size)
                << QPoint(offset_x + board_size - cell_size, offset_y + board_size - 2 * cell_size)
                << QPoint(offset_x + board_size, offset_y + board_size - 3 * cell_size);
    painter.drawPolygon(blue_region);

    painter.setPen(QPen(QColor("#95a5a6"), 1));
    for (int i = 0; i <= 5; ++i) {
        painter.drawLine(offset_x, offset_y + i * cell_size, offset_x + board_size,
                         offset_y + i * cell_size);
        painter.drawLine(offset_x + i * cell_size, offset_y, offset_x + i * cell_size,
                         offset_y + board_size);
    }

    for (std::size_t x = 0; x < core::Board::kSize; ++x) {
        for (std::size_t y = 0; y < core::Board::kSize; ++y) {
            auto piece = current_state_.board.at({x, y});
            if (piece.side == core::Side::None) {
                continue;
            }

            int cx = offset_x + static_cast<int>(y) * cell_size;
            int cy = offset_y + static_cast<int>(x) * cell_size;
            int margin = cell_size / 8;
            QRect piece_rect(cx + margin, cy + margin, cell_size - margin * 2,
                             cell_size - margin * 2);

            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0, 0, 0, 60));
            painter.drawEllipse(piece_rect.translated(3, 3));

            QRadialGradient gradient(
                piece_rect.center() - QPoint(piece_rect.width() / 4, piece_rect.height() / 4),
                piece_rect.width());
            if (piece.side == core::Side::Red) {
                gradient.setColorAt(0.0, QColor("#ff7675"));
                gradient.setColorAt(1.0, QColor("#c0392b"));
            } else {
                gradient.setColorAt(0.0, QColor("#74b9ff"));
                gradient.setColorAt(1.0, QColor("#2980b9"));
            }
            painter.setBrush(gradient);
            painter.drawEllipse(piece_rect);

            painter.setBrush(QColor(255, 255, 255, 100));
            painter.drawEllipse(
                piece_rect.center() - QPoint(piece_rect.width() / 5, piece_rect.height() / 5),
                piece_rect.width() / 6, piece_rect.height() / 6);

            painter.setPen(Qt::white);
            QFont font = painter.font();
            font.setPixelSize(cell_size / 3);
            font.setBold(true);
            painter.setFont(font);
            painter.drawText(piece_rect, Qt::AlignCenter, QString::number(piece.id));
        }
    }
}

}  // namespace ui