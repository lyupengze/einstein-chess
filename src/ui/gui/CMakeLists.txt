find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(einstein_gui
    main_gui.cpp
    main_window.cpp
    board_widget.cpp
)
target_link_libraries(einstein_gui PRIVATE
    einstein_core einstein_ai einstein_storage einstein_warnings Qt6::Widgets
)
set_target_properties(einstein_gui PROPERTIES AUTOMOC ON)


add_custom_command(TARGET einstein_gui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/style.qss"
    "$<TARGET_FILE_DIR:einstein_gui>/style.qss"
)