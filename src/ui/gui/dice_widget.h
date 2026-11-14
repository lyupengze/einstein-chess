#ifndef DICE_WIDGET_H_
#define DICE_WIDGET_H_

#include <QWidget>
#include <optional>

namespace ui {

class DiceWidget : public QWidget {
    Q_OBJECT
public:
    explicit DiceWidget(QWidget* parent = nullptr);

    // 接收最新的骰子点数
    void set_dice(std::optional<int> dice);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::optional<int> current_dice_;
};

}  // namespace ui

#endif