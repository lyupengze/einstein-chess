/// @file tests/gui_reconcile_test.cpp
/// @brief AI 回合驱动回归：按阶段续跑、异常中止与定时器取消。
///
/// GameWidget::handle_ai_turn 按 phase 分派，因此载入“轮到 AI 思考”的存档后
/// 能从中断处继续推进；走子失败则进入“对局异常”中止态，保留原始局面并禁用
/// 操作，不伪造规则外的“放弃回合”；返回主页会停掉尚未触发的 AI 定时器。
///
/// 时序不写死：先把 GameWidget 的三段延迟压到毫秒级，再用 qt_wait.h 轮询真实
/// 状态。慢机器上只是多转几圈事件循环，不会因为 sleep 不够而假失败。

#include <QApplication>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <iostream>
#include <string_view>
#include <vector>

#include "ai/ai.h"
#include "components/board_widget.h"
#include "core/game.h"
#include "qt_wait.h"
#include "test_support.h"
#include "views/game_widget.h"

using test_support::expect;

namespace {

std::vector<QString> label_texts(gui::GameWidget& widget) {
    std::vector<QString> texts;
    for (auto* label : widget.findChildren<QLabel*>()) {
        texts.push_back(label->text());
    }
    return texts;
}

bool any_contains(const std::vector<QString>& texts, const QString& needle) {
    for (const auto& text : texts) {
        if (text.contains(needle)) {
            return true;
        }
    }
    return false;
}

QPushButton* find_button(gui::GameWidget& widget, const QString& text) {
    for (auto* button : widget.findChildren<QPushButton*>()) {
        if (button->text() == text) {
            return button;
        }
    }
    return nullptr;
}

// 造出"轮到蓝方(AI)思考"的真实局面：红先走一步，蓝掷骰后停在 Player2Thinking。
core::GameState make_ai_thinking_state() {
    core::Game game;
    game.prepare_new_game(core::Side::Red);
    game.submit_layout(core::Side::Red, {1, 2, 3, 4, 5, 6});
    game.submit_layout(core::Side::Blue, ai::decide_layout());
    game.roll_dice();
    const auto red_move = ai::decide_move(game.get_state());
    game.make_move(red_move.from, red_move.to);
    game.roll_dice();
    return game.get_state();
}

}  // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    const core::GameState thinking = make_ai_thinking_state();

    {
        // 载入的存档停在 Player2Thinking：handle_ai_turn 必须接着把这一手走完，
        // 而不是只处理 ThrowingDice 后把界面留在“正在思考”。
        gui::GameWidget widget;
        widget.set_action_delays(1, 1, 1);  // 毫秒级推进，不必等真实的 500/800ms
        widget.resize(900, 700);
        widget.show();
        app.processEvents();

        auto* history = widget.findChild<QListWidget*>();
        expect(history != nullptr, "对局页暴露历史列表");
        widget.load_game_state(thinking, ai::kDefaultLevel);

        expect(test_support::wait_until([&] { return history != nullptr && history->count() == 1; },
                                        3000, "载入后 AI 完成该回合"),
               "载入 Player2Thinking 存档后 AI 补完该回合");
        expect(test_support::wait_until(
                   [&] { return any_contains(label_texts(widget), QString::fromUtf8("轮到你了")); },
                   3000, "状态文案回到人类回合"),
               "载入后状态文案回到人类回合");
    }

    {
        // 用合法局面 + 非法档位触发 decide_move 的异常。不能用缺子或已满足胜利
        // 条件的快照：load_game_state 会先做局面校验，这类数据在入口就被拒绝，
        // 测不到 handle_ai_turn 的异常兜底。非法枚举值能通过局面校验。
        gui::GameWidget widget;
        widget.set_action_delays(1, 1, 1);
        widget.resize(900, 700);
        widget.show();
        app.processEvents();

        auto* history = widget.findChild<QListWidget*>();
        auto* board = widget.findChild<gui::BoardWidget*>();
        auto* roll = find_button(widget, "掷骰子");
        auto* save = find_button(widget, "保存");
        expect(history != nullptr, "对局页暴露历史列表");
        expect(board != nullptr, "对局页暴露棋盘");
        expect(roll != nullptr, "对局页暴露掷骰按钮");
        expect(save != nullptr, "对局页暴露保存按钮");
        widget.load_game_state(thinking, static_cast<ai::Level>(99));

        expect(test_support::wait_until(
                   [&] { return any_contains(label_texts(widget), QString::fromUtf8("对局异常")); },
                   3000, "AI 抛异常时中止对局"),
               "AI 抛异常时中止对局，不伪造放弃回合");
        expect(history != nullptr && history->count() == 0, "异常发生在选子前，不落子");
        expect(!any_contains(label_texts(widget), QString::fromUtf8("轮到你了")),
               "中止态不把回合交给人类");
        expect(board != nullptr && !board->isEnabled(), "中止态禁用棋盘");
        expect(roll != nullptr && !roll->isEnabled(), "中止态禁用掷骰按钮");
        expect(save != nullptr && !save->isEnabled(), "中止态禁用保存按钮");

        // 中止态必须能被下一次读档清除，否则玩家只能重启程序。
        widget.load_game_state(thinking, ai::kDefaultLevel);
        expect(test_support::wait_until([&] { return history != nullptr && history->count() == 1; },
                                        3000, "中止后读档能继续完成回合"),
               "合法读档清除中止态");
        expect(test_support::wait_until(
                   [&] { return any_contains(label_texts(widget), QString::fromUtf8("轮到你了")); },
                   3000, "恢复后状态文案回到人类回合"),
               "恢复后回到人类回合");
        expect(board != nullptr && board->isEnabled(), "恢复后启用棋盘");
        expect(roll != nullptr && roll->isEnabled(), "恢复后启用掷骰按钮");
        expect(save != nullptr && save->isEnabled(), "恢复后启用保存按钮");
    }

    {
        // 延迟动作由成员 QTimer 承载，start_new_game() 先 cancel_pending_actions()。
        // 若用 QTimer::singleShot 排期，开新局或读档取消不掉：上一局的回调与新局的
        // 回调同时到点，第一个掷骰后阶段变成 Player2Thinking，第二个看到就直接走子，
        // 把思考停顿吞掉。这里把走子后等待与思考都设成 150ms，正确实现要 300ms 才
        // 落子，250ms 的观察窗足以区分。
        gui::GameWidget widget;
        widget.set_action_delays(1, 150, 150);
        widget.resize(900, 700);
        widget.show();
        app.processEvents();

        auto* history = widget.findChild<QListWidget*>();
        expect(history != nullptr, "对局页暴露历史列表");
        widget.load_game_state(thinking, ai::kDefaultLevel);  // 排一个 150ms 的 AI 回调
        widget.start_new_game(core::Side::Blue, {1, 2, 3, 4, 5, 6}, ai::Level::kSearch);

        expect(test_support::expect_remains_false(
                   [&] { return history != nullptr && history->count() > 0; }, 250,
                   "新局思考延迟前出现 AI 落子"),
               "新局取消上一局排队的 AI 动作");
        expect(test_support::wait_until([&] { return history != nullptr && history->count() == 1; },
                                        3000, "新局中 AI 正常走一手"),
               "新局中 AI 只走一手");
        // 新局里 AI 只有第一手（红方）该动，之后轮到人类蓝方；再等一个窗口确认没有第二手。
        expect(test_support::expect_remains_false(
                   [&] { return history != nullptr && history->count() > 1; }, 250,
                   "同一人类回合内出现第二次 AI 落子"),
               "新局不额外排队 AI 回合");
    }

    {
        // 返回主页必须停掉已经排队的 AI 动作：否则玩家切回菜单后，AI 仍在后台
        // 修改一个既看不到、也没有“继续对局”入口的局面。
        gui::GameWidget widget;
        widget.set_action_delays(1, 150, 150);
        widget.resize(900, 700);
        widget.show();
        app.processEvents();

        auto* history = widget.findChild<QListWidget*>();
        auto* exit = find_button(widget, "返回主页");
        expect(history != nullptr, "对局页暴露历史列表");
        expect(exit != nullptr, "对局页暴露返回主页按钮");
        widget.load_game_state(thinking, ai::kDefaultLevel);  // 排一个 150ms 的 AI 回调
        exit->click();

        expect(test_support::expect_remains_false(
                   [&] { return history != nullptr && history->count() > 0; }, 300,
                   "返回主页后出现 AI 落子"),
               "返回主页取消排队动作");
    }

    return test_support::finish();
}
