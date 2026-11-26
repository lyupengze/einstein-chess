/// @file src/ui/gui/components/dice_widget.h
/// @brief 骰子图形控件：把本回合点数画成一颗带点阵的骰子。

#pragma once

#include <QWidget>
#include <optional>

namespace gui {

/// @brief 骰子图形控件，尺寸被限制在 100~150 像素之间，由右侧面板水平居中摆放。
///
/// 随机数由 `core::Game` 产生，本控件只接收 1~6 或 `std::nullopt`：无点数时
/// 整帧不画，非法点数画红色问号，两者在视觉上可以区分。
///
/// 无输入、无交互：唯一的外部接口是 set_dice()，点阵布局在 paintEvent 里按点数分支绘制。
class DiceWidget : public QWidget {
    Q_OBJECT
public:
    /// @brief 构造控件；只设定尺寸范围，不显示任何点数。
    explicit DiceWidget(QWidget* parent = nullptr);

    /// @brief 设置要显示的点数并请求重绘。
    /// @param dice 1~6 的合法点数；std::nullopt 表示"本回合还没掷骰"，控件随即变空白
    /// @note 此处不做范围校验：传入 0 或大于 6 时骰身照画，点阵画成红色问号。
    void set_dice(std::optional<int> dice);

protected:
    /// @brief 按 current_dice_ 画骰身与点阵；无点数时什么都不画。
    void paintEvent(QPaintEvent* event) override;

private:
    std::optional<int> current_dice_;  ///< 当前点数 1~6；nullopt 表示本回合尚未掷骰
};

}  // namespace gui
