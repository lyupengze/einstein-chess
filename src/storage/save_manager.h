/// @file src/storage/save_manager.h
/// @brief 存档读写：把局面与当局 AI 档位序列化进 SQLite。
///
/// 数据库里只有一张表 `saves`，一个槽位一行，name 为主键，同名即覆盖。
/// core 只管内存中的局面，持久化格式与数据校验都在本模块；存档记录当局的
/// AI 档位并在读档时原样恢复，一局之内电脑强度始终一致。

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ai/ai.h"
#include "core/game_state.h"
#include "sqlite_wrapper.h"

namespace storage {

/// @brief 一次读档的完整结果：局面 + 存档时记录的 AI 档位。
struct SavedGame {
    core::GameState state;                   ///< 读出的对局局面，可直接交给 core::Game::load_game
    ai::Level ai_level = ai::kDefaultLevel;  ///< 存档时使用的 AI 档位；读档时必填
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
    /// @throws std::invalid_argument AI 档位非法、局面处于布阵阶段，或局面不自洽
    /// @throws std::runtime_error 数据库写入失败
    void save_game(const std::string& slot_name, const core::GameState& state, ai::Level level);

    /// 读取指定槽位。
    /// @return 槽位不存在返回 std::nullopt；数据损坏抛 std::runtime_error
    /// @note AI 档位是必填字段；缺字段、非法档位或布阵阶段局面都会被拒绝。
    [[nodiscard]] std::optional<SavedGame> load_game(const std::string& slot_name);

    /// 列出全部存档名，按保存时间从新到旧；同一秒按名称升序。
    [[nodiscard]] std::vector<std::string> list_saves();

private:
    Database db_;
};

}  // namespace storage
