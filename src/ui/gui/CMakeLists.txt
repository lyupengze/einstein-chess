# einstein_gui：Qt6 Widgets 图形界面。
# 源文件按 视图（views）/ 组件（components）/ 弹窗（dialogs）分层组织，
# resources.qrc 内嵌样式表等资源。
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# 界面逻辑单独做成静态库：可执行文件与 tests/ 下的回归测试共用同一份对象，
# 测试不必重复编译一遍界面源码（Q_OBJECT 的 moc 也只生成一次）。
add_library(einstein_gui_lib STATIC
    main_window.cpp

    views/menu_widget.cpp
    views/game_widget.cpp

    components/board_widget.cpp
    components/dice_widget.cpp
    components/toast_widget.cpp

    dialogs/setup_dialog.cpp
    dialogs/save_load_dialog.cpp
)

# 把本目录加入 include 路径，源码里才能按 "views/xxx.h" 等相对路径包含。
# PUBLIC：链接本库的测试同样需要这些头文件。
target_include_directories(einstein_gui_lib PUBLIC .)

# 版本号只在 CMake 里维护，菜单页通过编译定义读取，避免源码里再硬编码一份。
target_compile_definitions(einstein_gui_lib PRIVATE EINSTEIN_VERSION="${PROJECT_VERSION}")

target_link_libraries(einstein_gui_lib
    PUBLIC einstein_core einstein_ai einstein_storage Qt6::Widgets
    PRIVATE einstein_warnings
)

# AUTOMOC 处理带 Q_OBJECT 的类。资源不放这里：静态库里的 rcc 对象可能因为
# 没有被引用而在链接时被丢弃，样式表就会静默失效，因此 .qrc 留在可执行文件一侧。
set_target_properties(einstein_gui_lib PROPERTIES AUTOMOC ON)

add_executable(einstein_gui main_gui.cpp resources.qrc)

target_link_libraries(einstein_gui PRIVATE einstein_gui_lib einstein_warnings)

# AUTORCC 把 .qrc 编译进可执行文件本身，保证资源一定被注册。
set_target_properties(einstein_gui PROPERTIES AUTOMOC ON AUTORCC ON)

# Release 使用 Windows GUI 子系统，双击时不再附带控制台窗口；
# Debug 仍保持 Console 子系统，便于查看日志和调试输出。
# 生成器表达式让单配置（MinGW Makefiles）与多配置生成器都能正确判断配置。
set_target_properties(einstein_gui PROPERTIES WIN32_EXECUTABLE "$<CONFIG:Release>")
