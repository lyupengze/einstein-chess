#ifndef SAVE_MANAGER_H_
#define SAVE_MANAGER_H_

#include <optional>
#include <string>
#include <vector>

#include "core/game_state.h"
#include "sqlite_wrapper.h"

namespace storage {

constexpr const char* kDefaultDbPath = "einstein_save.db";

class SaveManager {
public:
    explicit SaveManager(const std::string& db_path);

    void save_game(const std::string& slot_name, const core::GameState& state);

    [[nodiscard]] std::optional<core::GameState> load_game(const std::string& slot_name);

    [[nodiscard]] std::vector<std::string> list_saves();

private:
    Database db_;
};

}  // namespace storage

#endif