// 存档模块：把 core::GameState 序列化成一行文本，存进 SQLite。
// 数据库里只有一张表 saves：一个存档槽一行，name 为主键（同名即覆盖）。
// 与 core 的分工：core 只管内存中的局面，持久化格式与数据校验都在本模块。

#ifndef SAVE_MANAGER_H_
#define SAVE_MANAGER_H_

#include <optional>
#include <string>
#include <vector>

#include "core/game_state.h"
#include "sqlite_wrapper.h"

namespace storage {

/// 默认数据库文件名。是相对路径，实际位置取决于进程的当前工作目录
/// （GUI 启动时会把工作目录切到 exe 所在目录，CLI 则跟随启动目录）。
constexpr const char* kDefaultDbPath = "einstein_save.db";

/// 存档管理器。构造时打开数据库并确保表结构存在。
class SaveManager {
public:
    /// @param db_path 数据库文件路径，不存在时自动创建
    /// @throws std::runtime_error 打开失败，或建表语句执行失败
    explicit SaveManager(const std::string& db_path);

    /// 保存到指定槽位；已存在同名槽位时覆盖。
    void save_game(const std::string& slot_name, const core::GameState& state);

    /// 读取指定槽位。
    /// @return 槽位不存在返回 std::nullopt；数据损坏抛 std::runtime_error
    [[nodiscard]] std::optional<core::GameState> load_game(const std::string& slot_name);

    /// 列出全部存档名，按保存时间从新到旧。
    [[nodiscard]] std::vector<std::string> list_saves();

private:
    Database db_;
};

}  // namespace storage

#endif