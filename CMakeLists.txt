# 顶层构建脚本：定义全局编译选项，并按依赖顺序组装各子目录的 targets。
# 所有可执行文件统一输出到构建目录下的 bin/（如 build/bin）。
cmake_minimum_required(VERSION 3.20)

project(einstein_chess VERSION 1.0.0 LANGUAGES C CXX)

# 可选图形界面：默认开启，保留现有“一次构建得到全部产物”的习惯。
# -DEINSTEIN_BUILD_GUI=OFF 时完全不查找 Qt6，只构建 core/ai/storage 与 CLI/Botzone，
# 方便在没有 Qt 的服务器或只跑无界面回归的环境里使用。
option(EINSTEIN_BUILD_GUI "Build the Qt6 GUI" ON)

# 全项目统一 C++17，禁止编译器私有扩展，保证不同平台行为一致。
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# SQLite 以 amalgamation 源码形式内嵌在 third_party 下，编译成独立静态库。
add_subdirectory(third_party/sqlite)

# 公共告警选项以 INTERFACE 库下发：任何 target 链接它即继承同一套编译参数。
# -Werror 表示新增代码不允许引入任何告警；Debug 用 -Og -g，其余配置用 -O2。
# MSVC 不认 -Wall/-Wextra/-Wpedantic，必须换成 /W4 + /permissive-，否则 Windows
# 上直接用 MSVC 构建会失败。
add_library(einstein_warnings INTERFACE)
if(MSVC)
    target_compile_options(einstein_warnings INTERFACE /W4 /WX /permissive- $<$<CONFIG:Debug>:/Od /Zi> $<$<NOT:$<CONFIG:Debug>>:/O2>)
else()
    target_compile_options(einstein_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Werror $<$<CONFIG:Debug>:-Og -g> $<$<NOT:$<CONFIG:Debug>>:-O2>)
endif()

# 依赖顺序：core 无外部依赖；ai 依赖 core；storage 依赖 core + ai（存档要记录 AI 档位）；
# 可执行目标最后加入，其中 GUI 由 EINSTEIN_BUILD_GUI 控制。
add_subdirectory(src/core)
add_subdirectory(src/ai)
add_subdirectory(src/storage)

add_subdirectory(src/ui/cli)
# GUI 是唯一依赖 Qt6 的目录；关掉选项后 find_package(Qt6) 也不会被执行。
if(EINSTEIN_BUILD_GUI)
    add_subdirectory(src/ui/gui)
endif()
add_subdirectory(src/ui/botzone)

# 回归测试：默认随构建一起编译，用 ctest 运行（见 tests/CMakeLists.txt）。
# 允许通过 -DEINSTEIN_BUILD_TESTS=OFF 关掉，方便只关心产物的打包场景。
option(EINSTEIN_BUILD_TESTS "Build regression tests" ON)
if(EINSTEIN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
