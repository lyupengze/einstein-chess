#ifndef SETUP_DIALOG_H_
#define SETUP_DIALOG_H_

#include <QDialog>
#include <QGridLayout>
#include <QPushButton>
#include <QRadioButton>
#include <array>
#include <optional>

namespace ui {

class SetupDialog : public QDialog {
    Q_OBJECT
public:
    explicit SetupDialog(QWidget* parent = nullptr);

    int get_selected_side() const;
    std::array<int, 6> get_layout() const;

private slots:
    void on_confirm_clicked();
    void on_side_changed();
    void on_grid_clicked(int x, int y);

private:
    QRadioButton* red_radio_;
    QRadioButton* blue_radio_;

    // 5x5 的微型棋盘按钮
    QPushButton* grid_btns_[5][5];
    std::array<int, 6> layout_;  // 记录按顺序填入的 6 个数字

    void update_grid_ui();
    bool is_red_region(int x, int y) const;
    bool is_blue_region(int x, int y) const;
    int get_index_in_region(int x, int y) const;
};

}  // namespace ui

#endif