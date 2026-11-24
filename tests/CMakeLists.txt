# 回归测试：把报告里"可稳定复现"且能自动化的缺陷固化下来。
# 图形用例统一在 offscreen 平台上跑，无显示器的 CI 环境同样可用。
#
# 所有用例都经 einstein_add_test 注册，把超时、标签、工作目录等 CTest 级
# 约束集中在一处，避免单个用例漏配超时、或并行时抢同一个数据库文件。

include(CMakeParseArguments)

# einstein_add_test(<测试名> <可执行目标>
#                   [TIMEOUT <秒>]
#                   [LABEL <标签>...]
#                   [ENVIRONMENT <变量=值>...]
#                   [RESOURCE_LOCK <资源名>]
#                   [WORKING_DIRECTORY <目录>])
#
# 默认超时 60 秒：任何一个用例卡死时 CTest 会杀掉进程并判失败，
# 而不是让整条 ctest 永远挂住。LABEL 供 `ctest -L gui` 之类的子集运行使用。
function(einstein_add_test name target)
    cmake_parse_arguments(TEST "" "TIMEOUT;RESOURCE_LOCK;WORKING_DIRECTORY"
                          "LABEL;ENVIRONMENT" ${ARGN})

    if(NOT TEST_TIMEOUT)
        set(TEST_TIMEOUT 60)
    endif()

    add_test(NAME ${name} COMMAND ${target})
    set_tests_properties(${name} PROPERTIES TIMEOUT ${TEST_TIMEOUT})

    if(TEST_LABEL)
        set_tests_properties(${name} PROPERTIES LABELS "${TEST_LABEL}")
    endif()
    if(TEST_ENVIRONMENT)
        set_tests_properties(${name} PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")
    endif()
    if(TEST_RESOURCE_LOCK)
        set_tests_properties(${name} PROPERTIES RESOURCE_LOCK "${TEST_RESOURCE_LOCK}")
    endif()
    if(TEST_WORKING_DIRECTORY)
        set_tests_properties(${name} PROPERTIES WORKING_DIRECTORY "${TEST_WORKING_DIRECTORY}")
    endif()
endfunction()

add_executable(test_board_geometry board_geometry_test.cpp)
target_link_libraries(test_board_geometry PRIVATE einstein_gui_lib einstein_warnings)
# Qt 的 offscreen 平台插件不需要显示器；RESOURCE_LOCK 让三个 GUI 用例串行，
# 避免并行时互相抢 CPU 把动画/定时器断言拖到超时。
einstein_add_test(board_geometry test_board_geometry
                  TIMEOUT 60 LABEL gui
                  ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
                  RESOURCE_LOCK qt_gui)

add_executable(test_ai_random ai_random_test.cpp)
target_link_libraries(test_ai_random PRIVATE einstein_ai einstein_warnings)
einstein_add_test(ai_random test_ai_random TIMEOUT 30 LABEL ai)

add_executable(test_core_rules core_rules_test.cpp)
target_link_libraries(test_core_rules PRIVATE einstein_core einstein_warnings)
einstein_add_test(core_rules test_core_rules TIMEOUT 60 LABEL core)

add_executable(test_ai_search ai_search_test.cpp)
target_link_libraries(test_ai_search PRIVATE einstein_ai einstein_warnings)
einstein_add_test(ai_search test_ai_search TIMEOUT 120 LABEL ai)

add_executable(test_ai_level ai_level_test.cpp)
target_link_libraries(test_ai_level PRIVATE einstein_ai einstein_warnings)
einstein_add_test(ai_level test_ai_level TIMEOUT 30 LABEL ai)

add_executable(test_botzone_protocol botzone_protocol_test.cpp)
# 协议库已 PUBLIC 暴露 core/ai 与 third_party 的 include，测试只需链接它。
target_link_libraries(test_botzone_protocol PRIVATE einstein_botzone_protocol einstein_warnings)
einstein_add_test(botzone_protocol test_botzone_protocol TIMEOUT 60 LABEL botzone)

add_executable(test_save_level save_level_test.cpp)
# 测试要直接改 SQLite 里的 data 列来伪造旧档，因此显式链接 sqlite3；
# storage 自身只 PRIVATE 依赖它，不会自动传递过来。
target_link_libraries(test_save_level PRIVATE einstein_storage sqlite3 einstein_warnings)
# 每个用例独占一个工作目录：存档文件名由测试自己决定，不同时跑也不会互删。
set(SAVE_LEVEL_WORK_DIR ${CMAKE_CURRENT_BINARY_DIR}/save_level)
file(MAKE_DIRECTORY ${SAVE_LEVEL_WORK_DIR})
einstein_add_test(save_level test_save_level
                  TIMEOUT 60 LABEL storage
                  WORKING_DIRECTORY ${SAVE_LEVEL_WORK_DIR})

add_executable(test_storage_roundtrip storage_roundtrip_test.cpp)
# 与 save_level 一样：伪造损坏数据要直接写 SQLite，因此显式链接 sqlite3。
target_link_libraries(test_storage_roundtrip PRIVATE einstein_storage sqlite3 einstein_warnings)
set(STORAGE_ROUNDTRIP_WORK_DIR ${CMAKE_CURRENT_BINARY_DIR}/storage_roundtrip)
file(MAKE_DIRECTORY ${STORAGE_ROUNDTRIP_WORK_DIR})
einstein_add_test(storage_roundtrip test_storage_roundtrip
                  TIMEOUT 60 LABEL storage
                  WORKING_DIRECTORY ${STORAGE_ROUNDTRIP_WORK_DIR})

add_executable(test_gui_reconcile gui_reconcile_test.cpp)
target_link_libraries(test_gui_reconcile PRIVATE einstein_gui_lib einstein_warnings)
einstein_add_test(gui_reconcile test_gui_reconcile
                  TIMEOUT 60 LABEL gui
                  ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
                  RESOURCE_LOCK qt_gui)

add_executable(test_toast_fade toast_fade_test.cpp)
target_link_libraries(test_toast_fade PRIVATE einstein_gui_lib einstein_warnings)
einstein_add_test(toast_fade test_toast_fade
                  TIMEOUT 60 LABEL gui
                  ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
                  RESOURCE_LOCK qt_gui)

# CLI 用例统一在独立目录里跑：CLI 的默认存档库是相对路径 einstein_save.db，
# 固定工作目录可以避免读到开发者本机遗留的存档，也能防止用例间互相影响。
set(CLI_WORK_DIR ${CMAKE_CURRENT_BINARY_DIR}/cli)
file(MAKE_DIRECTORY ${CLI_WORK_DIR})

# CLI 的 EOF 回归：用 cmake -P 脚本喂文件给二进制，
# 避免依赖各平台不同的管道/重定向语法。
add_test(NAME cli_eof
         COMMAND ${CMAKE_COMMAND}
                 -DEXE=$<TARGET_FILE:einstein_cli>
                 -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/data/eof_input.txt
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_eof_test.cmake
)
set_tests_properties(cli_eof PROPERTIES
                     TIMEOUT 30 LABELS cli WORKING_DIRECTORY ${CLI_WORK_DIR})

# CLI 正向用例：菜单里输入 3，必须打印感谢语并正常退出。
add_test(NAME cli_exit
         COMMAND ${CMAKE_COMMAND}
                 -DEXE=$<TARGET_FILE:einstein_cli>
                 -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/data/cli_exit_input.txt
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_exit_test.cmake
)
set_tests_properties(cli_exit PROPERTIES
                     TIMEOUT 30 LABELS cli WORKING_DIRECTORY ${CLI_WORK_DIR})

# 交互会话用例：从主菜单一路走到开局，覆盖错误输入重试与中途 EOF。
# 使用独立工作目录，避免与其它 CLI 用例共享 einstein_save.db 而产生跨用例干扰。
set(CLI_SESSION_WORK_DIR ${CMAKE_CURRENT_BINARY_DIR}/cli_session)
file(MAKE_DIRECTORY ${CLI_SESSION_WORK_DIR})
add_test(NAME cli_session
         COMMAND ${CMAKE_COMMAND}
                 -DEXE=$<TARGET_FILE:einstein_cli>
                 -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/data/cli_session_input.txt
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_session_test.cmake
)
set_tests_properties(cli_session PROPERTIES
                     TIMEOUT 30 LABELS cli WORKING_DIRECTORY ${CLI_SESSION_WORK_DIR})
