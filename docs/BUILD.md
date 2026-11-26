# 构建、测试与环境说明

本文是完整的构建参考。如果只想尽快把程序跑起来，先看 [使用说明](../README.md) 的“快速开始”一节；需要排查 Qt6、生成器、并行度或测试问题时再回到这里。

## 环境依赖

- **编译器**：任意支持 C++17 的编译器（GCC / Clang / MSVC；开发环境实测 GCC 16.2.0）
- **构建工具**：CMake ≥ 3.20（开发环境实测 4.4.3）
- **第三方库**：
  - `Qt6`：仅 GUI 需要；GUI 默认开启，配置时加 `-DEINSTEIN_BUILD_GUI=OFF` 可完全不查找 Qt6，只构建 CLI、Botzone 与无界面测试
  - `SQLite3`：已集成 amalgamation 源码（3.54.1），无需外部安装
  - `nlohmann/json`：单头文件（3.12.0），已内置，仅 Botzone 适配层使用

**平台支持**：core、AI、storage、CLI 与 Botzone 都是可移植的 C++17 代码；唯一的平台分支在 CLI 启动时把 Windows 控制台切到 UTF-8 代码页（`src/ui/cli/main_cli.cpp` 中的 `#ifdef _WIN32`，使用 `SetConsoleOutputCP` / `SetConsoleCP`）。GUI 依赖 Qt6。SQLite 与 JSON 均为可移植的 amalgamation/单头文件源码，因此工程在源码层面不排斥 Linux、macOS 或 MSVC，任何满足 C++17 的编译器与 Qt6 环境都可尝试构建。需要说明的是，当前仓库只在 Windows + MSYS2 UCRT64（GCC 16.2.0 / Qt 6.11.2）上实测过，其他平台尚未验证；GitHub Actions 也只在同一环境上跑完整与无 GUI 两套构建，并额外跑 clang-format 检查。

## 从零构建

```bash
# 1. 配置（单配置生成器用 -DCMAKE_BUILD_TYPE；VS / Xcode 等多配置生成器改用 --config）
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
# 2. 构建（并发数按机器核数/内存调整）
cmake --build build --parallel 8
# 3. 运行回归测试（可选，测试目标默认随构建一起编译）
ctest --test-dir build --output-on-failure
```

> 没有 Qt6、只需要 CLI 或 Botzone 时，配置阶段加 `-DEINSTEIN_BUILD_GUI=OFF`；此时不查找 Qt6，GUI 相关测试也不会注册。
>
> ```bash
> cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DEINSTEIN_BUILD_GUI=OFF
> ```
>
> 配好后仍用上面的 `cmake --build` 与 `ctest` 命令构建、测试。

> 可执行文件统一输出到 `build/bin/`，共四个可执行程序目标：`einstein_gui`、`einstein_cli`、`einstein_botzone`、`einstein_benchmark`（Windows 下带 `.exe` 后缀）；回归测试的可执行文件也输出到同一目录。

## 无 GUI 构建

没有 Qt6、或只想验证核心逻辑时，可以完全不构建 GUI：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DEINSTEIN_BUILD_GUI=OFF
cmake --build build --parallel 8
ctest --test-dir build --output-on-failure
```

此时 `einstein_gui`、GUI 静态库与 5 个 GUI 用例都不会注册，默认运行其余 14 个；用例清单以 `tests/CMakeLists.txt` 为准。

## 让 CMake 找到 Qt6

`find_package(Qt6)` 会搜索默认前缀、`PATH`、`CMAKE_PREFIX_PATH` 等位置，Qt6 装在这些位置之外时配置会直接失败。可任选一种处理：

- 配置前把 Qt 的 `bin` 目录加入 `PATH`。Windows 上还需注意 `moc`/`rcc` 能加载 Qt 运行时 DLL，否则 `AUTORCC` 的测试执行会以 `0xc0000139` 中断，生成阶段随之中断；
- 或显式指定前缀：`cmake -S . -B build -DCMAKE_PREFIX_PATH=<Qt6 安装前缀>`。

## 构建类型、生成器与并行度

> **构建类型**：Ninja、MinGW Makefiles、Unix Makefiles 都是单配置生成器，`--config Release` 不会生效，构建类型只能在配置阶段用 `-DCMAKE_BUILD_TYPE` 指定；Visual Studio、Xcode 则是多配置生成器，用 `--config <Config>`。留空时走 `einstein_warnings` 中的 `-O2` 分支。Windows 下 `einstein_gui` 的子系统随配置变化：Release 使用 Windows GUI 子系统（双击不附带控制台窗口），Debug 保留 Console 子系统（便于查看日志）。这由 `src/ui/gui/CMakeLists.txt` 中的 `WIN32_EXECUTABLE "$<CONFIG:Release>"` 控制。

> **生成器**：不写 `-G` 时 CMake 会挑默认生成器，可能与手上的 Qt6 不匹配（例如 Windows 上装了 Visual Studio，却要用 MSYS2 的 GCC 配 UCRT64 版 Qt6）——此时用 `-G "MinGW Makefiles"` 显式指定。只想产出可执行文件、不编译测试时，配置时加 `-DEINSTEIN_BUILD_TESTS=OFF`；想在没有 Qt6 的环境里只构建 CLI/Botzone，加 `-DEINSTEIN_BUILD_GUI=OFF`。

**并行度**：`--parallel 8` 里的 `8` 只是示例，按 CPU 核数与内存调整。不要写裸 `-j`：GNU Make 会把无参数的 `-j` 解释为不限制并发，可能瞬间启动大量编译进程。

## 运行环境

- `einstein_gui` 动态链接 `Qt6Core`/`Qt6Gui`/`Qt6Widgets`，运行时必须能加载这些库：
  - Windows：把 Qt 的 `bin` 目录加入 `PATH`，或用 `windeployqt` 把依赖收集到可执行文件同目录；
  - Linux：使用发行版提供的 Qt6 运行时包，或通过 `LD_LIBRARY_PATH`、rpath 指向 Qt 库目录；
  - macOS：用 `macdeployqt` 打包，或设置 `DYLD_LIBRARY_PATH`。
- `einstein_cli` 与 `einstein_botzone` 不依赖 Qt。若在 Windows 上以 MinGW 动态运行时构建，需保证 `libstdc++-6.dll`、`libgcc_s_seh-1.dll` 可被加载（把工具链的 `bin` 加入 `PATH`，或用 `-static-libgcc -static-libstdc++` 静态链接）；Linux/macOS 通常无需额外处理。
- 存档路径、可写目录与单实例限制属于运行行为，见 [使用说明](USAGE.md) 的“存档位置”一节。

## 测试

默认构建注册 19 个用例（清单、标签与超时以 `tests/CMakeLists.txt` 为准），测试可执行文件与产品可执行文件一样输出到 `build/bin/`，并由 CTest 在各自的工作目录中运行：

| 用例                | 覆盖内容                                   | 标签      | 超时（秒） |
| ------------------- | ------------------------------------------ | --------- | ---------: |
| `board_geometry`    | 棋盘几何，右/下余数像素不映射到格子        | `gui`     |         60 |
| `ai_random`         | AI 随机下限与固定种子可复现                | `ai`      |         30 |
| `core_rules`        | 选子、走法、胜负、布阵与对局生命周期       | `core`    |         60 |
| `ai_search`         | 搜索合法性、战术与确定性                   | `ai`      |        120 |
| `ai_level`          | 档位选择、默认档与随机档可复现             | `ai`      |         30 |
| `botzone_protocol`  | 协议库的布阵/走子/严格输入校验             | `botzone` |         60 |
| `save_level`        | 存档档位必填、严格解析与损坏拒绝           | `storage` |         60 |
| `storage_roundtrip` | 局面完整往返与语义校验                     | `storage` |         60 |
| `state_validation`  | 局面状态校验                               | `core`    |         30 |
| `game_state_guard`  | core 状态机守卫                            | `core`    |         30 |
| `gui_reconcile`     | GUI 状态收敛，上一局排队的定时器不落到新局 | `gui`     |         60 |
| `toast_fade`        | Toast 淡入淡出                             | `gui`     |         60 |
| `dice_widget`       | 骰子点数绘制与非法值错误标记               | `gui`     |         60 |
| `gui_dialogs`       | 布阵与存档对话框、同名覆盖确认             | `gui`     |         60 |
| `cli_input`         | CLI 参数与整行数字解析边界                 | `cli`     |         30 |
| `cli_eof`           | EOF 处理                                   | `cli`     |         30 |
| `cli_exit`          | 退出菜单                                   | `cli`     |         30 |
| `cli_session`       | 菜单交互与中途 EOF                         | `cli`     |         30 |
| `cli_broken_db`     | 坏存档库错误边界                           | `cli`     |         30 |

- 超时是防挂死的上限，默认 60 秒；`cli_*` 与 `ai_*` 用例按实际耗时缩短或放宽。
- `save_level`、`storage_roundtrip`、`gui_dialogs` 与 `cli_*` 各自在独立工作目录中运行，避免互相覆盖存档库。
- GUI 用例自动使用 `QT_QPA_PLATFORM=offscreen`，不需要真实显示器；`-DEINSTEIN_BUILD_GUI=OFF` 时这 5 个用例不注册，其余 14 个仍可运行。
- 按标签筛选示例：`ctest --test-dir build -L core`、`ctest --test-dir build -L gui`。
- 测试覆盖有限的原因与取舍见 [设计取舍](DESIGN.md) 与 [使用说明](USAGE.md)。

## CI

GitHub Actions 工作流位于 `.github/workflows/ci.yml`：

- `windows-ucrt64`：Windows + MSYS2 UCRT64，安装 GCC、CMake、Ninja 与 Qt6，完整构建并跑 19 个测试；
- `windows-ucrt64-nogui`：同一环境但不装 Qt，`-DEINSTEIN_BUILD_GUI=OFF`，跑 14 个测试；
- `format`：Ubuntu + Python，固定 `clang-format==23.1.1`，对 `src/`、`tests/`、`benchmarks/` 下受跟踪的 C++ 文件做 `--dry-run --Werror`；
- `docs`：Ubuntu + CMake，运行 `cmake -P tools/check_repo.cmake`，检查文件头、禁词、缺陷锚点、相对链接与测试数口径。

测试数量以 `tests/CMakeLists.txt` 为准；CI 的 Qt 版本由 MSYS2 包管理器决定，不固定小版本。

触发条件为推送到 `dev`/`main`、向这两个分支发起 Pull Request，以及手动 `workflow_dispatch`。构建或测试失败时会尝试上传 `build/Testing/Temporary/LastTest.log` 作为 artifact。

## 本地自检

提交前在仓库根依次执行：

```bash
cmake -P tools/check_repo.cmake
cmake --build build --parallel 8
ctest --test-dir build --output-on-failure
clang-format --dry-run --Werror <改动过的 .h/.cpp>
```

`check_repo.cmake` 只依赖 CMake 本身，与 CI 的 `docs` job 是同一条命令；它不注册 CTest，也不写构建产物。

## 常见构建问题

- **配置时报找不到 Qt6**：确认 Qt6 的 `bin` 在 `PATH`，或用 `-DCMAKE_PREFIX_PATH` 指向 Qt6 安装前缀；装了 MSVC 版 Qt6 却用 MinGW 编译时，CMake 可能找到不匹配的包，应显式使用 UCRT64 的 Qt6。
- **`moc`/`rcc` 以 `0xc0000139` 退出**：Qt 工具本身找不到运行时 DLL，把 Qt 的 `bin` 加入 `PATH`，或补齐运行库。
- **`--config Release` 没生效**：当前是单配置生成器（Ninja/MinGW Makefiles/Unix Makefiles），改用 `-DCMAKE_BUILD_TYPE=Release`。
- **Release 双击没有控制台**：这是 Windows GUI 子系统的预期行为；需要日志时构建 Debug，或用终端启动 Release 可执行文件。
- **测试没有生成**：确认配置时没有加 `-DEINSTEIN_BUILD_TESTS=OFF`；测试默认随构建一起编译。
- **并行构建把机器压满**：把 `--parallel 8` 调整为更小的值，避免裸 `-j`。
