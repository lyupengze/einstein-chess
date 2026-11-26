/// @file src/ui/botzone/main_botzone.cpp
/// @brief Botzone 适配入口：读一行 JSON、调用协议层、写一行 JSON。
///
/// 每次进程启动只处理一帧，对局历史由平台保存，本程序不持有跨请求状态；
/// 协议解析与响应组装都在 botzone_protocol 库里，本文件只负责 stdin/stdout
/// 与异常兜底。

#include <exception>
#include <iostream>
#include <string>

#include "botzone_protocol.h"

int main() {
    // 平台每次调用通过 stdin 给一行 JSON；读不到（EOF）就安静退出。
    std::string input_line;
    if (!std::getline(std::cin, input_line)) {
        return 0;
    }

    try {
        // 一行紧凑 JSON + 换行，构成完整的一帧响应。
        std::cout << botzone::handle_line(input_line).dump() << std::endl;
    } catch (const botzone::ProtocolError& e) {
        // 平台数据不符合协议：这是外部输入问题，值得单独标出来排查。
        // Botzone 没有定义错误响应，仍回空 response，避免伪造着法。
        std::cerr << "Bot protocol error: " << e.what() << std::endl;
        std::cout << botzone::empty_response().dump() << std::endl;
    } catch (const std::exception& e) {
        // 内部错误（例如 AI 无路可走）：同样回空 response，但日志区分开。
        std::cerr << "Bot internal error: " << e.what() << std::endl;
        std::cout << botzone::empty_response().dump() << std::endl;
    }

    return 0;
}
