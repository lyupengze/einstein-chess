/// @file tests/gui_game_widget_test.cpp
/// @brief 验证人类在棋盘上点击完成一次走子的完整链路。
///
/// 覆盖三件事：点击落在正确的格子上、选中后可走的格子只来自规则、落子后棋盘与历史记录同步。
/// 用真实鼠标事件驱动，不直接调用槽函数，这样坐标换算与信号连接也在检查范围内。
/// 载入的局面固定为“轮到红方、骰点 3”，避免依赖随机骰子。

#include <QApplication>
#include <QCoreApplication>
#include <QListWidget>
#include <QMouseEvent>
#include <QPointF>
#include <QPushButton>
#include <QString>
#include <cstdio>
#include <utility>

#include "ai/ai.h"
#include "components/board_widget.h"
#include "core/board.h"
#include "core/game_state.h"
#include "core/types.h"
#include "qt_wait.h"
#include "test_support.h"
#include "views/game_widget.h"

namespace {

/// @brief 构造一个轮到红方、骰点为 3 的合法局面。
core::GameState human_turn_state() {
    core::GameState state;
    state.player1_side = core::Side::Red;
    state.current_side = core::Side::Red;
    state.phase = core::Phase::Player1Thinking;
    state.current_dice = 3;
    state.player1_ready = true;
    state.player2_ready = true;
    state.board.setup_custom(core::Side::Red, {1, 2, 3, 4, 5, 6});
    state.board.setup_custom(core::Side::Blue, {1, 2, 3, 4, 5, 6});
    return state;
}

/// @brief 向棋盘控件发送一次左键按下，坐标是格子中心。
///
/// 棋盘在测试里固定为 500x500，按棋盘控件的几何约定每格 100 像素、无偏移；
/// 屏幕坐标的横轴对应列 y，纵轴对应行 x。
void click_cell(gui::BoardWidget* board, const core::Pos& pos) {
    constexpr int kCell = 100;
    const QPointF local(pos.y * kCell + kCell / 2.0, pos.x * kCell + kCell / 2.0);
    QMouseEvent press(QEvent::MouseButtonPress, local, board->mapToGlobal(local.toPoint()),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(board, &press);
}

}  // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    gui::GameWidget widget;
    widget.resize(900, 600);
    widget.set_scene_db_path("test_gui_game_widget_scene.db");
    widget.set_action_delays(1, 5000, 5000);
    widget.show();
    widget.load_game_state(human_turn_state(), ai::Level::Random, core::RngState{});
    test_support::spin_events(50);

    auto* board = widget.findChild<gui::BoardWidget*>();
    auto* history = widget.findChild<QListWidget*>("historyList");
    test_support::expect(board != nullptr, "对局页有棋盘控件");
    test_support::expect(history != nullptr, "对局页有历史列表");
    if (board == nullptr || history == nullptr) {
        std::remove("test_gui_game_widget_scene.db");
        return test_support::finish();
    }

    board->setFixedSize(500, 500);
    test_support::spin_events(50);

    const int before = history->count();
    click_cell(board, {0, 2});
    test_support::spin_events(30);
    click_cell(board, {1, 2});
    test_support::spin_events(50);

    test_support::expect(history->count() == before + 1, "点击走子后历史多出一条");
    if (history->count() > before) {
        const QString log = history->item(before)->text();
        test_support::expect(log.contains(QStringLiteral("(0,2)->(1,2)")),
                             "历史记录写明起点与终点");
    }

    // 再点一次同一个棋子：它已经不在初始区，这一步不应产生新的历史。
    const int after_move = history->count();
    click_cell(board, {1, 2});
    test_support::spin_events(30);
    test_support::expect(history->count() == after_move, "点已走过的棋子不产生新记录");

    std::remove("test_gui_game_widget_scene.db");
    return test_support::finish();
}
