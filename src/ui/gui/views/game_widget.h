#ifndef GAME_WIDGET_H_
#define GAME_WIDGET_H_

#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QWidget>
#include <optional>
#include <vector>

#include "components/board_widget.h"
#include "components/dice_widget.h"
#include "components/toast_widget.h"
#include "core/game.h"

namespace ui {

class GameWidget : public QWidget {
    Q_OBJECT
public:
    explicit GameWidget(QWidget* parent = nullptr);

    enum class MessageType { Success, Error };

    void start_new_game(core::Side human_side, const std::array<int, 6>& human_layout);
    void load_game_state(const core::GameState& state);

signals:

    void exit_to_menu();

private slots:
    void on_roll_dice_clicked();
    void on_cell_clicked(core::Pos pos);
    void on_exit_clicked();
    void on_save_clicked();

private:
    void update_ui();
    void handle_ai_turn();
    void show_message(const QString& msg);
    void trigger_ai_if_needed();
    void add_history_log(const QString& text);

    std::vector<core::Pos> get_selectable_positions() const;
    std::vector<core::Pos> get_reachable_positions(core::Pos from) const;

    core::Game game_;
    BoardWidget* board_widget_;
    DiceWidget* dice_widget_;
    QLabel* status_label_;
    ToastWidget* toast_;
    QPushButton* roll_btn_;
    QPushButton* exit_btn_;
    QPushButton* save_btn_;
    QListWidget* history_list_;
    std::optional<core::Pos> selected_pos_;
};

}  // namespace ui

#endif