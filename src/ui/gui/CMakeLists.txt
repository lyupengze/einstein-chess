find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(einstein_gui
    main_gui.cpp
    main_window.cpp
    board_widget.cpp
    dice_widget.cpp
    menu_widget.cpp
    game_widget.cpp
    toast_widget.cpp
    setup_dialog.cpp
    save_load_dialog.cpp
    resources.qrc
)

target_link_libraries(einstein_gui PRIVATE
    einstein_core einstein_ai einstein_storage einstein_warnings Qt6::Widgets
)

set_target_properties(einstein_gui PROPERTIES AUTOMOC ON AUTORCC ON)