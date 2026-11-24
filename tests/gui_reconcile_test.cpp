// AI 回合驱动回归：BUG-03（载入"AI 思考中"的存档后永久卡住）与
// BUG-09（AI 走子异常后界面停在"正在思考"）。
//
// 修复后 GameWidget::handle_ai_turn 按 phase 分派，可以从中断处继续推进；
// 走子失败则把回合判给人类，而不是把状态机留在 AI 侧。
// 这里直接载入两份人工构造的存档式快照，跑真实事件循环，观察界面是否回到人类回合。

#include <QApplication>
#include <QEventLoop>
#include <QLabel>
#include <QListWidget>
#include <QTimer>
#include <iostream>
#include <string_view>
#include <vector>

#include "ai/ai.h"
#include "core/game.h"
#include "views/game_widget.h"

namespace {

int failures = 0;

void expect(bool cond, std::string_view what) {
    std::cout << (cond ? "PASS" : "FAIL") << ' ' << what << '\n';
    if (!cond) {
        ++failures;
    }
}

// 让 Qt 事件循环（含 QTimer::singleShot 排的 AI 动作）跑到指定时刻。
void spin(int milliseconds) {
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}

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
        widget.resize(900, 700);
        widget.show();
        app.processEvents();

        auto* history = widget.findChild<QListWidget*>();
        widget.load_game_state(thinking, ai::kDefaultLevel);
        spin(2000);

        expect(history != nullptr && history->count() == 1,
               "loading a Player2Thinking save lets the AI finish its turn");
        expect(any_contains(label_texts(widget), QString::fromUtf8("轮到你了")),
               "status text returns to the human turn after loading");
    }

    {
        // 清空蓝方全部棋子：不论骰子几点都没有可动子，decide_move 必然抛异常。
        // 旧行为：只打印 qDebug，阶段停在 Player2Thinking，界面永久卡住。
        core::GameState broken = thinking;
        for (std::size_t x = 0; x < core::Board::kSize; ++x) {
            for (std::size_t y = 0; y < core::Board::kSize; ++y) {
                auto& piece = broken.board.at({x, y});
                if (piece.side == core::Side::Blue) {
                    piece = {core::Side::None, 0};
                }
            }
        }

        ui::GameWidget widget;
        widget.resize(900, 700);
        widget.show();
        app.processEvents();

        auto* history = widget.findChild<QListWidget*>();
        widget.load_game_state(broken, ai::kDefaultLevel);
        spin(2000);

        expect(history != nullptr && history->count() == 0,
               "AI with no legal move does not make a move");
        expect(any_contains(label_texts(widget), QString::fromUtf8("轮到你了")),
               "AI failure forfeits the turn instead of hanging in thinking");
        expect(any_contains(label_texts(widget), QString::fromUtf8("本回合判为放弃")),
               "AI failure surfaces a visible message");
    }

    {
        // BUG-11：旧实现用 QTimer::singleShot 排 AI，开新局/读档取消不掉。
        // 上一局排的 500ms 回调与新局的回调同时到点：第一个掷骰后阶段变成
        // Player2Thinking，第二个看到后就直接走子，把 800ms 的思考停顿吞掉。
        // 现在延迟动作换成成员 QTimer，start_new_game() 会先 cancel_pending_actions()。
        ui::GameWidget widget;
        widget.resize(900, 700);
        widget.show();
        app.processEvents();

        auto* history = widget.findChild<QListWidget*>();
        widget.load_game_state(thinking, ai::kDefaultLevel);  // 排一个 500ms 的 AI 回调
        widget.start_new_game(core::Side::Blue, {1, 2, 3, 4, 5, 6}, ai::Level::kSearch);

        spin(800);
        expect(history != nullptr && history->count() == 0,
               "new game cancels the AI action queued by the previous game");
        spin(1500);
        expect(history != nullptr && history->count() == 1,
               "AI still takes exactly one turn in the new game");
    }

    if (failures == 0) {
        std::cout << "ALL PASS\n";
    } else {
        std::cout << failures << " FAILURE(S)\n";
    }
    return failures == 0 ? 0 : 1;
}
