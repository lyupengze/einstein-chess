// AI 回合驱动回归：BUG-03（载入"AI 思考中"的存档后永久卡住）与
// BUG-09（AI 走子异常后界面停在"正在思考"）。
//
// 修复后 GameWidget::handle_ai_turn 按 phase 分派，可以从中断处继续推进；
// 走子失败则进入"对局异常"中止态，保留原始局面并禁用操作，而不是伪造一次
// 规则外的"放弃回合"。返回主页会停掉尚未触发的 AI 定时器。
//
// 时序不写死：先把 GameWidget 的三段延迟压到毫秒级，再用 qt_wait.h 轮询
// 真实状态。慢机器上只是多转几圈事件循环，不会因为 sleep 不够而假失败。

#include <QApplication>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <iostream>
#include <string_view>
#include <vector>

#include "components/board_widget.h"

#include "ai/ai.h"
#include "core/game.h"
#include "qt_wait.h"
#include "views/game_widget.h"

#include "test_support.h"

using test_support::expect;

namespace {

std::vector<QString> label_texts(ui::GameWidget& widget) {
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

QPushButton* find_button(ui::GameWidget& widget, const QString& text) {
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
        // 旧行为：handle_ai_turn 只处理 ThrowingDice，Player2Thinking 直接返回，
        // 于是定时器不再重排，界面永久停在"正在思考..."。
        ui::GameWidget widget;
        widget.set_action_delays(1, 1, 1);  // 毫秒级推进，不必等真实的 500/800ms
        widget.resize(900, 700);
        widget.show();
        app.processEvents();

        auto* history = widget.findChild<QListWidget*>();
        expect(history != nullptr, "game widget exposes its history list");
        widget.load_game_state(thinking, ai::kDefaultLevel);

        expect(test_support::wait_until(
                   [&] { return history != nullptr && history->count() == 1; }, 3000,
                   "AI finishes the turn from a loaded Player2Thinking save"),
               "loading a Player2Thinking save lets the AI finish its turn");
        expect(test_support::wait_until(
                   [&] { return any_contains(label_texts(widget), QString::fromUtf8("轮到你了")); },
                   3000, "status text returns to the human turn"),
               "status text returns to the human turn after loading");
    }

    {
        // 用合法局面 + 非法 AI 档位触发 decide_move 的“未知 AI 版本”异常。
        // 不能再用“清空蓝方全部棋子”的残缺局面：load_game_state 现在会先做
        // 局面校验，缺子/已满足胜利条件的快照会在入口被拒绝，测不到 handle_ai_turn
        // 的异常兜底。非法枚举值能通过局面校验，但会让 decide_move 抛异常。
        ui::GameWidget widget;
        widget.set_action_delays(1, 1, 1);
        widget.resize(900, 700);
        widget.show();
        app.processEvents();

        auto* history = widget.findChild<QListWidget*>();
        auto* board = widget.findChild<ui::BoardWidget*>();
        auto* roll = find_button(widget, "掷骰子");
        auto* save = find_button(widget, "保存");
        expect(history != nullptr, "game widget exposes its history list");
        expect(board != nullptr, "game widget exposes its board");
        expect(roll != nullptr, "game widget exposes its roll button");
        expect(save != nullptr, "game widget exposes its save button");
        widget.load_game_state(thinking, static_cast<ai::Level>(99));

        expect(test_support::wait_until(
                   [&] {
                       return any_contains(label_texts(widget), QString::fromUtf8("对局异常"));
                   },
                   3000, "AI failure aborts the game"),
               "AI failure aborts the game instead of forfeiting the turn");
        expect(history != nullptr && history->count() == 0,
               "AI that throws before choosing a move does not make a move");
        expect(!any_contains(label_texts(widget), QString::fromUtf8("轮到你了")),
               "aborted game does not hand the turn to the human");
        expect(board != nullptr && !board->isEnabled(), "aborted game disables the board");
        expect(roll != nullptr && !roll->isEnabled(), "aborted game disables the dice button");
        expect(save != nullptr && !save->isEnabled(), "aborted game disables the save button");

        // 中止态必须能被下一次读档清除，否则玩家只能重启程序。
        widget.load_game_state(thinking, ai::kDefaultLevel);
        expect(test_support::wait_until(
                   [&] { return history != nullptr && history->count() == 1; }, 3000,
                   "AI finishes a turn after recovering from abort"),
               "loading a valid save clears the aborted state");
        expect(test_support::wait_until(
                   [&] { return any_contains(label_texts(widget), QString::fromUtf8("轮到你了")); },
                   3000, "status text returns to the human turn after recovery"),
               "recovered game returns to the human turn");
        expect(board != nullptr && board->isEnabled(), "recovered game re-enables the board");
        expect(roll != nullptr && roll->isEnabled(), "recovered game enables the dice button");
        expect(save != nullptr && save->isEnabled(), "recovered game re-enables the save button");
    }

    {
        // BUG-11：旧实现用 QTimer::singleShot 排 AI，开新局/读档取消不掉。
        // 上一局排的回调与新局的回调同时到点：第一个掷骰后阶段变成
        // Player2Thinking，第二个看到后就直接走子，把思考停顿吞掉。
        // 现在延迟动作换成成员 QTimer，start_new_game() 会先 cancel_pending_actions()。
        //
        // 这里把“走子后等待”设成 150ms、“思考”也设成 150ms：
        // 正确实现要 300ms 才落子，旧实现会在 150ms 就被第二个回调直接走完，
        // 因此 250ms 的观察窗能把两者区分开。
        ui::GameWidget widget;
        widget.set_action_delays(1, 150, 150);
        widget.resize(900, 700);
        widget.show();
        app.processEvents();

        auto* history = widget.findChild<QListWidget*>();
        expect(history != nullptr, "game widget exposes its history list");
        widget.load_game_state(thinking, ai::kDefaultLevel);  // 排一个 150ms 的 AI 回调
        widget.start_new_game(core::Side::Blue, {1, 2, 3, 4, 5, 6}, ai::Level::kSearch);

        expect(test_support::expect_remains_false(
                   [&] { return history != nullptr && history->count() > 0; }, 250,
                   "an AI move before the new game's own think delay"),
               "new game cancels the AI action queued by the previous game");
        expect(test_support::wait_until(
                   [&] { return history != nullptr && history->count() == 1; }, 3000,
                   "AI takes its turn in the new game"),
               "AI still takes exactly one turn in the new game");
        // 新局里 AI 只有第一手（红方）该动，之后轮到人类蓝方；再等一个窗口确认没有第二手。
        expect(test_support::expect_remains_false(
                   [&] { return history != nullptr && history->count() > 1; }, 250,
                   "a second AI move in the same human turn"),
               "the new game does not queue an extra AI turn");
    }

    {
        // 返回主页必须停掉已经排队的 AI 动作：否则玩家切回菜单后，AI 仍在后台
        // 修改一个既看不到、也没有“继续对局”入口的局面。
        ui::GameWidget widget;
        widget.set_action_delays(1, 150, 150);
        widget.resize(900, 700);
        widget.show();
        app.processEvents();

        auto* history = widget.findChild<QListWidget*>();
        auto* exit = find_button(widget, "返回主页");
        expect(history != nullptr, "game widget exposes its history list");
        expect(exit != nullptr, "game widget exposes its exit button");
        widget.load_game_state(thinking, ai::kDefaultLevel);  // 排一个 150ms 的 AI 回调
        exit->click();

        expect(test_support::expect_remains_false(
                   [&] { return history != nullptr && history->count() > 0; }, 300,
                   "an AI move after returning to the menu"),
               "returning to the menu cancels the queued AI action");
    }

    return test_support::finish();
}
