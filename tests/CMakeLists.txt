# 回归测试：把报告里"可稳定复现"且能自动化的缺陷固化下来。
# 图形用例统一在 offscreen 平台上跑，无显示器的 CI 环境同样可用。

add_executable(test_board_geometry board_geometry_test.cpp)
target_link_libraries(test_board_geometry PRIVATE einstein_gui_lib einstein_warnings)
add_test(NAME board_geometry COMMAND test_board_geometry)

add_executable(test_ai_random ai_random_test.cpp)
target_link_libraries(test_ai_random PRIVATE einstein_ai einstein_warnings)
add_test(NAME ai_random COMMAND test_ai_random)

add_executable(test_gui_reconcile gui_reconcile_test.cpp)
target_link_libraries(test_gui_reconcile PRIVATE einstein_gui_lib einstein_warnings)
add_test(NAME gui_reconcile COMMAND test_gui_reconcile)

add_executable(test_toast_fade toast_fade_test.cpp)
target_link_libraries(test_toast_fade PRIVATE einstein_gui_lib einstein_warnings)
add_test(NAME toast_fade COMMAND test_toast_fade)

# Qt 的 offscreen 平台插件不需要显示器；用环境变量而不是命令行参数，
# 这样测试程序本身保持与真实 GUI 相同的启动路径。
set_tests_properties(board_geometry gui_reconcile toast_fade
                     PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# CLI 的 EOF 回归：用 cmake -P 脚本喂文件给二进制，
# 避免依赖各平台不同的管道/重定向语法。
add_test(NAME cli_eof
         COMMAND ${CMAKE_COMMAND}
                 -DEXE=$<TARGET_FILE:einstein_cli>
                 -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/data/eof_input.txt
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_eof_test.cmake
)
