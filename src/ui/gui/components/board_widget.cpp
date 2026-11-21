#include "board_widget.h"

#include <QFont>
#include <QPainter>
#include <QRadialGradient>
#include <algorithm>
#include <cstddef>

namespace ui {

namespace {

// 棋盘几何参数。绘制与命中判定共用这一份计算：
// 过去两处各自用 board_size = min(w, h) 和 cell_size = board_size / 5 重复整除，
// 判定用 board_size 而网格只覆盖 cell_size * 5，最右/最下一小条因此会把点击
// 换算成下标 5（越界）。现在统一以 cell_size * 5 得到的 span 为准。
struct BoardGeometry {
    int cell_size = 0;  // 单格边长
    int span = 0;       // 棋盘实际覆盖的像素边长，恒等于 cell_size * 5
    int offset_x = 0;   // 棋盘左上角横坐标（居中留白）
    int offset_y = 0;   // 棋盘左上角纵坐标
};

BoardGeometry compute_geometry(int widget_width, int widget_height) {
    BoardGeometry geometry;
    const int board_size = std::min(widget_width, widget_height);
    geometry.cell_size = board_size / 5;
    geometry.span = geometry.cell_size * 5;
    // 按 span 而不是 board_size 居中：上限最多 4 像素的整除余量平均分到两侧，
    // 保证网格、区域填充与点击判定共用同一条边。
    geometry.offset_x = (widget_width - geometry.span) / 2;
    geometry.offset_y = (widget_height - geometry.span) / 2;
    return geometry;
}

}  // namespace

// 棋盘控件实现。本文件只做两件事：把快照画出来、把点击换算回坐标。
// 绘制严格分层，后画的覆盖先画的：棋盘格 → 红/蓝初始区 → 网格线 →
// 棋子（投影 + 径向渐变 + 高光 + 编号）→ 三种高亮圈；高亮放在最后，
// 保证它永远压在棋子之上，不会被棋子挡住。
// 坐标换算在两处出现：paintEvent 由 (行 x, 列 y) 求出屏幕 (纵, 横)，
// mousePressEvent 再由屏幕 (横, 纵) 反推 (列, 行)。几何参数统一由
// compute_geometry 计算一次，两处共用，避免整除余量导致判定与绘制不一致。

BoardWidget::BoardWidget(QWidget* parent) : QWidget(parent) {
    // 最小 400x400：再小格子会挤到看不清编号，布局也会优先保住这个下限。
    setMinimumSize(400, 400);
}

void BoardWidget::update_state(const core::GameState& state, std::optional<core::Pos> selected_pos,
                               const std::vector<core::Pos>& selectable_pieces,
                               const std::vector<core::Pos>& reachable_targets) {
    // 整份覆盖式更新：不与旧值做差异比较，四个输入无条件替换，调用方不必关心历史状态。
    current_state_ = state;
    selected_pos_ = selected_pos;
    selectable_pieces_ = selectable_pieces;
    reachable_targets_ = reachable_targets;
    // update() 只是登记一次重绘请求，同一事件循环内的多次调用会被合并成一次 paintEvent。
    update();
}

void BoardWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 取短边作为正方形棋盘边长，剩余空间平分成两侧留白，控件被拉成长方形时棋盘仍然居中。
    // 几何参数统一由 compute_geometry 给出，绘制与 mousePressEvent 不再各算一遍。
    const BoardGeometry geometry = compute_geometry(width(), height());
    const int cell_size = geometry.cell_size;
    const int board_size = geometry.span;  // 网格与区域填充严格覆盖 5 * cell_size
    const int offset_x = geometry.offset_x;
    const int offset_y = geometry.offset_y;

    // 第一层：25 个格子按 (row + col) 的奇偶交替填色，得到经典棋盘纹；
    // 这里的 row/col 就是 Pos.x / Pos.y，屏幕位置按 (列→横、行→纵) 摆放。
    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 5; ++col) {
            QColor cell_color = ((row + col) % 2 == 0) ? QColor("#ecf0f1") : QColor("#bdc3c7");
            painter.fillRect(offset_x + col * cell_size, offset_y + row * cell_size, cell_size,
                             cell_size, cell_color);
        }
    }

    // 区域填充不描边，只靠 alpha=50 的半透明色叠在格子之上。
    painter.setPen(Qt::NoPen);

    // 第二层之一：红方初始区。从左上角出发，沿"列+行=3"的阶梯折回原点，围出一个五边形；
    // 顶点按 (横向 = 列, 纵向 = 行) 给出，与 QPoint 的 (x, y) 含义一致。
    // 顶点取到坐标 3，对应 0~2 三行三列围出的 6 格初始区；阴影仅作区域提示。
    painter.setBrush(QColor(231, 76, 60, 50));
    QPolygon red_region;
    red_region << QPoint(offset_x, offset_y) << QPoint(offset_x + 3 * cell_size, offset_y)
               << QPoint(offset_x + 2 * cell_size, offset_y + cell_size)
               << QPoint(offset_x + cell_size, offset_y + 2 * cell_size)
               << QPoint(offset_x, offset_y + 3 * cell_size);
    painter.drawPolygon(red_region);

    // 第二层之二：蓝方初始区，是红区以棋盘中心为对称点的镜像，顶点从右下角反向展开。
    painter.setBrush(QColor(52, 152, 219, 50));
    QPolygon blue_region;
    blue_region << QPoint(offset_x + board_size, offset_y + board_size)
                << QPoint(offset_x + board_size - 3 * cell_size, offset_y + board_size)
                << QPoint(offset_x + board_size - 2 * cell_size, offset_y + board_size - cell_size)
                << QPoint(offset_x + board_size - cell_size, offset_y + board_size - 2 * cell_size)
                << QPoint(offset_x + board_size, offset_y + board_size - 3 * cell_size);
    painter.drawPolygon(blue_region);

    // 第三层：6 横 + 6 纵的网格线（i 取 0~5，四条边也算在内），灰线 1 像素。
    painter.setPen(QPen(QColor("#95a5a6"), 1));
    for (int i = 0; i <= 5; ++i) {
        painter.drawLine(offset_x, offset_y + i * cell_size, offset_x + board_size,
                         offset_y + i * cell_size);
        painter.drawLine(offset_x + i * cell_size, offset_y, offset_x + i * cell_size,
                         offset_y + board_size);
    }

    // 第四层：棋子。按行、列顺序遍历，棋子彼此不重叠，所以绘制顺序不影响遮挡关系。
    for (std::size_t x = 0; x < core::Board::kSize; ++x) {
        for (std::size_t y = 0; y < core::Board::kSize; ++y) {
            auto piece = current_state_.board.at({x, y});
            if (piece.side == core::Side::None) {
                continue;
            }

            // 最关键的换算：行号 x 决定屏幕纵坐标，列号 y 决定屏幕横坐标，即按矩阵习惯摆放。
            int cx = offset_x + static_cast<int>(y) * cell_size;
            int cy = offset_y + static_cast<int>(x) * cell_size;
            // 四周各留 1/8 格宽的边距，相邻棋子之间才有间隙。
            int margin = cell_size / 8;
            QRect piece_rect(cx + margin, cy + margin, cell_size - margin * 2,
                             cell_size - margin * 2);

            // 投影：向右下偏移 3 像素的半透明黑圆，先画，大部分随后被实体圆盖住。
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0, 0, 0, 60));
            painter.drawEllipse(piece_rect.translated(3, 3));

            // 径向渐变：圆心刻意偏左上 1/4 格，让亮部落在左上、暗部沉到右下，形成球面受光感。
            // 红蓝各用一组同色系深浅值，阵营差异一眼可辨。
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

            // 高光：偏左上 1/5 处、半径 1/6 格的白色半透明小圆，与渐变亮部叠加成反光点。
            painter.setBrush(QColor(255, 255, 255, 100));
            painter.drawEllipse(
                piece_rect.center() - QPoint(piece_rect.width() / 5, piece_rect.height() / 5),
                piece_rect.width() / 6, piece_rect.height() / 6);

            // 编号：白色加粗，字号取 1/3 格宽，在棋子矩形内居中绘制；空格上面已 continue 跳过。
            painter.setPen(Qt::white);
            QFont font = painter.font();
            font.setPixelSize(cell_size / 3);
            font.setBold(true);
            painter.setFont(font);
            painter.drawText(piece_rect, Qt::AlignCenter, QString::number(piece.id));
        }
    }

    // 第五层：高亮圈。矩形算法与上面棋子完全一致，抽成 lambda 以免重复三遍。
    // 注意: 这里与棋子循环里的 margin 计算是两份独立代码，改边距必须同时改两处。
    auto get_piece_rect = [&](std::size_t rx, std::size_t ry) {
        int cx = offset_x + static_cast<int>(ry) * cell_size;
        int cy = offset_y + static_cast<int>(rx) * cell_size;
        int margin = cell_size / 8;
        return QRect(cx + margin, cy + margin, cell_size - margin * 2, cell_size - margin * 2);
    };

    // 用无填充画刷，圈内保持透明，不遮住底下的棋子。
    painter.setBrush(Qt::NoBrush);
    // 绿色虚线、线宽 3：本回合规则允许挑选的己方棋子。
    painter.setPen(QPen(QColor("#2ecc71"), 3, Qt::DashLine));
    for (const auto& pos : selectable_pieces_) {
        // 外扩 4 像素，圈住棋子外缘而不压到棋子本体。
        QRect rect = get_piece_rect(pos.x, pos.y).adjusted(-4, -4, 4, 4);
        painter.drawEllipse(rect);
    }

    // 可达落点沿用上面那支绿虚线画笔（此处没有再 setPen），所以两者样式天然一致。
    // 注意: 若把上面的 setPen 连同 selectable_pieces_ 循环一起删掉，这里的圈会沿用画
    //       棋子编号时的白色实线画笔，不再与可动圈样式一致。
    for (const auto& target : reachable_targets_) {
        QRect rect = get_piece_rect(target.x, target.y).adjusted(-4, -4, 4, 4);
        painter.drawEllipse(rect);
    }

    // 已选中：金色实线、线宽 4、只外扩 2 像素，比可达圈更内收；
    // 同一格既是选中又是可落时，靠"实线 + 金色"与虚线圈区分开。画在最后，层级最高。
    if (selected_pos_) {
        painter.setPen(QPen(QColor("#f1c40f"), 4));
        QRect rect = get_piece_rect(selected_pos_->x, selected_pos_->y).adjusted(-2, -2, 2, 2);
        painter.drawEllipse(rect);
    }
}

void BoardWidget::mousePressEvent(QMouseEvent* event) {
    // 屏幕坐标与棋盘坐标刻意分开命名：click_x 是横坐标（对应列），click_y 是纵坐标（对应行）。
    int click_x = event->pos().x();
    int click_y = event->pos().y();

    // 与 paintEvent 共用同一份几何参数：两边都调用 compute_geometry，
    // 不会再出现“判定用 board_size、网格只有 cell_size * 5”的余量失配。
    const BoardGeometry geometry = compute_geometry(width(), height());
    const int cell_size = geometry.cell_size;
    const int offset_x = geometry.offset_x;
    const int offset_y = geometry.offset_y;

    // 边界判定是左闭右开：正好压在右边或下边线上的点算棋盘外，落在留白里的点击直接丢弃。
    if (click_x < offset_x || click_x >= offset_x + geometry.span || click_y < offset_y ||
        click_y >= offset_y + geometry.span) {
        return;
    }

    // 反推：横坐标落在第几列即 y，纵坐标落在第几行即 x，与 paintEvent 的换算互为逆运算。
    const std::size_t col_y = static_cast<std::size_t>((click_x - offset_x) / cell_size);
    const std::size_t row_x = static_cast<std::size_t>((click_y - offset_y) / cell_size);
    // 双保险：span 由 cell_size * 5 推出，理论上必然落在 0..4；
    // 这里仍防御性拒绝 5，避免以后改动几何参数时又把越界坐标抛给上层。
    if (col_y >= core::Board::kSize || row_x >= core::Board::kSize) {
        return;
    }
    const core::Pos clicked_pos{row_x, col_y};

    // 这里不做任何命中判定（有没有子、是不是己方、轮到谁）就把坐标抛出去，由接收方全权决定。
    emit cell_clicked(clicked_pos);
}

}  // namespace ui