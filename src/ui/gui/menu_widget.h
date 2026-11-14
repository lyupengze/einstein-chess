#ifndef MENU_WIDGET_H_
#define MENU_WIDGET_H_

#include <QPushButton>
#include <QWidget>

namespace ui {

class MenuWidget : public QWidget {
    Q_OBJECT
public:
    explicit MenuWidget(QWidget* parent = nullptr);

signals:
    void start_game_clicked();
    void load_game_clicked();

private:
    QPushButton* start_btn_;
    QPushButton* load_btn_;
};

}  // namespace ui

#endif