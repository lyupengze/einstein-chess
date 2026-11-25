// AI 回合驱动回归：BUG-03（载入"AI 思考中"的存档后永久卡住）与
// BUG-09（AI 走子异常后界面停在"正在思考"）。
//
// 修复后 GameWidget::handle_ai_turn 按 phase 分派，可以从中断处继续推进；
// 走子失败则把回合判给人类，而不是把状态机留在 AI 侧。
// 这里直接载入两份人工构造的存档式快照，跑真实事件循环，观察界面是否回到人类回合。
//
// 时序不写死：先把 GameWidget 的三段延迟压到毫秒级，再用 qt_wait.h 轮询
// 真实状态。慢机器上只是多转几圈事件循环，不会因为 sleep 不够而假失败。

#include <QApplication>
#include <QLabel>
#include <QListWidget>
#include <iostream>
#include <string_view>
#include <vector>

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
        // 旧行为：只打印 qDebug，阶段停在 Player2Thinking，界面永久卡住。
        ui::GameWidget widget;
        widget.set_action_delays(1, 1, 1);
        widget.resize(900, 700);
        widget.show();
        app.processEvents();

        auto* history = widget.findChild<QListWidget*>();
        expect(history != nullptr, "game widget exposes its history list");
        widget.load_game_state(thinking, static_cast<ai::Level>(99));

        expect(test_support::wait_until(
                   [&] {
                       return any_contains(label_texts(widget), QString::fromUtf8("本回合判为放弃"));
                   },
                   3000, "AI failure surfaces a visible message"),
               "AI failure surfaces a visible message");
        expect(history != nullptr && history->count() == 0,
               "AI that throws before choosing a move does not make a move");
        expect(test_support::wait_until(
                   [&] { return any_contains(label_texts(widget), QString::fromUtf8("轮到你了")); },
                   3000, "status text returns to the human turn"),
               "AI failure forfeits the turn instead of hanging in thinking");
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

    return test_support::finish();
}
