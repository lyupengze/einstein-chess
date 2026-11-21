# CLI 的 EOF 回归：BUG（报告未覆盖）——stdin 关闭时旧实现死循环刷屏。
#
# 用法：cmake -DEXE=<einstein_cli> -DINPUT=<某个文件> -P cli_eof_test.cmake
# 把 INPUT 接到子进程的 stdin，模拟脚本/CI 里"输入旋即结束"的场景。

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
)

if(NOT cli_result STREQUAL "0")
    message(FATAL_ERROR
        "CLI did not exit cleanly on EOF (result='${cli_result}').\n"
        "stdout:\n${cli_stdout}\nstderr:\n${cli_stderr}")
endif()

# 旧实现会在 EOF 后无限循环打印提示语，几秒钟就能刷出上百 KB。
string(LENGTH "${cli_stdout}" stdout_length)
if(stdout_length GREATER 4096)
    message(FATAL_ERROR
        "CLI produced ${stdout_length} bytes of output on EOF; "
        "this looks like the old infinite prompt loop.")
endif()

message(STATUS "cli_eof: exited 0 with ${stdout_length} bytes of output")
