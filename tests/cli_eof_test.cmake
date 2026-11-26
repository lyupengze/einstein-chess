# CLI 的 EOF 回归：stdin 在启动后立即关闭，验证程序不进入提示循环。
#
# 用法：cmake -DEXE=<einstein_cli> -DINPUT=<某个文件> -P cli_eof_test.cmake
# 把 INPUT 接到子进程的 stdin，模拟脚本/CI 里"输入旋即结束"的场景。
#
# 这里不只数输出字节，还断言了退出前必须打印的 EOF 提示与主菜单提示次数：
# 字节阈值只能发现"刷屏"，发现不了"安静地不退出"或"只打印一次提示但退出码错误"。

if(NOT DEFINED EXE OR NOT DEFINED INPUT)
    message(FATAL_ERROR "cli_eof_test.cmake requires -DEXE=<path> and -DINPUT=<path>")
endif()

execute_process(
    COMMAND "${EXE}"
    INPUT_FILE "${INPUT}"
    OUTPUT_VARIABLE cli_stdout
    ERROR_VARIABLE cli_stderr
    RESULT_VARIABLE cli_result
    TIMEOUT 20
    # Windows 上 execute_process 默认按控制台代码页解码子进程输出；CTest
    # 没有稳定控制台，同一份 UTF-8 输出可能被当成 GBK 解出乱码，导致下面
    # 的中文字面量匹配失败。CLI 固定以 UTF-8 写 stdout，这里显式声明编码。
    ENCODING UTF-8
)

if(NOT cli_result STREQUAL "0")
    message(FATAL_ERROR
        "CLI did not exit cleanly on EOF (result='${cli_result}').\n"
        "stdout:\n${cli_stdout}\nstderr:\n${cli_stderr}")
endif()

# 正常路径只会打印少量提示；超过 4 KB 说明同一句提示被反复打印，即提示循环。
string(LENGTH "${cli_stdout}" stdout_length)
if(stdout_length GREATER 4096)
    message(FATAL_ERROR
        "CLI produced ${stdout_length} bytes of output on EOF; "
        "这看起来是提示循环。")
endif()

# 语义断言 1：读到 EOF 后必须明确告诉用户“输入已结束，退出。”，
# 而不是悄无声息地返回 0，让调用方分不清正常退出与崩溃。
string(FIND "${cli_stdout}" "输入已结束，退出。" eof_notice)
if(eof_notice EQUAL -1)
    message(FATAL_ERROR
        "CLI exited without printing the EOF notice.\nstdout:\n${cli_stdout}")
endif()

# 语义断言 2：主菜单提示只应出现一次；提示循环的典型特征就是同一句提示反复出现。
string(REGEX MATCHALL "请选择" menu_prompts "${cli_stdout}")
list(LENGTH menu_prompts prompt_count)
if(prompt_count GREATER 1)
    message(FATAL_ERROR
        "CLI printed the main-menu prompt ${prompt_count} times on EOF; "
        "stdout looks like a prompt loop.")
endif()

message(STATUS "cli_eof: exited 0 with ${stdout_length} bytes and the EOF notice")
