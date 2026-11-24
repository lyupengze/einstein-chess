// Botzone 无状态协议层：把平台下发的一帧 JSON 请求转换成 AI 决策，
// 再组装成平台要求的一帧 JSON 响应。
//
// 单独抽成库是为了让协议解析、棋盘解码与响应组装可以脱离 stdin/stdout 做单元测试；
// main_botzone.cpp 只负责读一行、调用本层、写一行，并在异常时输出空 response。
//
// 协议约定（详见 README 的 Botzone 一节）：
//   - requests 为空：首次调用，返回 response.layout 布阵；
//   - 最后一个 request 的 phase == "layout"：重新布阵；
//   - 最后一个 request 的 phase == "move"：读取 color、棋盘与骰子，返回 x0/y0/x1/y1；
//   - 其它阶段：返回空 response。
// 任何字段缺失、类型错误、编号越界/重复都会抛异常，由调用方统一兜底成空 response。

#ifndef BOTZONE_PROTOCOL_H_
#define BOTZONE_PROTOCOL_H_

#include <string>

#include <nlohmann/json.hpp>

namespace botzone {

/// @brief 处理一帧已经解析好的 Botzone 请求。
/// @param input 平台下发的 JSON 对象
/// @return 形如 {"response": {...}} 的 JSON；未知阶段返回空 response
/// @throws std::exception JSON 字段类型不符、颜色/骰子/棋盘编号非法，
///         或 AI 无合法着法时抛出
[[nodiscard]] nlohmann::json handle_request(const nlohmann::json& input);

/// @brief 解析一行 JSON 文本并处理，便于 main 与测试共用同一条入口。
/// @param line 单行 JSON 文本
/// @return 与 handle_request 相同的响应对象
/// @throws std::exception JSON 语法错误或 handle_request 的任何异常
[[nodiscard]] nlohmann::json handle_line(const std::string& line);

}  // namespace botzone

#endif  // BOTZONE_PROTOCOL_H_
