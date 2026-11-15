#ifndef MAIN_WINDOW_H_
#define MAIN_WINDOW_H_

#include <QStackedWidget>
#include <QWidget>

#include "views/game_widget.h"
#include "views/menu_widget.h"

namespace ui {

class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    QStackedWidget* stacked_widget_;
    MenuWidget* menu_widget_;
    GameWidget* game_widget_;
};

}  // namespace ui

#endif