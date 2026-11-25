#include "save_load_dialog.h"

#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <stdexcept>
#include <string>

#include "storage/save_manager.h"

// 存档对话框实现：两种形态共用一套控件，只在构造时按 mode_ 分支。
// 打开即查库（list_saves），所以新存或删除后必须重新打开才能看到最新列表。
// 校验集中在 on_confirm_clicked：名称非空 / 必须选中一项 / 同名覆盖需二次确认，
// 不满足就留在对话框里；只有全部通过才 accept()。
namespace ui {

SaveLoadDialog::SaveLoadDialog(DialogMode mode, QWidget* parent) : QDialog(parent), mode_(mode) {
    // 标题与下方提示语都随模式切换，其余控件复用。
    setWindowTitle(mode == DialogMode::Save ? "保存游戏" : "读取存档");
    setFixedSize(400, 300);

    auto* layout = new QVBoxLayout(this);

    // 临时对象只为拿一次列表；SaveManager 析构时数据库连接随之关闭。
    // 注意: 这里没有任何 try，数据库打不开时异常会直接从构造函数抛给调用方。
    storage::SaveManager save_manager(storage::kDefaultDbPath);
    auto saves = save_manager.list_saves();

    if (mode_ == DialogMode::Save) {
        // 保存模式：名称输入框在上，列表在下，列表只作参考。
        layout->addWidget(new QLabel("请输入存档名称："));
        name_input_ = new QLineEdit(this);
        name_input_->setPlaceholderText("例如：绝地反击");
        layout->addWidget(name_input_);

        // 明确告知同名会被覆盖，避免玩家以为产生了第二个副本。
        layout->addWidget(new QLabel("已有存档（同名将覆盖）："));
    } else {
        // 读取模式：没有输入框，空库与非空库给不同提示语，避免出现空白列表让人困惑。
        if (saves.empty()) {
            layout->addWidget(new QLabel("当前没有任何存档。"));
        } else {
            layout->addWidget(new QLabel("请选择要读取的存档："));
        }
    }

    // 列表内容原样来自数据库，顺序是最近保存在前（list_saves 按时间倒序）。
    list_widget_ = new QListWidget(this);
    for (const auto& name : saves) {
        list_widget_->addItem(QString::fromStdString(name));
    }
    layout->addWidget(list_widget_);

    auto* confirm_btn = new QPushButton("确认", this);
    confirm_btn->setStyleSheet("background-color: #3498db; color: white; padding: 8px;");
    layout->addWidget(confirm_btn);

    connect(confirm_btn, &QPushButton::clicked, this, &SaveLoadDialog::on_confirm_clicked);

    // 读取模式预选第一行（即最近一条），省掉一次点击；空库时保持无选中。
    if (mode_ == DialogMode::Load && !saves.empty()) {
        list_widget_->setCurrentRow(0);
    }
}

void SaveLoadDialog::on_confirm_clicked() {
    // 两种模式的合格条件不同，但处理方式一致：不合格就 warning 并 return，
    // 合格才 accept()，让 exec() 返回 QDialog::Accepted 交给调用方继续。
    if (mode_ == DialogMode::Save) {
        const QString name = name_input_->text().trimmed();
        if (name.isEmpty()) {
            QMessageBox::warning(this, "错误", "存档名不能为空！");
            return;
        }

        // 同名保存最终由 SaveManager 的 INSERT OR REPLACE 覆盖；覆盖会丢失旧档，
        // 所以先让用户确认一次，默认按钮选 No，避免回车误覆盖。
        if (!list_widget_->findItems(name, Qt::MatchExactly).isEmpty()) {
            const auto answer =
                QMessageBox::question(this, "覆盖存档", "已存在同名存档，确定要覆盖吗？",
                                      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (answer != QMessageBox::Yes) {
                return;
            }
        }
        accept();
    } else {
        // 空库时列表没有任何项，这里就是唯一的拦截点。
        if (!list_widget_->currentItem()) {
            QMessageBox::warning(this, "错误", "请选择一个存档！");
            return;
        }
        accept();
    }
}

std::string SaveLoadDialog::get_save_name() const {
    // Load 模式没有名称输入框，调用本方法属于契约违反；抛异常让错误尽早暴露，
    // 而不是返回空串，把“模式不对”伪装成一个合法的存档名。
    if (mode_ != DialogMode::Save || name_input_ == nullptr) {
        throw std::logic_error("SaveLoadDialog::get_save_name() 只能在 Save 模式调用");
    }
    // 取名字时就地 trim，写库用的就是这份干净的字符串，列表项本身不改。
    return name_input_->text().trimmed().toStdString();
}

std::string SaveLoadDialog::get_selected_name() const {
    // 没有选中项时返回空串而不是崩溃；调用方（MainWindow）依赖 accept 前已选中的前提。
    if (list_widget_->currentItem()) {
        return list_widget_->currentItem()->text().toStdString();
    }
    return "";
}

}  // namespace ui