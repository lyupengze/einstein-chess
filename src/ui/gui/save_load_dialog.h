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

    std::string get_save_name() const;

    std::string get_selected_name() const;

private slots:
    void on_confirm_clicked();

private:
    DialogMode mode_;
    QListWidget* list_widget_;
    QLineEdit* name_input_;
};

}  // namespace ui

#endif