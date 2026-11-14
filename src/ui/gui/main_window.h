#ifndef MAIN_WINDOW_H_
#define MAIN_WINDOW_H_

#include <QLabel>
#include <QPushButton>
#include <QWidget>

#include "board_widget.h"  // 引入你的棋盘组件
#include "core/game.h"

namespace ui {

class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    core::Game game_;
    BoardWidget* board_widget_;  // 持有组件的指针
    QLabel* status_label_;
};

}  // namespace ui

#endif