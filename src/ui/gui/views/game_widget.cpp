/// @file src/ui/gui/views/game_widget.cpp
/// @brief 对局页：布局搭建、输入处理、界面刷新与 AI 回合时序。
///
/// 规则判定一律委托 core::Game 与 core::rules，本文件不重复实现方向、吃子与胜负。
/// 三段过渡延迟（掷骰确认 400、走子衔接 500、AI 思考 800）只为人眼可见，不参与规则。
/// 定时器是成员而非 QTimer::singleShot，开新局或读档时要能取消尚未触发的回调。

#include "game_widget.h"

#include <QDebug>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QScopeGuard>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <cstddef>
#include <exception>
#include <optional>
#include <string>
#include <vector>

#include "ai/ai.h"
#include "core/rules.h"
#include "dialogs/save_load_dialog.h"
#include "storage/save_manager.h"

namespace gui {

GameWidget::GameWidget(QWidget* parent) : QWidget(parent), roll_timer_(this), ai_timer_(this) {
    auto* main_layout = new QVBoxLayout(this);
    auto* top_layout = new QHBoxLayout();
    auto* content_layout = new QHBoxLayout();
    auto* right_panel = new QVBoxLayout();

    // 对应 style.qss 里的 QLabel#statusLabel。
    status_label_ = new QLabel("等待开始游戏...", this);
    status_label_->setObjectName("statusLabel");

    roll_btn_ = new QPushButton("掷骰子", this);
    exit_btn_ = new QPushButton("返回主页", this);
    save_btn_ = new QPushButton("保存", this);
    roll_btn_->setFixedSize(100, 40);
    exit_btn_->setFixedSize(100, 40);
    save_btn_->setFixedSize(100, 40);

    top_layout->addWidget(status_label_);
    top_layout->addStretch();
    top_layout->addWidget(roll_btn_);
    top_layout->addWidget(exit_btn_);
    top_layout->addWidget(save_btn_);
    main_layout->addLayout(top_layout);

    board_widget_ = new BoardWidget(this);
    dice_widget_ = new DiceWidget(this);
    history_list_ = new QListWidget(this);
    history_list_->setObjectName("historyList");
    history_list_->setFixedWidth(200);

    right_panel->addWidget(dice_widget_, 0, Qt::AlignHCenter);
    right_panel->addWidget(history_list_);
    content_layout->addWidget(board_widget_, 1);
    content_layout->addLayout(right_panel);
    main_layout->addLayout(content_layout, 1);

    connect(roll_btn_, &QPushButton::clicked, this, &GameWidget::on_roll_dice_clicked);
    connect(exit_btn_, &QPushButton::clicked, this, &GameWidget::on_exit_clicked);
    connect(save_btn_, &QPushButton::clicked, this, &GameWidget::on_save_clicked);
    connect(board_widget_, &BoardWidget::cell_clicked, this, &GameWidget::on_cell_clicked);

    roll_timer_.setSingleShot(true);
    ai_timer_.setSingleShot(true);
    connect(&roll_timer_, &QTimer::timeout, this, &GameWidget::on_roll_dice_timeout);
    connect(&ai_timer_, &QTimer::timeout, this, &GameWidget::handle_ai_turn);

    // 初始禁用；之后每次都由 update_ui() 按阶段重判。
    roll_btn_->setEnabled(false);
    save_btn_->setEnabled(false);

    // 浮层不参与布局，只在 show_message 时出现。
    toast_ = new ToastWidget(this);
}

void GameWidget::start_new_game(core::Side human_side,
                                const std::array<int, core::Board::kChessCount>& human_layout,
                                ai::Level ai_level) {
    // 先取消上一局排队的回调，否则旧的对局意图会落到新局。
    cancel_pending_actions();
    game_aborted_ = false;
    abort_reason_.clear();

    // 档位是整局配置，开新局时写入一次。
    ai_level_ = ai_level;

    // 每局换一个随机流：Game 的种子同时播种 AI，界面上不显示种子，但存档会记录，
    // 读档与复现都依赖它。
    game_ = core::Game();
    ai::set_random_seed(game_.seed());

    game_.prepare_new_game(human_side);
    game_.submit_layout(human_side, human_layout);

    // AI 布阵提交后，Game::check_both_ready 自动推进到 ThrowingDice 并固定红方先手。
    core::Side ai_side = (human_side == core::Side::Red) ? core::Side::Blue : core::Side::Red;
    game_.submit_layout(ai_side, ai::decide_layout());

    // 清掉上一局的选中格与历史，避免界面残留旧信息。
    selected_pos_ = std::nullopt;
    history_list_->clear();
    update_ui();
    snapshot_scene();

    // 人类执蓝时红方先手是电脑，必须主动启动 AI 回合。
    trigger_ai_if_needed();
}

void GameWidget::load_game_state(const core::GameState& state, ai::Level ai_level,
                                 core::RngState rng) {
    // 先取消排队中的旧动作，再整体覆盖对局。
    cancel_pending_actions();

    // 档位随局面一起恢复，后续 AI 回合与存档时一致。
    ai_level_ = ai_level;

    // load_game 整体覆盖、可能因局面不合法而抛异常；只有成功后才清除中止态，
    // 否则失败的读档会把中止中的旧局误恢复成可操作。
    game_.load_game(state);
    // 随机流同样来自存档：种子与已掷次数一起还原，AI 用同一个种子重新播种，
    // 读档后的骰序与 AI 决策才和存档前一致。
    game_.restore_rng_state(rng);
    ai::set_random_seed(rng.seed);
    game_aborted_ = false;
    abort_reason_.clear();
    selected_pos_ = std::nullopt;
    history_list_->clear();
    update_ui();
    snapshot_scene();

    // 存档可能停在 AI 回合，载入后同样检查是否该轮到电脑。
    trigger_ai_if_needed();
}

void GameWidget::set_action_delays(int roll_ms, int after_move_ms, int think_ms) {
    // 只改延迟，不动对局状态；应在 start_new_game / load_game_state 之前调用。
    roll_delay_ms_ = roll_ms;
    after_move_delay_ms_ = after_move_ms;
    think_delay_ms_ = think_ms;
}

void GameWidget::set_scene_db_path(const std::string& path) {
    scene_db_path_ = path;
    // 路径变了，之前打开的连接与目标文件对不上，必须丢弃。
    scene_save_.reset();
    scene_warned_ = false;
}

void GameWidget::snapshot_scene() noexcept {
    if (scene_db_path_.empty()) {
        return;
    }
    try {
        if (!scene_save_) {
            scene_save_.emplace(scene_db_path_);
        }
        scene_save_->save_game(std::string(storage::kSceneSlot), game_.get_state(), ai_level_,
                               game_.rng_state());
    } catch (const std::exception& error) {
        // 快照是排查辅助，写不进去不能打断对局；只提示一次，避免刷屏。
        if (!scene_warned_) {
            scene_warned_ = true;
            qWarning() << "现场快照写入失败:" << error.what();
        }
    }
}

void GameWidget::on_exit_clicked() {
    // 返回主页只停止推进：取消未触发的回调，但不清空 game_——
    // 这个局面玩家看不到也无法继续，下次开新局或读档会整体覆盖。
    cancel_pending_actions();
    emit exit_to_menu();
}

void GameWidget::on_roll_dice_clicked() {
    if (game_aborted_) {
        return;
    }
    // 立刻禁用，防止延迟期间被连点造成重复掷骰。
    roll_btn_->setEnabled(false);
    status_label_->setText("🎲 正在掷骰子...");

    // 400ms 后才真正掷骰，让“正在掷骰”先渲染一帧；成员定时器便于新局或读档取消。
    roll_timer_.start(roll_delay_ms_);
}

void GameWidget::on_roll_dice_timeout() {
    try {
        game_.roll_dice();
        update_ui();
        snapshot_scene();
    } catch (const std::exception& e) {
        // 正常路径不可达（按钮按阶段禁用）；失败后也必须 update_ui()，否则按钮停在禁用态。
        qDebug() << "掷骰子失败:" << e.what();
        show_message(QStringLiteral("掷骰失败: ") + QString::fromUtf8(e.what()));
        update_ui();
    }
}

void GameWidget::on_save_clicked() {
    if (game_aborted_) {
        show_message("对局已中止，无法保存");
        return;
    }

    // 对话框会进入嵌套事件循环，先冻结定时器，否则后台会改掉玩家点“保存”时看到的局面。
    const PendingActions paused = pause_action_timers();
    // 作用域守卫：确认、取消、异常都按剩余时间恢复；放在 try 外，catch 弹窗期间也保持暂停。
    [[maybe_unused]] const auto resume_guard =
        qScopeGuard([this, paused]() { resume_action_timers(paused); });

    // 对话框只收集名字，写库在调用方；kDefaultDbPath 是相对名，
    // 工作目录已切到 exe 所在目录，存档就落在 exe 旁边。
    // SaveLoadDialog 构造即打开数据库，必须与写库一起放进 try，否则异常会穿出事件循环。
    try {
        SaveLoadDialog dialog(DialogMode::Save, this);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        std::string name = dialog.get_save_name();
        storage::SaveManager save_manager(storage::kDefaultDbPath);
        save_manager.save_game(name, game_.get_state(), ai_level_, game_.rng_state());
        show_message("游戏已保存");
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "保存失败", e.what());
    }
}

void GameWidget::show_message(const QString& msg) {
    toast_->show_message(msg);
}

void GameWidget::add_history_log(const QString& text) {
    // 追加即发生顺序；滚到底保证最新一手可见。
    history_list_->addItem(text);
    history_list_->scrollToBottom();
}

void GameWidget::on_cell_clicked(core::Pos pos) {
    if (game_aborted_) {
        return;
    }
    // 快照：后续分支只读它，避免每步重新 get_state()。
    auto state = game_.get_state();
    // 越界点击直接丢弃，不让 Pos 流到 Board::at；
    // 坐标换算的误差见 `#board-remainder-click`。
    if (pos.x >= core::Board::kSize || pos.y >= core::Board::kSize) {
        return;
    }
    // 只接受“人类 + 轮到人类走子”的点击；Player1 指发起方而非红方，人类执蓝时同样成立。
    if (state.current_side != state.player1_side || state.phase != core::Phase::Player1Thinking) {
        return;
    }

    // 再点同一格 = 取消选中。
    if (selected_pos_ && *selected_pos_ == pos) {
        selected_pos_ = std::nullopt;
        update_ui();
        return;
    }

    // 已选中又点了别处：先当作走子。
    if (selected_pos_) {
        try {
            // 先拷出起点：make_move 成功后 selected_pos_ 被清空，再解引用就是空 optional。
            const core::Pos from_pos = *selected_pos_;

            game_.make_move(*selected_pos_, pos);
            selected_pos_ = std::nullopt;

            // 用快照读终点：落子后棋盘那格已是己方棋子，只有快照还留着被吃的棋子。
            auto target_piece = state.board.at(pos);
            QString log = QStringLiteral("你: (%1,%2)->(%3,%4)")
                              .arg(from_pos.x)
                              .arg(from_pos.y)
                              .arg(pos.x)
                              .arg(pos.y);
            // 规则允许吃己方棋子，所以“吃”后面可能是己方编号。
            if (target_piece.side != core::Side::None) {
                log += QStringLiteral(" 吃 %1%2")
                           .arg(target_piece.side == core::Side::Red ? "R" : "B")
                           .arg(target_piece.id);
            }

            add_history_log(log);
            update_ui();
            snapshot_scene();

            // 人类分支的唯一出口：走完可能轮到电脑。
            trigger_ai_if_needed();
            return;
        } catch (const std::exception& e) {
            // 走子被拒：若目标格是己方可动棋子，视为改选；否则清空选择并提示 e.what()。
            auto piece = state.board.at(pos);
            if (piece.side == state.player1_side) {
                auto selectable = core::rules::get_selectable_ids(state);
                bool is_valid_to_select = false;
                for (int id : selectable) {
                    if (id == piece.id) {
                        is_valid_to_select = true;
                        break;
                    }
                }

                if (is_valid_to_select) {
                    selected_pos_ = pos;
                    update_ui();
                    return;
                }
            }
            selected_pos_ = std::nullopt;
            update_ui();
            show_message(QStringLiteral("移动失败: ") + QString::fromUtf8(e.what()));
        }
    } else {
        // 未选中任何棋子：尝试选中点击格上的己方棋子。
        // 空格或敌方棋子静默忽略。
        auto piece = state.board.at(pos);
        if (piece.side == state.player1_side) {
            // 骰子决定可选编号（含降级），查表确认这枚棋子本回合能否动。
            auto selectable = core::rules::get_selectable_ids(state);
            bool is_valid_to_select = false;
            for (int id : selectable) {
                if (id == piece.id) {
                    is_valid_to_select = true;
                    break;
                }
            }

            if (is_valid_to_select) {
                selected_pos_ = pos;
                update_ui();
            } else {
                show_message("这个棋子本回合不能动");
            }
        }
    }
}

void GameWidget::trigger_ai_if_needed() {
    // 中止态下棋盘与按钮都不推进对局。
    if (game_aborted_) {
        return;
    }
    auto state = game_.get_state();

    // 已分胜负就不能再掷骰，否则对着 GameOver 掷骰会抛异常。
    if (state.phase == core::Phase::GameOver) {
        return;
    }
    if (state.current_side == state.player1_side) {
        return;
    }

    // 500ms 后再启动 AI，让人类走子后的棋盘与日志先画完。
    ai_timer_.start(after_move_delay_ms_);
}

GameWidget::PendingActions GameWidget::pause_action_timers() {
    PendingActions paused;
    // 先读 remainingTime() 再 stop()，停掉之后计时信息就没了；
    // 夹到 0 以防“已到期但事件尚未派发”时返回异常值。
    if (roll_timer_.isActive()) {
        paused.roll_remaining_ms = std::max(0, roll_timer_.remainingTime());
        roll_timer_.stop();
    }
    if (ai_timer_.isActive()) {
        paused.ai_remaining_ms = std::max(0, ai_timer_.remainingTime());
        ai_timer_.stop();
    }
    return paused;
}

void GameWidget::resume_action_timers(const PendingActions& paused) {
    // 只恢复暂停时真的在运行的定时器；nullopt 表示本来就没排动作。
    if (paused.roll_remaining_ms) {
        roll_timer_.start(*paused.roll_remaining_ms);
    }
    if (paused.ai_remaining_ms) {
        ai_timer_.start(*paused.ai_remaining_ms);
    }
}

void GameWidget::cancel_pending_actions() {
    // 两个定时器都停：未执行的掷骰与 AI 步进作废。
    roll_timer_.stop();
    ai_timer_.stop();
}

void GameWidget::abort_game(const QString& reason) {
    // 异常不能翻译成“过一手”去清骰/换边，否则掩盖真实缺陷；只冻结界面，保留原局面。
    cancel_pending_actions();
    game_aborted_ = true;
    abort_reason_ = reason;
    selected_pos_ = std::nullopt;
    update_ui();
}

void GameWidget::handle_ai_turn() {
    // 已中止或已返回主页：排队的回调直接返回，不推进对局。
    if (game_aborted_) {
        return;
    }
    // 回调期间局面可能已变（返回主页、重开、读档、已分胜负），所以每次重读局面，
    // 按“阶段 + 当前方”推进；读档直接落在 Player2Thinking 时也靠这里继续。
    auto state = game_.get_state();
    if (state.phase == core::Phase::GameOver || state.current_side == state.player1_side) {
        return;
    }

    switch (state.phase) {
        case core::Phase::LayOut:
        case core::Phase::Player1Thinking:
            return;
        case core::Phase::ThrowingDice:
            try {
                game_.roll_dice();
            } catch (const std::exception& e) {
                qDebug() << "电脑掷骰失败:" << e.what();
                abort_game(QStringLiteral("电脑掷骰失败: %1").arg(QString::fromUtf8(e.what())));
                return;
            }
            update_ui();
            snapshot_scene();
            // 掷骰后等 800ms 再要着法，让提示可见；重入时落到 Player2Thinking 分支。
            ai_timer_.start(think_delay_ms_);
            return;
        case core::Phase::Player2Thinking:
            // 骰子已就绪，落到下面走子。
            break;
        case core::Phase::GameOver:
            // 开头已判过，这里只为让 switch 覆盖全部枚举值。
            return;
    }

    // AI 或 core 抛异常说明局面/搜索有问题，不能当成“放弃回合”；中止本局并保留原局面。
    try {
        auto move = ai::decide_move(game_.get_state(), ai_level_);

        // 落子前算好日志（落子后读不到被吃的棋子），但先不写列表，避免失败时留下假记录。
        auto target_piece = game_.get_state().board.at(move.to);
        QString log = QStringLiteral("电脑: (%1,%2)->(%3,%4)")
                          .arg(move.from.x)
                          .arg(move.from.y)
                          .arg(move.to.x)
                          .arg(move.to.y);
        if (target_piece.side != core::Side::None) {
            log += QStringLiteral(" 吃 %1%2")
                       .arg(target_piece.side == core::Side::Red ? "R" : "B")
                       .arg(target_piece.id);
        }

        game_.make_move(move.from, move.to);
        add_history_log(log);
        update_ui();
        snapshot_scene();
    } catch (const std::exception& e) {
        qDebug() << "AI 异常:" << e.what();
        abort_game(QStringLiteral("电脑走子失败: %1").arg(QString::fromUtf8(e.what())));
        return;
    }

    // 所有分支在这里汇合，再检查一次是否仍该由电脑行动。
    trigger_ai_if_needed();
}

void GameWidget::update_ui() {
    auto state = game_.get_state();

    // 中止态：棋盘只显示、不接受点击；保存也禁用，异常局面不写档。
    board_widget_->setEnabled(!game_aborted_);
    save_btn_->setEnabled(!game_aborted_);

    // 只有人类可操作时才计算高亮，AI 回合喂空数组。
    std::vector<core::Pos> selectable_pos;
    std::vector<core::Pos> reachable_pos;

    if (!game_aborted_ && state.current_side == state.player1_side &&
        state.phase == core::Phase::Player1Thinking) {
        if (!selected_pos_) {
            // 未选中：标出本回合可动的棋子。
            selectable_pos = get_selectable_positions();
        } else {
            // 已选中：标出可达格。
            reachable_pos = get_reachable_positions(*selected_pos_);
        }
    }

    board_widget_->update_state(state, selected_pos_, selectable_pos, reachable_pos);
    dice_widget_->set_dice(state.current_dice);

    // 中止优先于一切：不显示回合文案，也不允许掷骰。
    if (game_aborted_) {
        status_label_->setText(
            QStringLiteral("⚠️ 对局异常：%1；请返回主页后重新开局或读档。").arg(abort_reason_));
        roll_btn_->setEnabled(false);
        return;
    }

    // 文案与按钮必须成对更新，否则会出现“请掷骰子”却按钮灰的矛盾界面。
    if (state.phase == core::Phase::GameOver) {
        // 终局：展示胜者并禁用掷骰后直接返回。
        status_label_->setText(QStringLiteral("🏆 %1 获胜")
                                   .arg(QString::fromStdString(core::side_name(state.winner))));
        roll_btn_->setEnabled(false);
        return;
    }

    // 思考阶段理论上必有骰子，但存档可能自相矛盾，用 "?" 兜底而不直接解引用
    // optional；成因见 `#save-unchecked-state`。
    const QString dice_text =
        state.current_dice ? QString::number(*state.current_dice) : QStringLiteral("?");

    if (state.current_side == state.player1_side) {
        if (state.phase == core::Phase::ThrowingDice) {
            // 全局唯一点亮掷骰按钮的组合。
            status_label_->setText("轮到你了，请掷骰子");
            roll_btn_->setEnabled(true);
        } else {
            status_label_->setText("🎲 你掷出了 " + dice_text + "，请走棋");
            roll_btn_->setEnabled(false);
        }
    } else {
        // 电脑回合：两个子阶段都由定时器推进，按钮保持禁用。
        if (state.phase == core::Phase::ThrowingDice) {
            status_label_->setText("🤖 电脑正在掷骰子...");
        } else {
            status_label_->setText("🎲 电脑掷出了 " + dice_text + "，正在思考...");
        }
        roll_btn_->setEnabled(false);
    }
}

std::vector<core::Pos> GameWidget::get_selectable_positions() const {
    std::vector<core::Pos> positions;
    auto state = game_.get_state();
    // LayOut / ThrowingDice 没有“可动棋子”的概念。
    if (state.phase != core::Phase::Player1Thinking) {
        return positions;
    }

    // 规则给编号、界面要坐标，逐枚反查；查不到说明已被吃或不属于人类，跳过即可。
    auto selectable_ids = core::rules::get_selectable_ids(state);
    for (int id : selectable_ids) {
        auto pos = state.board.find_piece(state.player1_side, id);
        if (pos) {
            positions.push_back(*pos);
        }
    }
    return positions;
}

std::vector<core::Pos> GameWidget::get_reachable_positions(core::Pos from) const {
    std::vector<core::Pos> positions;
    auto state = game_.get_state();

    // 25 格逐个试 check_move：UI 不复制方向或阶段规则；不拦终点占位，己方格也会高亮。
    // 本函数不校验阶段，由调用方保证。
    for (std::size_t x = 0; x < core::Board::kSize; ++x) {
        for (std::size_t y = 0; y < core::Board::kSize; ++y) {
            core::Pos to{x, y};
            if (core::rules::check_move(state, from, to).empty()) {
                positions.push_back(to);
            }
        }
    }
    return positions;
}

}  // namespace gui
