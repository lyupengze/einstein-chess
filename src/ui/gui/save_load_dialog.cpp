#include "save_load_dialog.h"

#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

#include "storage/save_manager.h"

namespace ui {

SaveLoadDialog::SaveLoadDialog(DialogMode mode, QWidget* parent) : QDialog(parent), mode_(mode) {
    setWindowTitle(mode == DialogMode::Save ? "保存游戏" : "读取存档");
    setFixedSize(400, 300);

    auto* layout = new QVBoxLayout(this);

    storage::SaveManager save_manager("einstein_save.db");
    auto saves = save_manager.list_saves();

    if (mode_ == DialogMode::Save) {
        layout->addWidget(new QLabel("请输入存档名称："));
        name_input_ = new QLineEdit(this);
        name_input_->setPlaceholderText("例如：绝地反击");
        layout->addWidget(name_input_);

        layout->addWidget(new QLabel("已有存档（同名将覆盖）："));
    } else {
        if (saves.empty()) {
            layout->addWidget(new QLabel("当前没有任何存档。"));
        } else {
            layout->addWidget(new QLabel("请选择要读取的存档："));
        }
    }

    list_widget_ = new QListWidget(this);
    for (const auto& name : saves) {
        list_widget_->addItem(QString::fromStdString(name));
    }
    layout->addWidget(list_widget_);

    auto* confirm_btn = new QPushButton("确认", this);
    confirm_btn->setStyleSheet("background-color: #3498db; color: white; padding: 8px;");
    layout->addWidget(confirm_btn);

    connect(confirm_btn, &QPushButton::clicked, this, &SaveLoadDialog::on_confirm_clicked);

    if (mode_ == DialogMode::Load && !saves.empty()) {
        list_widget_->setCurrentRow(0);
    }
}

void SaveLoadDialog::on_confirm_clicked() {
    if (mode_ == DialogMode::Save) {
        if (name_input_->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "错误", "存档名不能为空！");
            return;
        }
        accept();
    } else {
        if (!list_widget_->currentItem()) {
            QMessageBox::warning(this, "错误", "请选择一个存档！");
            return;
        }
        accept();
    }
}

std::string SaveLoadDialog::get_save_name() const {
    return name_input_->text().trimmed().toStdString();
}

std::string SaveLoadDialog::get_selected_name() const {
    if (list_widget_->currentItem()) {
        return list_widget_->currentItem()->text().toStdString();
    }
    return "";
}

}  // namespace ui