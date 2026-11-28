# 回归测试：覆盖核心规则、状态校验、存储、CLI、Botzone 与 GUI 的可自动化断言。
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

# GUI 用例依赖 einstein_gui_lib；-DEINSTEIN_BUILD_GUI=OFF 时整个目录不会被加入，
# 这里必须跳过，否则测试配置阶段会引用不存在的 target。
if(EINSTEIN_BUILD_GUI)
    add_executable(test_board_geometry board_geometry_test.cpp)
    target_link_libraries(test_board_geometry PRIVATE einstein_gui_lib einstein_warnings)
    # Qt 的 offscreen 平台插件不需要显示器；RESOURCE_LOCK 让 GUI 用例串行，
    # 避免并行时互相抢 CPU 把动画/定时器断言拖到超时。
    einstein_add_test(board_geometry test_board_geometry
                      TIMEOUT 60 LABEL gui
                      ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
                      RESOURCE_LOCK qt_gui)
endif()

add_executable(test_ai_random ai_random_test.cpp)
target_link_libraries(test_ai_random PRIVATE einstein_ai einstein_warnings)
einstein_add_test(ai_random test_ai_random TIMEOUT 30 LABEL ai)

add_executable(test_core_rules core_rules_test.cpp)
target_link_libraries(test_core_rules PRIVATE einstein_core einstein_warnings)
einstein_add_test(core_rules test_core_rules TIMEOUT 60 LABEL core)

add_executable(test_state_validation state_validation_test.cpp)
target_link_libraries(test_state_validation PRIVATE einstein_core einstein_warnings)
einstein_add_test(state_validation test_state_validation TIMEOUT 30 LABEL core)

add_executable(test_game_state_guard game_state_guard_test.cpp)
target_link_libraries(test_game_state_guard PRIVATE einstein_core einstein_warnings)
einstein_add_test(game_state_guard test_game_state_guard TIMEOUT 30 LABEL core)

add_executable(test_game_seed game_seed_test.cpp)
target_link_libraries(test_game_seed PRIVATE einstein_core einstein_warnings)
einstein_add_test(game_seed test_game_seed TIMEOUT 30 LABEL core)

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
# 测试要直接改 SQLite 里的 data 列来伪造缺档位的数据，因此显式链接 sqlite3；
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

# 参数绑定失败要用越界下标触发，测试直接覆盖 storage 的 SQLite 封装。
add_executable(test_storage_bind storage_bind_test.cpp)
target_link_libraries(test_storage_bind PRIVATE einstein_storage sqlite3 einstein_warnings)
set(STORAGE_BIND_WORK_DIR ${CMAKE_CURRENT_BINARY_DIR}/storage_bind)
file(MAKE_DIRECTORY ${STORAGE_BIND_WORK_DIR})
einstein_add_test(storage_bind test_storage_bind
                  TIMEOUT 30 LABEL storage
                  WORKING_DIRECTORY ${STORAGE_BIND_WORK_DIR})

if(EINSTEIN_BUILD_GUI)
    add_executable(test_gui_reconcile gui_reconcile_test.cpp)
    target_link_libraries(test_gui_reconcile PRIVATE einstein_gui_lib einstein_warnings)
    einstein_add_test(gui_reconcile test_gui_reconcile
                      TIMEOUT 60 LABEL gui
                      ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
                      RESOURCE_LOCK qt_gui)

    # 状态栏与掷骰按钮的可操作性：无骰子时的文案兜底，以及掷骰失败后的按钮恢复。
    add_executable(test_gui_status gui_status_test.cpp)
    target_link_libraries(test_gui_status PRIVATE einstein_gui_lib einstein_warnings)
    einstein_add_test(gui_status test_gui_status
                      TIMEOUT 60 LABEL gui
                      ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
                      RESOURCE_LOCK qt_gui)

    add_executable(test_toast_fade toast_fade_test.cpp)
    target_link_libraries(test_toast_fade PRIVATE einstein_gui_lib einstein_warnings)
    einstein_add_test(toast_fade test_toast_fade
                      TIMEOUT 60 LABEL gui
                      ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
                      RESOURCE_LOCK qt_gui)

    # 骰子绘制：非法点数必须画出错误标记，不能留下白色空骰面。
    add_executable(test_dice_widget dice_widget_test.cpp)
    target_link_libraries(test_dice_widget PRIVATE einstein_gui_lib einstein_warnings)
    einstein_add_test(dice_widget test_dice_widget
                      TIMEOUT 60 LABEL gui
                      ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
                      RESOURCE_LOCK qt_gui)

    # 对话框用例：直接构造并操作控件，不进入模态循环。
    # 存档对话框按相对路径打开 einstein_save.db，因此必须有独立工作目录；
    # 显式链接 sqlite3，原因与 save_level 相同（storage 只 PRIVATE 依赖它）。
    add_executable(test_gui_dialogs gui_dialogs_test.cpp)
    target_link_libraries(test_gui_dialogs PRIVATE einstein_gui_lib sqlite3 einstein_warnings)
    set(GUI_DIALOGS_WORK_DIR ${CMAKE_CURRENT_BINARY_DIR}/gui_dialogs)
    file(MAKE_DIRECTORY ${GUI_DIALOGS_WORK_DIR})
    einstein_add_test(gui_dialogs test_gui_dialogs
                      TIMEOUT 60 LABEL gui
                      ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
                      RESOURCE_LOCK qt_gui
                      WORKING_DIRECTORY ${GUI_DIALOGS_WORK_DIR})

    # 样式表回归：QSS 编进测试可执行文件，加载后展开下拉列表，
    # 断言没有 QFont 字号警告（px 字号会让弹出层拿到 pointSize 为 -1 的字体）。
    add_executable(test_gui_style gui_style_test.cpp ${PROJECT_SOURCE_DIR}/src/ui/gui/resources.qrc)
    set_target_properties(test_gui_style PROPERTIES AUTORCC ON)
    target_link_libraries(test_gui_style PRIVATE einstein_gui_lib einstein_warnings)
    einstein_add_test(gui_style test_gui_style
                      TIMEOUT 60 LABEL gui
                      ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
                      RESOURCE_LOCK qt_gui)
endif()

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
                 -DDB=${CLI_WORK_DIR}/cli_exit.db
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

# CLI 异常边界：先生成一个 saves 表结构损坏的数据库，再让 CLI 选择“读档”。
# 辅助程序只负责造库，不注册为独立用例；它直接链接 sqlite3，避免 SaveManager
# 在构造时把表修好。
add_executable(make_broken_save_db make_broken_save_db.cpp)
target_link_libraries(make_broken_save_db PRIVATE sqlite3 einstein_warnings)
set(CLI_BROKEN_DB_WORK_DIR ${CMAKE_CURRENT_BINARY_DIR}/cli_broken_db)
file(MAKE_DIRECTORY ${CLI_BROKEN_DB_WORK_DIR})
add_test(NAME cli_broken_db
         COMMAND ${CMAKE_COMMAND}
                 -DEXE=$<TARGET_FILE:einstein_cli>
                 -DHELPER=$<TARGET_FILE:make_broken_save_db>
                 -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/data/cli_broken_db_input.txt
                 -DWORKDIR=${CLI_BROKEN_DB_WORK_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_broken_db_test.cmake
)
set_tests_properties(cli_broken_db PROPERTIES
                     TIMEOUT 30 LABELS cli WORKING_DIRECTORY ${CLI_BROKEN_DB_WORK_DIR})

# CLI 纯解析单测：不启动进程、不依赖 Qt/SQLite，专门覆盖参数与整行数字的边界。
add_executable(test_cli_input cli_input_test.cpp)
target_include_directories(test_cli_input PRIVATE ${CMAKE_SOURCE_DIR}/src/ui/cli)
target_link_libraries(test_cli_input PRIVATE einstein_warnings)
einstein_add_test(cli_input test_cli_input TIMEOUT 30 LABEL cli)
