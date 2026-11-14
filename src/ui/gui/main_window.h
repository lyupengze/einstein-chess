#ifndef MAIN_WINDOW_H_
#define MAIN_WINDOW_H_

#include <QLabel>
#include <QPushButton>
#include <QWidget>
#include <vector>

#include "board_widget.h"
#include "core/game.h"
#include "core/types.h"
#include "dice_widget.h"

namespace ui {

class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void on_start_clicked();
    void on_roll_dice_clicked();
    void on_cell_clicked(core::Pos pos);

private:
    void update_ui();
    void handle_ai_turn();
    void show_message(const QString& msg);
    std::vector<core::Pos> get_selectable_positions() const;
    std::vector<core::Pos> get_reachable_positions(core::Pos from) const;

    core::Game game_;
    BoardWidget* board_widget_;
    DiceWidget* dice_widget_;
    QLabel* status_label_;
    QLabel* message_label_;
    QPushButton* start_btn_;
    QPushButton* roll_btn_;
    std::optional<core::Pos> selected_pos_;
};

}  // namespace ui

#endif