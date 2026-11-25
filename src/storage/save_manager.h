// 存档模块：把 core::GameState 与当局 AI 档位序列化成一行文本，存进 SQLite。
// 数据库里只有一张表 saves：一个存档槽一行，name 为主键（同名即覆盖）。
// 与 core 的分工：core 只管内存中的局面，持久化格式与数据校验都在本模块。
// 存档会记录当局使用的 AI 档位，读档时原样恢复，保证一局之内电脑强度始终一致，
// 不会因为之后在“开始游戏”里换了档位而影响旧存档。

#ifndef SAVE_MANAGER_H_
#define SAVE_MANAGER_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ai/ai.h"
#include "core/game_state.h"
#include "sqlite_wrapper.h"

namespace storage {

/// @brief 一次读档的完整结果：局面 + 该档位记录的 AI 强度。
struct SavedGame {
    core::GameState state;  ///< 读出的对局局面，可直接交给 core::Game::load_game
    /// 存档时使用的 AI 档位；旧版本存档没有这一项，按 kDefaultLevel（当前最新版）恢复。
    ai::Level ai_level = ai::kDefaultLevel;
};

/// 默认数据库文件名。是相对路径，实际位置取决于进程的当前工作目录
/// （GUI 启动时会把工作目录切到 exe 所在目录，CLI 则跟随启动目录）。
constexpr std::string_view kDefaultDbPath = "einstein_save.db";

/// 存档管理器。构造时打开数据库并确保表结构存在。
class SaveManager {
public:
    /// @param db_path 数据库文件路径，不存在时自动创建；构造期间复制，不要求调用方长期持有
    /// @throws std::runtime_error 打开失败，或建表语句执行失败
    explicit SaveManager(std::string_view db_path);

    /// 保存到指定槽位；已存在同名槽位时覆盖。
    /// @param slot_name 槽位名（主键），同名即覆盖旧存档
    /// @param state 要保存的局面
    /// @param level 当局使用的 AI 档位，读档时原样恢复
    void save_game(const std::string& slot_name, const core::GameState& state, ai::Level level);

    /// 读取指定槽位。
    /// @return 槽位不存在返回 std::nullopt；数据损坏抛 std::runtime_error
    /// @note 旧存档缺少 AI 档位字段时，按 kDefaultLevel 恢复，不会拒绝读取。
    [[nodiscard]] std::optional<SavedGame> load_game(const std::string& slot_name);

    /// 列出全部存档名，按保存时间从新到旧；同一秒按名称升序。
    [[nodiscard]] std::vector<std::string> list_saves();

private:
    Database db_;
};

}  // namespace storage

#endif