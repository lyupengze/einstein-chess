#include "game_widget.h"

#include <QDebug>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QTimer>
#include <QVBoxLayout>
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

// 对局页实现：布局搭建、输入处理、界面刷新与 AI 回合时序都在这里。
// 规则判定一律委托给 core::Game 与 core::rules，本文件不重复实现方向、吃子与胜负逻辑。
// “掷骰 -> 思考 -> 走子”用 QTimer::singleShot 串成 400 / 500 / 800 ms 三段延迟，
// 目的只是让状态文案在界面上停留可见，规则本身不需要等待。
namespace ui {

GameWidget::GameWidget(QWidget* parent) : QWidget(parent) {
    // 主布局上下分层：顶层是状态文案与按钮排，下面是棋盘加右侧面板。
    auto* main_layout = new QVBoxLayout(this);
    auto* top_layout = new QHBoxLayout();
    auto* content_layout = new QHBoxLayout();
    auto* right_panel = new QVBoxLayout();

    status_label_ = new QLabel("等待开始游戏...", this);
    status_label_->setObjectName("statusLabel");  // 对应 style.qss 里的 QLabel#statusLabel

    // 三个按钮同尺寸，横向排在右上角。
    roll_btn_ = new QPushButton("掷骰子", this);
    exit_btn_ = new QPushButton("返回主页", this);
    save_btn_ = new QPushButton("保存", this);
    roll_btn_->setFixedSize(100, 40);
    exit_btn_->setFixedSize(100, 40);
    save_btn_->setFixedSize(100, 40);

    // 状态文案靠左、按钮靠右，中间用 stretch 吃掉空隙。
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
    history_list_->setFixedWidth(200);  // 固定侧栏宽度，剩余宽度全部给棋盘

    right_panel->addWidget(dice_widget_, 0, Qt::AlignHCenter);
    right_panel->addWidget(history_list_);
    content_layout->addWidget(board_widget_, 1);  // 拉伸因子 1：棋盘随窗口放大
    content_layout->addLayout(right_panel);
    main_layout->addLayout(content_layout, 1);

    connect(roll_btn_, &QPushButton::clicked, this, &GameWidget::on_roll_dice_clicked);
    connect(exit_btn_, &QPushButton::clicked, this, &GameWidget::on_exit_clicked);
    connect(save_btn_, &QPushButton::clicked, this, &GameWidget::on_save_clicked);
    connect(board_widget_, &BoardWidget::cell_clicked, this, &GameWidget::on_cell_clicked);

    // 进页时先禁用掷骰；真正的可用性每次都由 update_ui() 按阶段重新判定。
    roll_btn_->setEnabled(false);

    // Toast 最后创建并先隐藏：它是脱离布局的浮层，只在 show_message 时露出。
    toast_ = new ToastWidget(this);
    toast_->hide();
}

void GameWidget::start_new_game(core::Side human_side, const std::array<int, 6>& human_layout) {
    // prepare_new_game 会把 GameState 重置成空棋盘，所以同一个控件可以直接开下一局。
    game_.prepare_new_game(human_side);

    game_.submit_layout(human_side, human_layout);

    // AI 的布阵由 ai::decide_layout 随机生成；两次 submit 都到位后，
    // Game::check_both_ready 自动把阶段推进到 ThrowingDice，并把先手固定为红方。
    core::Side ai_side = (human_side == core::Side::Red) ? core::Side::Blue : core::Side::Red;
    game_.submit_layout(ai_side, ai::decide_layout());

    // 新局不保留上一局的选中格与历史记录，否则界面会残留旧信息。
    selected_pos_ = std::nullopt;

    history_list_->clear();

    update_ui();
    // 人类选蓝方时红方先手是电脑，这里必须让它自己开局。
    trigger_ai_if_needed();
}

void GameWidget::load_game_state(const core::GameState& state) {
    // load_game 是整体覆盖，不做增量合并；存档里的 player1_side 决定谁是“人类”。
    game_.load_game(state);
    selected_pos_ = std::nullopt;

    history_list_->clear();

    update_ui();
    // 存档可能正停在 AI 回合，所以载入后同样要检查一次是否该轮到电脑。
    trigger_ai_if_needed();
}

void GameWidget::on_exit_clicked() {
    // 只发信号，切页由 MainWindow 做；对局对象原样保留，返回主页不会清空 game_。
    emit exit_to_menu();
}

void GameWidget::on_roll_dice_clicked() {
    // 立刻禁用按钮，防止这段延迟里被连点第二次而触发重复掷骰。
    roll_btn_->setEnabled(false);
    status_label_->setText("🎲 正在掷骰子...");

    // 延迟 400ms 才真正掷骰：让“正在掷骰子”这行文案先渲染出来，
    // 之后再跳到点数，玩家才能看出这是两步而不是瞬间完成。
    // 定时器以 this 为 context，控件销毁时回调会自动取消。
    QTimer::singleShot(400, this, [this]() {
        try {
            game_.roll_dice();
            update_ui();
        } catch (const std::exception& e) {
            // 正常操作到不了这里（按钮已按阶段禁用），出错只记日志，不弹窗打断玩家。
            // 注意: 异常分支不恢复按钮，掷骰失败后界面会一直停在禁用态无法自行恢复。
            qDebug() << "掷骰子失败:" << e.what();
        }
    });
}

void GameWidget::on_save_clicked() {
    // 对话框只收集名字，写库放在调用方；路径是相对名 kDefaultDbPath，
    // 因为 main_gui.cpp 已把工作目录切到 exe 所在目录，存档就落在 exe 旁边。
    // 注意: 这一行在 try 之外，而对话框构造时就会打开数据库，失败时异常无人接住。
    SaveLoadDialog dialog(DialogMode::Save, this);
    if (dialog.exec() == QDialog::Accepted) {
        std::string name = dialog.get_save_name();
        try {
            // 每次新建而不是做成成员：SQLite 连接的生命周期就被限制在这个块里。
            storage::SaveManager save_manager(storage::kDefaultDbPath);
            save_manager.save_game(name, game_.get_state());
            show_message("游戏已保存！");
        } catch (const std::exception& e) {
            QMessageBox::critical(this, "保存失败", e.what());
        }
    }
}

void GameWidget::show_message(const QString& msg) {
    // 转发给浮层；显示时长、淡出等细节由 ToastWidget 自己管。
    toast_->show_message(msg);
}

void GameWidget::add_history_log(const QString& text) {
    // 追加而不是前插，列表顺序即发生顺序；滚到底保证最新一手始终可见。
    history_list_->addItem(text);
    history_list_->scrollToBottom();
}

void GameWidget::on_cell_clicked(core::Pos pos) {
    // 先取一份快照：后面所有分支都用它做只读判断，不必每步重新 get_state()。
    auto state = game_.get_state();
    // 只接受“人类这一方 + 轮到它走子”的点击。阶段名 Player1 指的是发起方（人类），
    // 不是红方，所以人类执蓝时这个条件同样成立；不在此时点击一律静默忽略。
    if (state.current_side != state.player1_side || state.phase != core::Phase::Player1Thinking) {
        return;
    }

    // 分支一：再次点击已选中的同一格 = 取消选中。
    if (selected_pos_ && *selected_pos_ == pos) {
        selected_pos_ = std::nullopt;
        update_ui();
        return;
    }

    // 分支二：已选中一枚棋子又点了别处，先当成“走子”尝试。
    if (selected_pos_) {
        try {
            // 必须在 make_move 之前把起点拷出来：成功后 selected_pos_ 会被清空，
            // 日志若再解引用 optional 就是空解引用（未定义行为）。
            const core::Pos from_pos = *selected_pos_;

            game_.make_move(*selected_pos_, pos);
            selected_pos_ = std::nullopt;

            // 用进函数时的快照读终点内容：此刻棋盘已经落子，game_.get_state() 里那格
            // 已是己方棋子，只有快照还留着被吃掉的棋子，日志才写得出“吃 R3”。
            auto target_piece = state.board.at(pos);
            QString log = QString("你: (%1,%2)->(%3,%4)")
                              .arg(from_pos.x)
                              .arg(from_pos.y)
                              .arg(pos.x)
                              .arg(pos.y);
            // 注意: 终点可能是己方棋子，规则允许吃自己的子，所以“吃”也会带己方编号。
            if (target_piece.side != core::Side::None) {
                log += QString(" 吃 %1%2")
                           .arg(target_piece.side == core::Side::Red ? "R" : "B")
                           .arg(target_piece.id);
            }

            add_history_log(log);
            update_ui();

            // 走完这一步可能就轮到电脑了；在这里统一触发，人类分支只有这一个出口。
            trigger_ai_if_needed();
            return;
        } catch (const std::exception& e) {
            // 走子被 rules 拒绝。此时再判断一次“目标格上是不是己方的可动棋子”：
            // 是的话说明玩家其实想改选另一枚，按换选处理，不弹错误；
            // 否则清空选择并提示失败原因（e.what() 是 core::rules 给出的中文文案）。
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
            show_message(QString("移动失败: ") + e.what());
        }
    } else {
        // 分支三：没有选中任何棋子，尝试选中点击格上的己方棋子。
        // 点到空格或敌方棋子直接静默返回，不打扰玩家。
        auto piece = state.board.at(pos);
        if (piece.side == state.player1_side) {
            // 不是所有己方棋子本回合都能动：骰子决定编号，get_selectable_ids 给出
            // “点数对应棋子，或左右最近的己方棋子”，这里只是查表确认。
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
                show_message("这个棋子本回合不能动！");
            }
        }
    }
}

void GameWidget::trigger_ai_if_needed() {
    auto state = game_.get_state();

    // 已分胜负就没人该动了，否则会对着 GameOver 再掷一次骰子而抛异常。
    if (state.phase == core::Phase::GameOver) {
        return;
    }
    // 人类回合什么都不做，等玩家点棋盘。
    if (state.current_side == state.player1_side) {
        return;
    }

    // 延迟 500ms 而不是直接调用：让人类走子后的棋盘刷新与日志先画完，
    // 否则“你走完 -> 电脑立刻掷骰”会挤在同一帧里，看不出先后关系。
    QTimer::singleShot(500, this, &GameWidget::handle_ai_turn);
}

void GameWidget::handle_ai_turn() {
    // 定时器回调期间局面可能已经变了（玩家返回主页、重开一局、或已分出胜负），
    // 所以进来先复查一次，条件不满足就放弃这次 AI 行动。
    auto state = game_.get_state();
    if (state.phase == core::Phase::GameOver || state.current_side == state.player1_side) {
        return;
    }

    // 注意: 只在 ThrowingDice 分支行动；若存档停在 Player2Thinking，对局会卡住无法推进。
    if (state.phase == core::Phase::ThrowingDice) {
        game_.roll_dice();
        update_ui();

        // 掷骰后再等 800ms 才要着法：让“电脑掷出了 X，正在思考...”有可见的停留时间，
        // 这正是三段延迟里最长的一段。
        QTimer::singleShot(800, this, [this]() {
            try {
                auto move = ai::decide_move(game_.get_state());

                // 先记日志再落子：落子后终点已是己方棋子，就读不到被吃掉的是谁了。
                // 注意: 这里读的是尚未落子的当前棋盘，与人类分支用快照的写法不同。
                auto target_piece = game_.get_state().board.at(move.to);
                QString log = QString("电脑: (%1,%2)->(%3,%4)")
                                  .arg(move.from.x)
                                  .arg(move.from.y)
                                  .arg(move.to.x)
                                  .arg(move.to.y);
                if (target_piece.side != core::Side::None) {
                    log += QString(" 吃 %1%2")
                               .arg(target_piece.side == core::Side::Red ? "R" : "B")
                               .arg(target_piece.id);
                }
                add_history_log(log);

                game_.make_move(move.from, move.to);
                update_ui();
            } catch (const std::exception& e) {
                // 注意: AI 抛异常时只记日志，阶段停在 Player2Thinking，界面不会自行恢复。
                qDebug() << "AI 异常:" << e.what();
            }
        });
    }
}

void GameWidget::update_ui() {
    auto state = game_.get_state();

    // 高亮数据只在“人类可操作”时才计算：AI 回合喂空数组，棋盘上就不会闪出可动圈。
    std::vector<core::Pos> selectable_pos;
    std::vector<core::Pos> reachable_pos;

    if (state.current_side == state.player1_side && state.phase == core::Phase::Player1Thinking) {
        if (!selected_pos_) {
            // 还没选：提示哪些棋子本回合能动。
            selectable_pos = get_selectable_positions();
        } else {
            // 已选中：提示这枚棋子能落到哪里。
            reachable_pos = get_reachable_positions(*selected_pos_);
        }
    }

    board_widget_->update_state(state, selected_pos_, selectable_pos, reachable_pos);
    dice_widget_->set_dice(state.current_dice);

    // 下面是一张“状态文案 + 掷骰按钮”的联动表，两者必须成对修改，
    // 否则会出现文案写着“请掷骰子”而按钮是灰的这种自相矛盾的界面。
    if (state.phase == core::Phase::GameOver) {
        // 终局：只展示胜者并禁用掷骰，然后直接返回，不再落到下面的回合分支。
        status_label_->setText(
            QString("🏆 %1 获胜！").arg(QString::fromStdString(core::side_name(state.winner))));
        roll_btn_->setEnabled(false);
        return;
    }

    if (state.current_side == state.player1_side) {
        if (state.phase == core::Phase::ThrowingDice) {
            // 人类回合且尚未掷骰：这是全局唯一允许点亮掷骰按钮的组合。
            status_label_->setText("轮到你了，请掷骰子");
            roll_btn_->setEnabled(true);
        } else {
            // 人类回合且已掷骰：展示点数，按钮保持禁用。
            // 注意: 直接解引用 current_dice，若在 LayOut 阶段被调用会解引用空 optional。
            status_label_->setText("🎲 你掷出了 " + QString::number(*state.current_dice) +
                                   "，请走棋");
            roll_btn_->setEnabled(false);
        }
    } else {
        // 电脑回合：掷骰与思考都由定时器推进，无论哪个子阶段按钮都禁用。
        if (state.phase == core::Phase::ThrowingDice) {
            status_label_->setText("🤖 电脑正在掷骰子...");
        } else {
            status_label_->setText("🎲 电脑掷出了 " + QString::number(*state.current_dice) +
                                   "，正在思考...");
        }
        roll_btn_->setEnabled(false);
    }
}

std::vector<core::Pos> GameWidget::get_selectable_positions() const {
    std::vector<core::Pos> positions;
    auto state = game_.get_state();
    // LayOut / ThrowingDice 阶段没有“可动棋子”的概念，直接返回空表。
    if (state.phase != core::Phase::Player1Thinking) {
        return positions;
    }

    // 规则给的是编号，棋盘要的是坐标，所以逐枚反查；查不到说明那枚棋子已被吃掉
    // （或该 id 本来就不属于人类），跳过即可，不必当成错误。
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

    // 25 格全试一遍，用 check_move 返回空串当“合法”判据：这样 UI 不必复制任何
    // 方向或阶段规则，规则以后改了这里自动跟着变。本函数不校验阶段，由 update_ui 保证。
    // 注意: check_move 不拦终点占位，己方棋子所在格也会被高亮，这是规则允许的。
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

}  // namespace ui