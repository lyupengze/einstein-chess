#ifndef SAVE_LOAD_DIALOG_H_
#define SAVE_LOAD_DIALOG_H_

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <optional>
#include <string>

namespace ui {

enum class DialogMode { Save, Load };

class SaveLoadDialog : public QDialog {
    Q_OBJECT
public:
    explicit SaveLoadDialog(DialogMode mode, QWidget* parent = nullptr);

    // 如果是 Save 模式，获取用户输入的存档名
    std::string get_save_name() const;
    // 如果是 Load 模式，获取选中的存档名
    std::string get_selected_name() const;

private slots:
    void on_confirm_clicked();

private:
    DialogMode mode_;
    QListWidget* list_widget_;
    QLineEdit* name_input_;  // 仅在 Save 模式下显示
};

}  // namespace ui

#endif