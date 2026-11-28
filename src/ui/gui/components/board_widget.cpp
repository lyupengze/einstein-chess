/// @file src/ui/gui/components/board_widget.cpp
/// @brief 绘制棋盘快照，并把鼠标点击换算回棋盘坐标。
///
/// 绘制严格分层，后画的覆盖先画的：棋盘格 → 红/蓝初始区 → 网格线 →
/// 棋子（投影 + 径向渐变 + 高光 + 编号）→ 三种高亮圈；高亮放在最后，
/// 保证它永远压在棋子之上。坐标换算在两处出现且互为逆运算：
/// paintEvent 由 (行 x, 列 y) 求出屏幕 (纵, 横)，mousePressEvent 再反推。

#include "board_widget.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <algorithm>
#include <cstddef>

namespace gui {

namespace {

/// @brief 网格边长，取自 core::Board::kSize，绘制循环统一引用它。
constexpr int kSide{static_cast<int>(core::Board::kSize)};

// 棋盘几何参数。绘制与命中判定共用这一份计算，边界一律用 span：
// span = cell_size * kSide 才是棋盘实际覆盖的边长，min(w, h) 的整除余量不算棋盘。
// 若拿 board_size 判边界，最右/最下一小条会把点击换算成下标 5 并越界。
struct BoardGeometry {
    int cell_size = 0;  ///< 单格边长。
    int span = 0;       ///< 棋盘实际覆盖的像素边长，恒等于 cell_size * 5。
    int offset_x = 0;   ///< 棋盘左上角横坐标（居中留白）。
    int offset_y = 0;   ///< 棋盘左上角纵坐标。
};

BoardGeometry compute_geometry(int widget_width, int widget_height) {
    BoardGeometry geometry;
    const int board_size = std::min(widget_width, widget_height);
    geometry.cell_size = board_size / kSide;
    geometry.span = geometry.cell_size * kSide;
    // 按 span 而不是 board_size 居中：上限最多 4 像素的整除余量平均分到两侧，
    // 保证网格、区域填充与点击判定共用同一条边。
    geometry.offset_x = (widget_width - geometry.span) / 2;
    geometry.offset_y = (widget_height - geometry.span) / 2;
    return geometry;
}

// 一格在屏幕上占用的整数矩形：右、下边是开区间，相邻格不共享像素行。
// 初始区按格子矩形拼成一条路径，台阶边界正好落在网格线上。
QRect cell_rect(const BoardGeometry& geometry, const core::Pos& pos) {
    return QRect(geometry.offset_x + static_cast<int>(pos.y) * geometry.cell_size,
                 geometry.offset_y + static_cast<int>(pos.x) * geometry.cell_size,
                 geometry.cell_size, geometry.cell_size);
}

// 棋子在格内占用的矩形：四周各留 1/8 格边距，相邻棋子之间才有间隙。
// 绘制棋子与绘制高亮圈共用这一份计算，改边距时只需改这里，
// 不会出现“棋子按一套边距画、圈按另一套边距画”的错位。
QRect piece_rect_at(int offset_x, int offset_y, int cell_size, int row, int col) {
    const int margin = cell_size / 8;
    return QRect(offset_x + col * cell_size + margin, offset_y + row * cell_size + margin,
                 cell_size - margin * 2, cell_size - margin * 2);
}

}  // namespace

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
    const BoardGeometry geometry = compute_geometry(width(), height());
    const int cell_size = geometry.cell_size;
    const int board_size = geometry.span;
    const int offset_x = geometry.offset_x;
    const int offset_y = geometry.offset_y;

    // 第一层：每个格子按 (row + col) 的奇偶交替填色，得到经典棋盘纹；
    // 这里的 row/col 就是 Pos.x / Pos.y，屏幕位置按 (列→横、行→纵) 摆放。
    for (int row = 0; row < kSide; ++row) {
        for (int col = 0; col < kSide; ++col) {
            QColor cell_color = ((row + col) % 2 == 0) ? QColor("#ecf0f1") : QColor("#bdc3c7");
            painter.fillRect(offset_x + col * cell_size, offset_y + row * cell_size, cell_size,
                             cell_size, cell_color);
        }
    }

    // 区域填充不描边，只靠 alpha=50 的半透明色叠在格子之上。
    painter.setPen(Qt::NoPen);

    // 第二层：红/蓝初始区。整片区域装进一个 QPainterPath，只调用一次 fillPath：
    // 相邻格共享的边在路径内部互相抵消，覆盖率按并集算，不会出现两次 alpha 叠加的接缝。
    // 格子表来自 core::Board，表的顺序变了这里自动跟随。
    QPainterPath red_region;
    for (const core::Pos& cell : core::Board::kRedStartPositions) {
        red_region.addRect(cell_rect(geometry, cell));
    }
    painter.fillPath(red_region, QColor(231, 76, 60, 50));

    QPainterPath blue_region;
    for (const core::Pos& cell : core::Board::kBlueStartPositions) {
        blue_region.addRect(cell_rect(geometry, cell));
    }
    painter.fillPath(blue_region, QColor(52, 152, 219, 50));

    // 第三层：6 横 + 6 纵的网格线（i 取 0~5，四条边也算在内），灰线 1 像素。
    painter.setPen(QPen(QColor("#95a5a6"), 1));
    for (int i = 0; i <= kSide; ++i) {
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

            // 关键换算：行号 x 决定屏幕纵坐标，列号 y 决定屏幕横坐标，即按矩阵习惯摆放。
            // 棋子矩形与高亮圈共用 piece_rect_at 的边距，两者不会错位。
            const QRect piece_rect = piece_rect_at(offset_x, offset_y, cell_size,
                                                   static_cast<int>(x), static_cast<int>(y));

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

            // 编号：白色加粗，字号取 1/3 格宽，在棋子矩形内居中。
            painter.setPen(Qt::white);
            QFont font = painter.font();
            font.setPixelSize(cell_size / 3);
            font.setBold(true);
            painter.setFont(font);
            painter.drawText(piece_rect, Qt::AlignCenter, QString::number(piece.id));
        }
    }

    // 第五层：高亮圈。矩形与棋子共用 piece_rect_at，只在这里按圈的类型做外扩。
    auto rect_for = [&](core::Pos pos) {
        return piece_rect_at(offset_x, offset_y, cell_size, static_cast<int>(pos.x),
                             static_cast<int>(pos.y));
    };

    // 无填充画刷让圈内保持透明；绿色虚线、线宽 3 表示本回合可选中的己方棋子。
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor("#2ecc71"), 3, Qt::DashLine));
    for (const auto& pos : selectable_pieces_) {
        // 外扩 4 像素，圈住棋子外缘而不压到棋子本体。
        QRect rect = rect_for(pos).adjusted(-4, -4, 4, 4);
        painter.drawEllipse(rect);
    }

    // 可达落点沿用上面那支绿虚线画笔（此处没有再 setPen），所以两者样式天然一致。
    // 注意：若把上面的 setPen 连同 selectable_pieces_ 循环一起删掉，这里的圈会沿用画
    //       棋子编号时的白色实线画笔，与可动圈样式不一致。
    for (const auto& target : reachable_targets_) {
        QRect rect = rect_for(target).adjusted(-4, -4, 4, 4);
        painter.drawEllipse(rect);
    }

    // 已选中：金色实线、线宽 4、只外扩 2 像素，比可达圈更内收；
    // 同一格既是选中又是可落时，靠QStringLiteral("实线 + 金色")与虚线圈区分开。画在最后，层级最高。
    if (selected_pos_) {
        painter.setPen(QPen(QColor("#f1c40f"), 4));
        QRect rect = rect_for(*selected_pos_).adjusted(-2, -2, 2, 2);
        painter.drawEllipse(rect);
    }
}

void BoardWidget::mousePressEvent(QMouseEvent* event) {
    // 只认左键：右键/中键不能误触走子或改变选中格，交还基类处理。
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    // 屏幕坐标与棋盘坐标刻意分开命名：click_x 是横坐标（对应列），click_y 是纵坐标（对应行）。
    int click_x = event->pos().x();
    int click_y = event->pos().y();

    // 与 paintEvent 共用 compute_geometry，边界与绘制落在同一条边上。
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
    // 这里显式拒绝越界值，避免几何参数改动后把非法坐标抛给上层。
    if (col_y >= core::Board::kSize || row_x >= core::Board::kSize) {
        return;
    }
    const core::Pos clicked_pos{row_x, col_y};

    // 这里不做任何命中判定（有没有子、是不是己方、轮到谁）就把坐标抛出去，由接收方全权决定。
    emit cell_clicked(clicked_pos);
}

}  // namespace gui
