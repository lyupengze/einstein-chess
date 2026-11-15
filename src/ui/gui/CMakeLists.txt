# einstein_gui：Qt6 Widgets 图形界面可执行目标。
# 源文件按 视图（views）/ 组件（components）/ 弹窗（dialogs）分层组织，
# resources.qrc 内嵌样式表等资源。
find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(einstein_gui
    main_gui.cpp
    main_window.cpp

    views/menu_widget.cpp
    views/game_widget.cpp

    components/board_widget.cpp
    components/dice_widget.cpp
    components/toast_widget.cpp

    dialogs/setup_dialog.cpp
    dialogs/save_load_dialog.cpp

    resources.qrc
)

# 把本目录加入 include 路径，源码里才能按 "views/xxx.h" 等相对路径包含。
target_include_directories(einstein_gui PRIVATE .)

target_link_libraries(einstein_gui PRIVATE
    einstein_core einstein_ai einstein_storage einstein_warnings Qt6::Widgets
)

# Qt 的元对象与资源编译需要显式开启：
# AUTOMOC 处理带 Q_OBJECT 的类，AUTORCC 把 .qrc 编译进可执行文件。
set_target_properties(einstein_gui PROPERTIES AUTOMOC ON AUTORCC ON)