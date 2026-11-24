# CLI 正向回归：主菜单输入 3 必须打印感谢语并以退出码 0 结束。
#
# 用法：cmake -DEXE=<einstein_cli> -DINPUT=<内容为 "3" 的文件> -P cli_exit_test.cmake
#
# 与 cli_eof_test.cmake 互补：那个用例覆盖“输入提前结束”，这个覆盖“用户主动退出”，
# 两者都要求退出码 0，但正常退出不应出现 EOF 提示。

if(NOT DEFINED EXE OR NOT DEFINED INPUT)
    message(FATAL_ERROR "cli_exit_test.cmake requires -DEXE=<path> and -DINPUT=<path>")
endif()

execute_process(
    COMMAND "${EXE}"
    INPUT_FILE "${INPUT}"
    OUTPUT_VARIABLE cli_stdout
    ERROR_VARIABLE cli_stderr
    RESULT_VARIABLE cli_result
    TIMEOUT 20
    # 与 cli_eof 相同：显式按 UTF-8 解码，避免 Windows 控制台代码页干扰中文断言。
    ENCODING UTF-8
)

if(NOT cli_result STREQUAL "0")
    message(FATAL_ERROR
        "CLI did not exit with 0 after choosing '3' (result='${cli_result}').\n"
        "stdout:\n${cli_stdout}\nstderr:\n${cli_stderr}")
endif()

string(FIND "${cli_stdout}" "感谢你的游玩" farewell)
if(farewell EQUAL -1)
    message(FATAL_ERROR
        "CLI did not print the farewell message after choosing '3'.\n"
        "stdout:\n${cli_stdout}")
endif()

# 主动退出走的是菜单分支，不应该落到“输入已结束”的 EOF 提示上。
string(FIND "${cli_stdout}" "输入已结束" eof_notice)
if(NOT eof_notice EQUAL -1)
    message(FATAL_ERROR
        "CLI took the EOF path instead of the normal exit path.\nstdout:\n${cli_stdout}")
endif()

string(LENGTH "${cli_stdout}" stdout_length)
if(stdout_length GREATER 4096)
    message(FATAL_ERROR
        "CLI produced ${stdout_length} bytes for a single exit; expected a short menu.")
endif()

message(STATUS "cli_exit: exited 0 with the farewell message")
