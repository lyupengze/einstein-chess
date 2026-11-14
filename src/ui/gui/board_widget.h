#ifndef BOARD_WIDGET_H_
#define BOARD_WIDGET_H_

#include <QWidget>

#include "core/game_state.h"

namespace ui {

class BoardWidget : public QWidget {
    Q_OBJECT
public:
    explicit BoardWidget(QWidget* parent = nullptr);

    void update_state(const core::GameState& state);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    core::GameState current_state_;
};

}  // namespace ui

#endif