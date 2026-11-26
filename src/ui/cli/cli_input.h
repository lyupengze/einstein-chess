/// @file src/ui/cli/cli_input.h
/// @brief CLI 输入解析：把一整行文本解析成数字或定长数组。
///
/// 刻意不依赖 Qt、SQLite 或游戏核心，便于用普通 C++ 单元测试覆盖边界值；
/// 命令行参数与交互输入共用同一套规则，避免解析逻辑在 main_cli.cpp 里散落多份。

#pragma once

#include <array>
#include <charconv>
#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

namespace cli {

/// @brief 把 `--seed` 的文本解析为非负整数。
/// @param text 待解析的整行文本
/// @return 解析成功返回数值；空串、带正负号、含空白或尾随字符、超出
///         `unsigned int` 范围时返回 `std::nullopt`
/// @note 用 `std::from_chars` 而不是 `strtoul`，避免负号回绕、errno 的平台差异
///       以及 `unsigned long` 宽度不同带来的截断。
[[nodiscard]] inline std::optional<unsigned int> parse_seed(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }
    if (text.front() == '-' || text.front() == '+') {
        return std::nullopt;
    }

    unsigned int value = 0;
    const char* const first = text.data();
    const char* const last = first + text.size();
    const auto result = std::from_chars(first, last, value);

    // ec 非零表示非法字符或溢出；ptr 未到末尾表示还有多余字符。
    if (result.ec != std::errc{} || result.ptr != last) {
        return std::nullopt;
    }
    return value;
}

/// @brief 严格解析恰好 N 个整数。
/// @param text 待解析的整行文本
/// @param out 解析结果写入此处，元素个数由模板参数决定
/// @return 恰好读满 N 个整数且其后只剩空白时返回 true
/// @note 多一个 token（例如走法写成 5 个数）直接返回 false，不像 `operator>>`
///       循环那样静默忽略；负数可以解析，范围由 `core::rules` 决定。
template <std::size_t N>
[[nodiscard]] bool parse_int_array(std::string_view text, std::array<int, N>& out) {
    std::istringstream stream{std::string{text}};
    for (int& value : out) {
        if (!(stream >> value)) {
            return false;
        }
    }
    stream >> std::ws;
    return stream.eof();
}

/// @brief 解析“电脑强度”菜单的 1-based 编号。
/// @param text 待解析的整行文本
/// @param count 可选档位总数，由 `ai::kAllLevels.size()` 传入
/// @return 落在 1..count 内返回编号，否则返回 `std::nullopt`；空行由调用方处理
[[nodiscard]] inline std::optional<int> parse_level_index(std::string_view text, int count) {
    std::array<int, 1> values{};
    if (!parse_int_array(text, values)) {
        return std::nullopt;
    }
    if (values[0] < 1 || values[0] > count) {
        return std::nullopt;
    }
    return values[0];
}

}  // namespace cli
