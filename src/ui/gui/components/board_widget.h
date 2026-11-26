#ifndef BOARD_WIDGET_H_
#define BOARD_WIDGET_H_

#include <QMouseEvent>
#include <QWidget>
#include <optional>
#include <vector>

#include "core/game_state.h"
#include "core/types.h"

namespace ui {

// 棋盘控件的对外接口：只声明"怎样被喂状态"和"怎样把点击抛出去"，不暴露任何规则 API。
// 绘制与换算细节全部留在 .cpp，头部刻意保持精简，以体现它是一个纯展示组件。
// 状态按整份快照传入：每次刷新都覆盖全部四项，控件不保留任何跨帧的中间状态。

/// @brief 棋盘绘制控件：纯展示组件，所有状态由 update_state() 从外部灌入。
///
/// 职责边界：
/// - 画：棋盘格 → 红/蓝初始区 → 网格线 → 棋子 → 三种高亮圈（顺序见 .cpp）；
/// - 收：把鼠标点击换算成棋盘坐标，通过 cell_clicked 抛给 GameWidget；
/// - 不判断：轮次、走法是否合法、目标格有没有子，一概不看，给什么高亮就画什么。
///
/// @note 坐标语义沿用 core::Pos：x 是行号（向下增大）、y 是列号（向右增大）；
///       绘制按矩阵习惯处理：x 落到屏幕纵向、y 落到屏幕横向，不做转置；
///       paintEvent 与 mousePressEvent 各做一次互逆的换算，改动其中一处必须同步另一处。
class BoardWidget : public QWidget {
    Q_OBJECT
public:
    /// @brief 构造控件，最小尺寸 400x400；不初始化任何棋子，等待首次 update_state()。
    explicit BoardWidget(QWidget* parent = nullptr);

    /// @brief 用一整份对局快照加三组高亮信息刷新界面，内部只做保存并触发重绘。
    /// @param state 当前对局快照，按值复制进成员，调用方之后改动不影响本控件
    /// @param selected_pos 当前选中的棋子坐标；std::nullopt 表示没有选中任何棋子
    /// @param selectable_pieces 本回合规则允许挑选的棋子位置，画成绿色虚线整圈
    /// @param reachable_targets 选中棋子的可达落点，画成绿色虚线圆圈；可含己方棋子所在格
    /// @note 三个列表与 state 之间不做任何一致性校验，传进来什么就原样画出来。
    void update_state(const core::GameState& state, std::optional<core::Pos> selected_pos,
                      const std::vector<core::Pos>& selectable_pieces,
                      const std::vector<core::Pos>& reachable_targets);

signals:
    /// @brief 棋盘被点击时发出，携带已换算好的棋盘坐标。
    /// @note 只保证坐标落在棋盘内；是否轮到自己、该格有无棋子，全部由接收方判断。
    void cell_clicked(core::Pos pos);

protected:
    /// @brief 分层绘制整个棋盘，层序见实现文件开头说明。
    void paintEvent(QPaintEvent* event) override;

    /// @brief 把点击位置换算成棋盘坐标并发出 cell_clicked；点在棋盘外则静默忽略。
    void mousePressEvent(QMouseEvent* event) override;

private:
    core::GameState current_state_;          ///< 最近一次 update_state 的整份快照，未刷新前是空盘
    std::optional<core::Pos> selected_pos_;  ///< 当前选中的棋子，只用于画金色实线框
    std::vector<core::Pos> selectable_pieces_;  ///< 本回合可选棋子位置，绿色虚线整圈
    std::vector<core::Pos> reachable_targets_;  ///< 当前选中棋子的可达落点，绿色虚线圆圈
};

}  // namespace ui

#endif  // BOARD_WIDGET_H_