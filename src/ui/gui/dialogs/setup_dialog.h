/// @file src/ui/gui/dialogs/setup_dialog.h
/// @brief 布阵对话框：选阵营、选电脑档位、把 1~6 摆进己方三角区。

#pragma once

#include <QComboBox>
#include <QDialog>
#include <QPushButton>
#include <QRadioButton>
#include <array>

#include "ai/ai.h"

namespace gui {

/// @brief 布阵阶段对话框：选阵营 + 选电脑强度 + 点击己方三角区循环切换棋子编号。
///
/// 5x5 按钮网格与棋盘同构：行下标 x 即 Pos.x（向下增大），列下标 y 即 Pos.y（向右增大）。
/// 只有红/蓝三角区内的格子可点，其余格子禁用且显示为空。
class SetupDialog : public QDialog {
    Q_OBJECT
public:
    /// @brief 搭 5x5 网格、阵营单选、档位下拉框与确认按钮，并把阵营重置成红方。
    /// @param parent 父控件，模态执行时以它为父窗口
    explicit SetupDialog(QWidget* parent = nullptr);

    /// @brief 人类选择的阵营。
    /// @return 1 表示红方（先手 / 左上），2 表示蓝方（后手 / 右下）
    /// @note 1/2 只是本对话框的约定，MainWindow 会按 1、2 显式映射到 core::Side，
    ///       不依赖 Side 的枚举数值。
    int get_selected_side() const;
    /// @brief 六个编号的布阵结果。
    /// @return 下标 0~5 与 Board::setup_custom 的落位顺序一一对应
    /// @note 只有 accept() 之后取才有意义；对话框已保证六个编号互不重复且在 1~6。
    std::array<int, 6> get_layout() const;
    /// @brief 玩家选择的电脑档位（界面文字称“强度”）。
    /// @return 下拉框当前项对应的 ai::Level；初值为 ai::kDefaultLevel
    ai::Level get_selected_level() const;

private slots:
    /// @brief 确认按钮：先查重复编号，全部通过才 accept()。
    void on_confirm_clicked();
    /// @brief 阵营切换：清空已摆好的编号并重画网格。
    void on_side_changed();
    /// @brief 点击网格中的一格，把该格编号按 1->2->…->6->1 循环。
    /// @param x 行号，对应 Pos.x
    /// @param y 列号，对应 Pos.y
    void on_grid_clicked(int x, int y);

private:
    QRadioButton* red_radio_;   ///< 红方（先手 / 左上），默认选中
    QRadioButton* blue_radio_;  ///< 蓝方（后手 / 右下）
    QComboBox* level_combo_;    ///< 电脑档位，条目由 ai::kAllLevels 生成

    QPushButton* grid_btns_[5][5];  ///< 网格按钮，下标即 x、y，与 Pos 同序
    std::array<int, 6> layout_;     ///< 布阵结果；0 表示“该格尚未分配编号”

    /// @brief 按当前阵营重画 5x5 网格：活动格着色并显示编号，其余格禁用清空。
    void update_grid_ui();
    /// @brief 判断某格是否属于红方三角区（左上角 3+2+1 格）。
    bool is_red_region(int x, int y) const;
    /// @brief 判断某格是否属于蓝方三角区（右下角 3+2+1 格）。
    bool is_blue_region(int x, int y) const;
    /// @brief 把坐标换算成己方三角区内的 0~5 下标。
    /// @return 落在三角区内返回 0~5，否则返回 -1（调用方须自行过滤）
    int get_index_in_region(int x, int y) const;
};

}  // namespace gui
