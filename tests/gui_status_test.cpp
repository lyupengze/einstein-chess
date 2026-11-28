/// @file tests/gui_status_test.cpp
/// @brief 状态栏文案回归：骰点为空时显示 ?，掷骰失败后按钮能恢复。
///
/// 思考阶段缺骰子的快照会被局面校验拒绝，界面收不到这种状态；同一骰点文案
/// 分支在合法布阵局面上仍会走到，可用来验证判空。掷骰失败则通过直接触发槽
/// 模拟阶段错乱，随后的人类回合必须重新点亮按钮。回归背景：见
/// `#gui-status-empty-dice` 与 `#gui-roll-button-stuck`。

#include <QApplication>
#include <QLabel>
#include <QListWidget>
#include <QMetaObject>
#include <QPushButton>
#include <QString>
#include <cstdio>
#include <stdexcept>
#include <vector>

#include "ai/ai.h"
#include "core/game.h"
#include "qt_wait.h"
#include "storage/save_manager.h"
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

// 与 gui_reconcile_test.cpp 相同的构造：红先走一步，蓝掷骰后停在 Player2Thinking。
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

// 只提交红方布阵：仍是 LayOut、current_side 为 None、current_dice 为空，
// 但能通过局面校验，因此会走到状态栏的骰点文案。
core::GameState make_layout_state() {
    core::Game game;
    game.prepare_new_game(core::Side::Red);
    game.submit_layout(core::Side::Red, {1, 2, 3, 4, 5, 6});
    return game.get_state();
}

bool load_throws(gui::GameWidget& widget, const core::GameState& state) {
    try {
        widget.load_game_state(state, ai::kDefaultLevel, core::RngState{});
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    {
        // 合法布阵局面没有骰子：状态栏仍要刷新完成，并把空点数显示成 ?。
        gui::GameWidget widget;
        widget.set_action_delays(1, 1, 1);
        widget.resize(900, 700);
        widget.show();
        app.processEvents();

        auto* roll = find_button(widget, QStringLiteral("掷骰子"));
        expect(roll != nullptr, "对局页暴露掷骰按钮");

        widget.load_game_state(make_layout_state(), ai::kDefaultLevel, core::RngState{});
        expect(any_contains(label_texts(widget), QStringLiteral("?")),
               "无骰子局面显示 ? 而不是中止刷新");
        expect(roll != nullptr && !roll->isEnabled(), "无骰子局面不点亮掷骰按钮");

        // 思考阶段缺骰子的快照在入口就被拒绝，界面根本收不到；把它固定成断言，
        // 避免以后把这种快照当成可载入的数据。
        core::GameState missing_dice = make_ai_thinking_state();
        missing_dice.current_dice.reset();
        expect(load_throws(widget, missing_dice), "思考阶段缺骰子在载入时被拒绝");
    }

    {
        // 在 AI 思考阶段直接触发掷骰槽，模拟阶段错乱；失败后按钮必须随人类回合恢复。
        // AI 的落子延时给到 3 秒，让「掷骰失败不落子」的断言有足够余量，不受机器卡顿影响。
        gui::GameWidget widget;
        widget.set_action_delays(1, 3000, 1);
        widget.resize(900, 700);
        widget.show();
        app.processEvents();

        auto* history = widget.findChild<QListWidget*>();
        auto* roll = find_button(widget, QStringLiteral("掷骰子"));
        expect(history != nullptr, "对局页暴露历史列表");
        expect(roll != nullptr, "对局页暴露掷骰按钮");

        widget.load_game_state(make_ai_thinking_state(), ai::kDefaultLevel, core::RngState{});

        expect(QMetaObject::invokeMethod(&widget, "on_roll_dice_clicked", Qt::DirectConnection),
               "能直接触发掷骰槽");

        expect(test_support::wait_until(
                   [&] { return any_contains(label_texts(widget), QStringLiteral("掷骰失败")); },
                   1000, "掷骰失败提示出现"),
               "非掷骰阶段掷骰会提示失败原因");
        expect(!any_contains(label_texts(widget), QStringLiteral("正在掷骰子")),
               "失败后文案离开正在掷骰子");
        expect(history != nullptr && history->count() == 0, "掷骰失败不落子");

        // AI 完成这一回合后回合交回人类，按钮必须重新可用，而不是一直灰着。
        expect(test_support::wait_until([&] { return history != nullptr && history->count() == 1; },
                                        8000, "AI 完成该回合"),
               "失败后 AI 回合继续推进");
        expect(test_support::wait_until([&] { return roll != nullptr && roll->isEnabled(); }, 3000,
                                        "掷骰按钮恢复可用"),
               "进入人类掷骰阶段后按钮恢复");
        expect(any_contains(label_texts(widget), QStringLiteral("轮到你了")),
               "按钮恢复时文案同步为人类回合");

        roll->click();
        expect(test_support::wait_until(
                   [&] { return any_contains(label_texts(widget), QStringLiteral("你掷出了")); },
                   3000, "恢复后能正常掷骰"),
               "恢复后的按钮可以完成一次真实掷骰");
    }

    {
        // 现场快照：开局后必须自动写出一份可读的存档，供开发者用 CLI 续跑复现。
        constexpr const char* kSceneDb = "test_gui_status_scene.db";
        std::remove(kSceneDb);

        gui::GameWidget widget;
        widget.set_action_delays(1, 1, 1);
        widget.set_scene_db_path(kSceneDb);
        widget.resize(900, 700);
        widget.show();
        app.processEvents();

        widget.start_new_game(core::Side::Red, {1, 2, 3, 4, 5, 6}, ai::Level::Search);

        bool snapshot_written = false;
        core::RngState snapshot_rng;
        try {
            storage::SaveManager manager(kSceneDb);
            const auto saved = manager.load_game(std::string(storage::kSceneSlot));
            snapshot_written = saved.has_value();
            if (saved) {
                snapshot_rng = saved->rng;
            }
        } catch (const std::exception&) {
            snapshot_written = false;
        }
        expect(snapshot_written, "开局后写出可读的现场快照");
        expect(snapshot_rng.rolls == 0, "现场快照记录已掷次数为 0");

        std::remove(kSceneDb);
    }

    return test_support::finish();
}
