// CLI 纯解析函数的单元测试。
//
// 这些用例不启动 CLI 进程，也不依赖 Qt/SQLite；完整交互流程由
// cli_session 等脚本用例覆盖，这里专门盯住边界值，避免解析规则
// 在 main_cli.cpp 里被改回去。

#include "cli_input.h"

#include <array>
#include <limits>
#include <optional>
#include <string>

#include "test_support.h"

using test_support::expect;

namespace {

void test_parse_seed() {
    const auto zero = cli::parse_seed("0");
    expect(zero.has_value() && *zero == 0U, "parse_seed accepts zero");

    const auto normal = cli::parse_seed("42");
    expect(normal.has_value() && *normal == 42U, "parse_seed accepts 42");

    const auto maximum = cli::parse_seed(std::to_string(std::numeric_limits<unsigned int>::max()));
    expect(maximum.has_value() && *maximum == std::numeric_limits<unsigned int>::max(),
           "parse_seed accepts unsigned int max");

    expect(!cli::parse_seed("").has_value(), "parse_seed rejects empty text");
    expect(!cli::parse_seed("-1").has_value(), "parse_seed rejects negative text");
    expect(!cli::parse_seed("+1").has_value(), "parse_seed rejects plus sign");
    expect(!cli::parse_seed(" 1").has_value(), "parse_seed rejects leading space");
    expect(!cli::parse_seed("1 ").has_value(), "parse_seed rejects trailing space");
    expect(!cli::parse_seed("1abc").has_value(), "parse_seed rejects trailing chars");
    expect(!cli::parse_seed("abc").has_value(), "parse_seed rejects non-digits");

    const std::string too_large = std::to_string(
        static_cast<unsigned long long>(std::numeric_limits<unsigned int>::max()) + 1ULL);
    expect(!cli::parse_seed(too_large).has_value(),
           "parse_seed rejects values above unsigned int max");
}

void test_parse_int_array_4() {
    std::array<int, 4> values{};
    expect(cli::parse_int_array("0 2 1 2", values), "parse_int_array accepts four ints");
    expect(values[0] == 0 && values[1] == 2 && values[2] == 1 && values[3] == 2,
           "parse_int_array keeps four values");

    expect(cli::parse_int_array(" 0  2\t1 2 ", values), "parse_int_array accepts extra whitespace");
    expect(cli::parse_int_array("-1 0 0 0", values),
           "parse_int_array lets core reject negative coordinates");

    expect(!cli::parse_int_array("0 2 1", values), "parse_int_array rejects too few ints");
    expect(!cli::parse_int_array("0 2 1 2 9", values), "parse_int_array rejects trailing token");
    expect(!cli::parse_int_array("0 2 1 x", values), "parse_int_array rejects non-integer token");
    expect(!cli::parse_int_array("", values), "parse_int_array rejects empty text");
}

void test_parse_int_array_6() {
    std::array<int, 6> values{};
    expect(cli::parse_int_array("1 2 3 4 5 6", values), "parse_int_array accepts six ints");
    expect(cli::parse_int_array("1 1 2 3 4 5", values),
           "parse_int_array leaves duplicate detection to setup_custom");

    expect(!cli::parse_int_array("1 2 3 4 5", values), "parse_int_array rejects five ints");
    expect(!cli::parse_int_array("1 2 3 4 5 6 7", values), "parse_int_array rejects seven ints");
}

void test_parse_level_index() {
    const auto first = cli::parse_level_index("1", 3);
    expect(first.has_value() && *first == 1, "parse_level_index accepts 1");

    const auto second = cli::parse_level_index(" 2 ", 3);
    expect(second.has_value() && *second == 2, "parse_level_index accepts padded 2");

    expect(!cli::parse_level_index("0", 3).has_value(), "parse_level_index rejects zero");
    expect(!cli::parse_level_index("4", 3).has_value(),
           "parse_level_index rejects index above count");
    expect(!cli::parse_level_index("1abc", 3).has_value(),
           "parse_level_index rejects trailing chars");
    expect(!cli::parse_level_index("1 2", 3).has_value(), "parse_level_index rejects extra token");
    expect(!cli::parse_level_index("", 3).has_value(), "parse_level_index rejects empty text");
}

}  // namespace

int main() {
    test_parse_seed();
    test_parse_int_array_4();
    test_parse_int_array_6();
    test_parse_level_index();
    return test_support::finish();
}
