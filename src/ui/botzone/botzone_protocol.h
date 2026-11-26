/// @file src/ui/botzone/botzone_protocol.h
/// @brief Botzone 无状态协议层：一帧 JSON 请求 → 一次 AI 决策 → 一帧 JSON 响应。
///
/// 单独抽成库，使协议解析、棋盘解码与响应组装可以脱离 stdin/stdout 做单元测试；
/// main_botzone.cpp 只负责读一行、调用本层、写一行，并在异常时输出空 response。
/// 协议细节见 `docs/BOTZONE.md`。
///
/// 结构/类型不符合协议时抛 ProtocolError，AI 内部错误抛普通 std::runtime_error；
/// 两者由 main 统一兜底成空 response，日志可以区分“平台数据坏了”和“我们算错了”。
///
/// 响应形状：
/// - requests 为空：首次调用，返回 response.layout 布阵；
/// - 最后一个 request 的 phase == "layout"：重新布阵；
/// - 最后一个 request 的 phase == "move"：读取 color、棋盘与骰子，返回 x0/y0/x1/y1；
/// - 其它阶段：返回空 response。

#pragma once

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace botzone {

/// @brief 平台下发的请求不符合 Botzone 协议（字段缺失、类型错误、棋盘维度不对等）。
///
/// 与 AI 内部错误分开，便于 main 输出不同日志；线上仍统一回空 response。
class ProtocolError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// @brief 构造协议层的空响应，表示“本次没有决策”。
///
/// Botzone 未定义非法输入应如何响应；返回一个保底着法会掩盖协议错误，
/// 因此这里只回空 response，由平台决定后续处理。不要在错误路径伪造走子。
/// @return 形如 {"response": {}} 的 JSON 对象
[[nodiscard]] nlohmann::json empty_response();

/// @brief 处理一帧已经解析好的 Botzone 请求。
/// @param input 平台下发的 JSON 对象
/// @return 形如 {"response": {...}} 的 JSON；未知阶段返回空 response
/// @throws ProtocolError 请求结构/字段类型/棋盘维度不符合协议
/// @throws std::invalid_argument AI 无合法着法（局面不自洽，属内部错误）
[[nodiscard]] nlohmann::json handle_request(const nlohmann::json& input);

/// @brief 解析一行 JSON 文本并处理，便于 main 与测试共用同一条入口。
/// @param line 单行 JSON 文本
/// @return 与 handle_request 相同的响应对象
/// @throws std::exception JSON 语法错误或 handle_request 的任何异常
[[nodiscard]] nlohmann::json handle_line(const std::string& line);

}  // namespace botzone
