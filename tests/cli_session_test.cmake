# CLI 会话回归：把“主菜单 -> 开新局 -> 选强度 -> 选色 -> 布阵”的完整交互
# 用脚本输入走一遍，验证每条错误输入都会重新询问、正确输入能推进流程，
# 并确认对局中途 stdin 结束时仍能打印提示并正常返回主菜单。
#
# 用法：cmake -DEXE=<einstein_cli> -DINPUT=<cli_session_input.txt> -P cli_session_test.cmake
#
# 输入文件刻意包含四类错误：非法主菜单命令、空存档库、非法强度编号、
# 非法颜色与非法布阵（数量不足/编号重复），最后才是一组合法布阵。
# 对局开始后不再喂输入，让 stdin 在“按回车掷骰子”处结束。

if(NOT DEFINED EXE OR NOT DEFINED INPUT)
    message(FATAL_ERROR "cli_session_test.cmake requires -DEXE=<path> and -DINPUT=<path>")
endif()

execute_process(
    COMMAND "${EXE}"
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
        "CLI session did not exit with 0 (result='${cli_result}').\n"
        "stdout:\n${cli_stdout}\nstderr:\n${cli_stderr}")
endif()

# 1. 主菜单输入 9 后必须提示非法命令，而不是退出或进入某个分支。
string(FIND "${cli_stdout}" "命令不合法" invalid_menu)
if(invalid_menu EQUAL -1)
    message(FATAL_ERROR "CLI did not reject the invalid main-menu command.\nstdout:\n${cli_stdout}")
endif()

# 2. 空存档库必须给出明确提示，并回到主菜单继续接受输入。
string(FIND "${cli_stdout}" "没有找到任何存档！" empty_saves)
if(empty_saves EQUAL -1)
    message(FATAL_ERROR "CLI did not report an empty save list.\nstdout:\n${cli_stdout}")
endif()

# 3. 非法强度编号（0）应被拒绝；随后空行取默认档位，输出其展示名。
string(FIND "${cli_stdout}" "电脑强度: 搜索（第二版）" default_level)
if(default_level EQUAL -1)
    message(FATAL_ERROR
        "CLI did not fall back to the default AI level after an invalid index.\n"
        "stdout:\n${cli_stdout}")
endif()

# 4. 非法颜色后重新询问，输入 1 后应确认红方。
string(FIND "${cli_stdout}" "你选择了红方" red_side)
if(red_side EQUAL -1)
    message(FATAL_ERROR "CLI did not accept the valid color choice.\nstdout:\n${cli_stdout}")
endif()

# 5. 数量不足按格式错误处理；重复编号由 Board::setup_custom 拒绝，并给出原因。
string(FIND "${cli_stdout}" "布阵失败:" layout_failure)
if(layout_failure EQUAL -1)
    message(FATAL_ERROR
        "CLI did not report the rejected layout.\nstdout:\n${cli_stdout}")
endif()

# 6. 合法布阵后应进入对局；stdin 在第一次掷骰询问处结束，随后回主菜单再退出。
string(FIND "${cli_stdout}" "【开始游戏】" game_started)
if(game_started EQUAL -1)
    message(FATAL_ERROR "CLI did not start the game after a valid layout.\nstdout:\n${cli_stdout}")
endif()

string(FIND "${cli_stdout}" "输入已结束，本局结束。" midgame_eof)
if(midgame_eof EQUAL -1)
    message(FATAL_ERROR
        "CLI did not leave the game cleanly when stdin ended mid-game.\n"
        "stdout:\n${cli_stdout}")
endif()

string(FIND "${cli_stdout}" "输入已结束，退出。" final_eof)
if(final_eof EQUAL -1)
    message(FATAL_ERROR
        "CLI did not exit the main menu cleanly after the game ended.\n"
        "stdout:\n${cli_stdout}")
endif()

# 交互脚本的输出主要是棋盘与提示，正常应在几千字节内；给一个宽松上限防止刷屏。
string(LENGTH "${cli_stdout}" stdout_length)
if(stdout_length GREATER 20000)
    message(FATAL_ERROR
        "CLI produced ${stdout_length} bytes for the scripted session; "
        "this looks like a prompt loop.")
endif()

# 7. 参数校验：--seed 必须是非负十进制整数，负数/带符号/溢出/非数字
#    都应在打开数据库前以退出码 2 拒绝。
foreach(bad_seed -1 +1 4294967296 not-a-number)
    execute_process(
        COMMAND "${EXE}" --seed "${bad_seed}"
        OUTPUT_VARIABLE seed_stdout
        ERROR_VARIABLE seed_stderr
        RESULT_VARIABLE seed_result
        TIMEOUT 20
        ENCODING UTF-8
    )
    if(NOT seed_result STREQUAL "2")
        message(FATAL_ERROR
            "CLI accepted an invalid --seed value '${bad_seed}' (result='${seed_result}').\n"
            "stdout:\n${seed_stdout}\nstderr:\n${seed_stderr}")
    endif()
endforeach()

message(STATUS "cli_session: rejected invalid input and exited cleanly on mid-game EOF")
