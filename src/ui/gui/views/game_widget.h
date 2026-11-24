#ifndef GAME_WIDGET_H_
#define GAME_WIDGET_H_

#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>
#include <QWidget>
#include <array>
#include <optional>
#include <vector>

#include "ai/ai.h"
#include "components/board_widget.h"
#include "components/dice_widget.h"
#include "components/toast_widget.h"
#include "core/game.h"

// 对局页（GameWidget）的接口声明：棋盘、骰子、状态栏、操作按钮与走子历史。
// 类内持有唯一的 core::Game 实例，规则判定全部转交 core::Game 与 core::rules；
// 本类只负责采集输入、把 GameState 翻译成界面、以及驱动 AI 回合的定时器。
namespace ui {

/// @brief 对局主界面：左侧棋盘，右侧骰子与历史记录，顶部状态文案与三个操作按钮。
///
/// 人类固定占用 GameState::player1_side，另一方由 ai::decide_move 接管。
/// 判断“轮到谁”一律写 state.current_side == state.player1_side，不把红蓝写死，
/// 这样人类选蓝方时全部分支依然成立。
class GameWidget : public QWidget {
    Q_OBJECT
public:
    /// @brief 只搭控件与信号连接；对局本身要等 start_new_game / load_game_state。
    /// @param parent 父控件，生命周期交给 Qt 对象树
    explicit GameWidget(QWidget* parent = nullptr);

    /// @brief 开新局：重置对局状态，提交双方布阵后刷新界面。
    /// @param human_side 人类的阵营，另一阵营自动交给 AI
    /// @param human_layout 人类布阵，下标 0~5 对应 Board::setup_custom 的落位顺序
    /// @param ai_level 本局电脑强度，来自布阵对话框；整局保持不变
    void start_new_game(core::Side human_side, const std::array<int, 6>& human_layout,
                        ai::Level ai_level);
    /// @brief 用存档快照覆盖当前对局；若快照停在 AI 回合会立刻把回合交给 AI。
    /// @param state 从 SaveManager 读出的完整状态
    /// @param ai_level 存档记录的电脑强度，读档后整局沿用，保证与存档时一致
    void load_game_state(const core::GameState& state, ai::Level ai_level);

signals:

    /// @brief 用户点了“返回主页”；MainWindow 据此把栈切回菜单页。
    void exit_to_menu();

private slots:
    /// @brief 掷骰按钮：先禁用按钮并改文案，400ms 后才真正调用 game_.roll_dice()。
    void on_roll_dice_clicked();
    /// @brief roll_timer_ 的超时回调：真正执行掷骰，失败时恢复界面并提示。
    void on_roll_dice_timeout();
    /// @brief 棋盘点击的唯一入口，含取消选中、走子、换选、选中四个动作。
    /// @param pos 被点击的格；Pos.x 是行号，Pos.y 是列号
    void on_cell_clicked(core::Pos pos);
    /// @brief 返回主页按钮，只发 exit_to_menu，不动对局状态。
    void on_exit_clicked();
    /// @brief 保存按钮：弹保存对话框，确认后写库到 storage::kDefaultDbPath。
    void on_save_clicked();

private:
    /// @brief 按当前 GameState 统一刷新棋盘高亮、骰子、状态文案与掷骰按钮可用性。
    /// @note 状态文案与按钮可用性是一组联动，改一边必须同步改另一边。
    void update_ui();
    /// @brief 取消所有尚未执行的延迟动作：掷骰 400ms、AI 500/800ms。
    /// @note 开新局与读档必须先调用它，否则上一局排队的回调会作用到新局上。
    void cancel_pending_actions();
    /// @brief 定时器回调：AI 回合先掷骰，800ms 后再向 ai::decide_move 要着法。
    void handle_ai_turn();
    /// @brief 用悬浮 Toast 提示一句，到期自动消失。
    /// @param msg 提示正文
    void show_message(const QString& msg);
    /// @brief 若当前不是人类回合且未分胜负，就安排一次延迟的 handle_ai_turn()。
    void trigger_ai_if_needed();
    /// @brief 向历史列表追加一行并滚动到底部。
    /// @param text 形如“你: (0,1)->(1,1) 吃 B3”的一行文本
    void add_history_log(const QString& text);

    /// @brief 本回合允许移动的己方棋子所在格，供棋盘画可动圈。
    /// @return 阶段不是 Player1Thinking 时返回空
    std::vector<core::Pos> get_selectable_positions() const;
    /// @brief 选中某枚棋子后它能落到的全部格子，供棋盘画可达圈。
    /// @param from 起点，调用方保证是当前可动棋子之一
    /// @return 由 core::rules::check_move 逐格试出的结果，含己方棋子所在格
    std::vector<core::Pos> get_reachable_positions(core::Pos from) const;

    core::Game game_;                        ///< 唯一的对局对象，所有规则进出的门
    ai::Level ai_level_ = ai::kDefaultLevel;  ///< 本局电脑强度；开新局/读档时写入，之后不再变
    BoardWidget* board_widget_;              ///< 棋盘绘制者，也是 cell_clicked 的来源
    DiceWidget* dice_widget_;                ///< 骰子点数展示，nullopt 表示未掷
    QLabel* status_label_;                   ///< 顶部状态文案
    ToastWidget* toast_;                     ///< 悬浮提示，构造末尾创建并先隐藏
    QPushButton* roll_btn_;                  ///< 掷骰子，可用性完全由 update_ui() 决定
    QPushButton* exit_btn_;                  ///< 返回主页，任何时候都可用
    QPushButton* save_btn_;                  ///< 保存，任何时候都可用
    QListWidget* history_list_;              ///< 走子历史，按发生顺序追加
    std::optional<core::Pos> selected_pos_;  ///< 人类已选起点；nullopt 表示未选

    /// 延迟动作全部用成员 QTimer 而不是 QTimer::singleShot：
    /// singleShot 没有句柄，开了新局或读了档也取消不掉，旧回调会落到新局上。
    QTimer roll_timer_;  ///< 人类掷骰的 400ms 延迟，单发
    QTimer ai_timer_;    ///< AI 回合的 500/800ms 延迟，单发；两次启动共用同一支定时器
};

}  // namespace ui

#endif