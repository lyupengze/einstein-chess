// CLI 输入解析工具。
//
// 这里刻意不依赖 Qt、SQLite 或游戏核心，只做“字符串 -> 数字”的纯解析，
// 便于用普通 C++ 单元测试覆盖边界值。命令行参数和交互输入共用同一套规则，
// 避免 main_cli.cpp 里散落多份略有差异的解析代码。

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

// 把 --seed 的文本解析为非负整数。
//
// 只接受纯十进制数字，不接受正负号、前后空白或尾随字符；超出
// unsigned int 范围（通常是 0..4294967295）时返回 std::nullopt。
// 使用 std::from_chars 而不是 strtoul，是为了避免负号回绕、errno
// 平台差异以及 unsigned long 宽度不同带来的截断。
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

// 严格解析恰好 N 个整数。
//
// 读满 N 个 int 后只允许剩余空白；多一个 token（例如走法写成 5 个数）
// 就返回 false，而不是像 operator>> 循环那样静默忽略。负数可以解析，
// 是否在棋盘范围内由 core/rules 决定。
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

// 解析“电脑强度”菜单的 1-based 编号。
//
// 空行由调用方处理为默认档位；这里只接受恰好一个整数，且必须落在
// 1..count 内。count 由 ai::kAllLevels.size() 传入，避免在 CLI 里写死档位数。
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
