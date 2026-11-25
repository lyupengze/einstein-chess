# CLI 异常边界回归：构造一个 saves 表结构损坏的数据库，验证“读档列表”
# 失败时 CLI 只打印原因并回到主菜单，而不是让异常穿出 main。
#
# 用法：
#   cmake -DEXE=<einstein_cli> -DHELPER=<make_broken_save_db>
#         -DINPUT=<cli_broken_db_input.txt> -DWORKDIR=<dir>
#         -P cli_broken_db_test.cmake

if(NOT DEFINED EXE OR NOT DEFINED HELPER OR NOT DEFINED INPUT OR NOT DEFINED WORKDIR)
    message(FATAL_ERROR
        "cli_broken_db_test.cmake requires -DEXE, -DHELPER, -DINPUT and -DWORKDIR")
endif()

file(MAKE_DIRECTORY "${WORKDIR}")
set(DATABASE "${WORKDIR}/einstein_save.db")
# 只删旧库，不删除工作目录本身：CTest 的 WORKING_DIRECTORY 指向这里，
# 删除当前目录在 Windows 上既脆弱又会拖慢用例。
file(REMOVE "${DATABASE}")

# 先建一个只有 x 列的 saves 表；SaveManager 的 CREATE TABLE IF NOT EXISTS
# 不会修复它，随后 list_saves() 的 SELECT name 必然失败。
execute_process(
    COMMAND "${HELPER}" "${DATABASE}"
    OUTPUT_VARIABLE helper_stdout
    ERROR_VARIABLE helper_stderr
    RESULT_VARIABLE helper_result
    TIMEOUT 20
    ENCODING UTF-8
)
if(NOT helper_result STREQUAL "0")
    message(FATAL_ERROR
        "Failed to create the broken save database (result='${helper_result}').\n"
        "stdout:\n${helper_stdout}\nstderr:\n${helper_stderr}")
endif()
if(NOT EXISTS "${DATABASE}")
    message(FATAL_ERROR "The broken save database was not created: ${DATABASE}")
endif()

execute_process(
    COMMAND "${EXE}"
    WORKING_DIRECTORY "${WORKDIR}"
    INPUT_FILE "${INPUT}"
    OUTPUT_VARIABLE cli_stdout
    ERROR_VARIABLE cli_stderr
    RESULT_VARIABLE cli_result
    TIMEOUT 20
    # 与其它 CLI 用例一致：显式按 UTF-8 解码，避免 Windows 控制台代码页干扰中文断言。
    ENCODING UTF-8
)

if(NOT cli_result STREQUAL "0")
    message(FATAL_ERROR
        "CLI did not survive a broken save table (result='${cli_result}').\n"
        "stdout:\n${cli_stdout}\nstderr:\n${cli_stderr}")
endif()

# 必须走到新加的 list_saves 异常分支。
string(FIND "${cli_stdout}" "读取存档列表失败:" list_failure)
if(list_failure EQUAL -1)
    message(FATAL_ERROR
        "CLI did not report the broken save table.\nstdout:\n${cli_stdout}")
endif()

# 失败后必须回到主菜单，而不是误报“没有存档”或直接退出。
string(FIND "${cli_stdout}" "没有找到任何存档！" empty_saves)
if(NOT empty_saves EQUAL -1)
    message(FATAL_ERROR
        "CLI treated the database error as an empty save list.\nstdout:\n${cli_stdout}")
endif()

# 输入 3 后必须正常退出，证明异常没有破坏菜单循环。
string(FIND "${cli_stdout}" "感谢你的游玩" farewell)
if(farewell EQUAL -1)
    message(FATAL_ERROR
        "CLI did not reach the normal exit path after the database error.\n"
        "stdout:\n${cli_stdout}")
endif()

message(STATUS "cli_broken_db: reported the broken save table and returned to the menu")
