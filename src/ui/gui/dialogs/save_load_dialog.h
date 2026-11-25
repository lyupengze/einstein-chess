#ifndef SAVE_LOAD_DIALOG_H_
#define SAVE_LOAD_DIALOG_H_

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <string>

// 存档对话框（SaveLoadDialog）的接口声明。
// 一个类同时承担“保存”和“读取”两种形态，区别只在构造时传入的 DialogMode；
// 本类只收集“存档名”，真正的读写库由调用方（GameWidget / MainWindow）执行。
namespace ui {

/// @brief 对话框形态：Save 需要输入名称，Load 必须从列表里选一项。
enum class DialogMode { Save, Load };

/// @brief 存档对话框，按 DialogMode 切换成“保存”或“读取”两种界面。
///
/// 构造时就查一次数据库（list_saves），因此新存或删档后要重新打开才会看到变化。
/// Save 模式下列表只作参考，同名保存会先弹出覆盖确认；Load 模式下默认选中最近的一条。
class SaveLoadDialog : public QDialog {
    Q_OBJECT
public:
    /// @brief 按模式搭界面并加载已有存档列表。
    /// @param mode 保存还是读取
    /// @param parent 父控件，模态执行时以它为父窗口
    explicit SaveLoadDialog(DialogMode mode, QWidget* parent = nullptr);

    /// @brief 取出输入框里的存档名（已去掉首尾空白）。
    /// @return Save 模式的存档名；Load 模式没有输入框，调用它没有意义
    /// @note Load 模式下 name_input_ 不会被创建，也不在初始化列表里置空，
    ///       误调用会解引用未初始化指针。回调只用 get_selected_name()。
    std::string get_save_name() const;

    /// @brief 取出列表里当前选中的存档名。
    /// @return 选中的名字；没有选中任何一项时返回空串
    std::string get_selected_name() const;

private slots:
    /// @brief 确认按钮：先做“名称非空 / 必须选中一项 / 同名覆盖确认”的校验，通过才 accept()。
    void on_confirm_clicked();

private:
    DialogMode mode_;           ///< 形态标记，所有分支都靠它区分
    QListWidget* list_widget_;  ///< 已有存档名列表，内容来自 SaveManager::list_saves
    QLineEdit* name_input_;     ///< 仅 Save 模式创建的名称输入框
};

}  // namespace ui

#endif