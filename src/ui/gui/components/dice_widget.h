#pragma once

#include <QWidget>
#include <optional>

namespace ui {

// 骰子点数展示控件：把"本回合掷出的点数"画成一颗带点阵的白色骰子。
// 纯展示组件——随机数由 core::Game 产生，这里只负责接收 1~6 或 nullopt 并画出对应点阵。
// 点数为空（尚未掷骰）时 paintEvent 直接返回，界面上看不到骰子，只有右侧面板的空白区域。

/// @brief 骰子图形控件，尺寸被限制在 100~150 像素之间，由右侧面板水平居中摆放。
///
/// 无输入、无交互：唯一的外部接口是 set_dice()，点阵布局在 paintEvent 里按点数分支绘制。
class DiceWidget : public QWidget {
    Q_OBJECT
public:
    /// @brief 构造控件；只设定尺寸范围，不显示任何点数。
    explicit DiceWidget(QWidget* parent = nullptr);

    /// @brief 设置要显示的点数并请求重绘。
    /// @param dice 1~6 的合法点数；std::nullopt 表示"本回合还没掷骰"，控件随即变空白
    /// @note 此处不做范围校验：传入 0 或大于 6 时骰身照画，但点阵一个都不会出现（空白骰面）。
    void set_dice(std::optional<int> dice);

protected:
    /// @brief 按 current_dice_ 画骰身与点阵；无点数时什么都不画。
    void paintEvent(QPaintEvent* event) override;

private:
    std::optional<int> current_dice_;  ///< 当前点数 1~6；nullopt 表示本回合尚未掷骰
};

}  // namespace ui
