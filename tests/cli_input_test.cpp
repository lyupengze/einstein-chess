/// @file tests/cli_input_test.cpp
/// @brief CLI 纯解析函数的单元测试：种子、整数数组与档位序号。
///
/// 不启动 CLI 进程，也不依赖 Qt/SQLite；完整交互流程由 cli_session 等脚本用例
/// 覆盖，这里盯住边界值。

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
    expect(zero.has_value() && *zero == 0U, "parse_seed 接受 0");

    const auto normal = cli::parse_seed("42");
    expect(normal.has_value() && *normal == 42U, "parse_seed 接受 42");

    const auto maximum = cli::parse_seed(std::to_string(std::numeric_limits<unsigned int>::max()));
    expect(maximum.has_value() && *maximum == std::numeric_limits<unsigned int>::max(),
           "parse_seed 接受 unsigned int 最大值");

    expect(!cli::parse_seed("").has_value(), "parse_seed 拒绝空串");
    expect(!cli::parse_seed("-1").has_value(), "parse_seed 拒绝负号");
    expect(!cli::parse_seed("+1").has_value(), "parse_seed 拒绝正号");
    expect(!cli::parse_seed(" 1").has_value(), "parse_seed 拒绝前导空格");
    expect(!cli::parse_seed("1 ").has_value(), "parse_seed 拒绝尾随空格");
    expect(!cli::parse_seed("1abc").has_value(), "parse_seed 拒绝尾随字符");
    expect(!cli::parse_seed("abc").has_value(), "parse_seed 拒绝非数字");

    const std::string too_large = std::to_string(
        static_cast<unsigned long long>(std::numeric_limits<unsigned int>::max()) + 1ULL);
    expect(!cli::parse_seed(too_large).has_value(),
           "parse_seed 拒绝超过 unsigned int 最大值的输入");
}

void test_parse_int_array_4() {
    std::array<int, 4> values{};
    expect(cli::parse_int_array("0 2 1 2", values), "parse_int_array 接受 4 个整数");
    expect(values[0] == 0 && values[1] == 2 && values[2] == 1 && values[3] == 2,
           "parse_int_array 正确写入 4 个值");

    expect(cli::parse_int_array(" 0  2\t1 2 ", values), "parse_int_array 接受多余空白");
    expect(cli::parse_int_array("-1 0 0 0", values), "parse_int_array 把负坐标留给 core 拒绝");

    expect(!cli::parse_int_array("0 2 1", values), "parse_int_array 拒绝个数不足");
    expect(!cli::parse_int_array("0 2 1 2 9", values), "parse_int_array 拒绝多余整数");
    expect(!cli::parse_int_array("0 2 1 x", values), "parse_int_array 拒绝非整数");
    expect(!cli::parse_int_array("", values), "parse_int_array 拒绝空串");
}

void test_parse_int_array_6() {
    std::array<int, 6> values{};
    expect(cli::parse_int_array("1 2 3 4 5 6", values), "parse_int_array 接受 6 个整数");
    expect(cli::parse_int_array("1 1 2 3 4 5", values),
           "parse_int_array 把重复编号留给 setup_custom 拒绝");

    expect(!cli::parse_int_array("1 2 3 4 5", values), "parse_int_array 拒绝 5 个整数");
    expect(!cli::parse_int_array("1 2 3 4 5 6 7", values), "parse_int_array 拒绝 7 个整数");
}

void test_parse_level_index() {
    const auto first = cli::parse_level_index("1", 3);
    expect(first.has_value() && *first == 1, "parse_level_index 接受 1");

    const auto second = cli::parse_level_index(" 2 ", 3);
    expect(second.has_value() && *second == 2, "parse_level_index 接受带空白的 2");

    expect(!cli::parse_level_index("0", 3).has_value(), "parse_level_index 拒绝 0");
    expect(!cli::parse_level_index("4", 3).has_value(), "parse_level_index 拒绝超过上限的序号");
    expect(!cli::parse_level_index("1abc", 3).has_value(), "parse_level_index 拒绝尾随字符");
    expect(!cli::parse_level_index("1 2", 3).has_value(), "parse_level_index 拒绝多余内容");
    expect(!cli::parse_level_index("", 3).has_value(), "parse_level_index 拒绝空串");
}

}  // namespace

int main() {
    test_parse_seed();
    test_parse_int_array_4();
    test_parse_int_array_6();
    test_parse_level_index();
    return test_support::finish();
}
