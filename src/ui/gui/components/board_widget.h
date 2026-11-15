#ifndef BOARD_WIDGET_H_
#define BOARD_WIDGET_H_

#include <QMouseEvent>
#include <QWidget>
#include <optional>
#include <vector>

#include "core/game_state.h"
#include "core/types.h"

namespace ui {

class BoardWidget : public QWidget {
    Q_OBJECT
public:
    explicit BoardWidget(QWidget* parent = nullptr);

    void update_state(const core::GameState& state, std::optional<core::Pos> selected_pos,
                      const std::vector<core::Pos>& selectable_pieces,
                      const std::vector<core::Pos>& reachable_targets);

signals:
    void cell_clicked(core::Pos pos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    core::GameState current_state_;
    std::optional<core::Pos> selected_pos_;
    std::vector<core::Pos> selectable_pieces_;
    std::vector<core::Pos> reachable_targets_;
};

}  // namespace ui

#endif