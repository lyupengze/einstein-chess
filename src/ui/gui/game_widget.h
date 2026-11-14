#ifndef GAME_WIDGET_H_
#define GAME_WIDGET_H_

#include <QLabel>
#include <QPushButton>
#include <QWidget>
#include <optional>
#include <vector>

#include "board_widget.h"
#include "core/game.h"
#include "dice_widget.h"

namespace ui {

class GameWidget : public QWidget {
    Q_OBJECT
public:
    explicit GameWidget(QWidget* parent = nullptr);

    // 供 MainWindow 调用来开局
    void start_new_game(core::Side human_side, const std::array<int, 6>& human_layout);
    void load_game_state(const core::GameState& state);

signals:
    // 告诉 MainWindow 我要回菜单
    void exit_to_menu();

private slots:
    void on_roll_dice_clicked();
    void on_cell_clicked(core::Pos pos);
    void on_exit_clicked();  // 返回主页按钮
    void on_save_clicked();  // 新增

private:
    void update_ui();
    void handle_ai_turn();
    void show_message(const QString& msg);
    void trigger_ai_if_needed();  // 新增

    std::vector<core::Pos> get_selectable_positions() const;
    std::vector<core::Pos> get_reachable_positions(core::Pos from) const;

    core::Game game_;
    BoardWidget* board_widget_;
    DiceWidget* dice_widget_;
    QLabel* status_label_;
    QLabel* message_label_;
    QPushButton* roll_btn_;
    QPushButton* exit_btn_;
    QPushButton* save_btn_;  // 新增
    std::optional<core::Pos> selected_pos_;
};

}  // namespace ui

#endif