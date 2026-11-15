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

target_include_directories(einstein_gui PRIVATE .)

target_link_libraries(einstein_gui PRIVATE
    einstein_core einstein_ai einstein_storage einstein_warnings Qt6::Widgets
)

set_target_properties(einstein_gui PROPERTIES AUTOMOC ON AUTORCC ON)