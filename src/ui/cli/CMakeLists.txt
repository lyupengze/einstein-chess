# einstein_cli：命令行版可执行目标，依赖 core + storage + ai。
# 不依赖 Qt，可直接在终端运行。
add_executable(einstein_cli main_cli.cpp)
target_link_libraries(einstein_cli PRIVATE einstein_core einstein_storage einstein_ai)
target_link_libraries(einstein_cli PRIVATE einstein_warnings)