# ♟️ 爱因斯坦棋 (Einstein Chess)

<div align="center">

![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg?style=flat-square&logo=c%2B%2B)
![Qt](https://img.shields.io/badge/Qt-6.11.2-green.svg?style=flat-square&logo=qt)
![SQLite](https://img.shields.io/badge/SQLite-3.54.1-blue.svg?style=flat-square&logo=sqlite)
![nlohmann/json](https://img.shields.io/badge/nlohmann%2Fjson-3.12.0-ff69b4.svg?style=flat-square)
![CMake](https://img.shields.io/badge/CMake-3.20+-red.svg?style=flat-square&logo=cmake)
![License](https://img.shields.io/badge/License-MIT-yellow.svg?style=flat-square)
![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg?style=flat-square)

**基于 C++17 与 Qt6 的 5×5 爱因斯坦棋人机对弈程序**

提供 GUI、CLI 与 Botzone 对战平台三个入口，支持图形化布阵、走法高亮提示，以及基于 SQLite 的本地多存档。

</div>

## 🛠️ 环境依赖与编译说明

- **编译器**：任意支持 C++17 的编译器（GCC / Clang / MSVC；开发环境实测 GCC 16.2.0）
- **构建工具**：CMake ≥ 3.20（开发环境实测 4.4.3）
- **第三方库**：
  - `Qt6`：仅 GUI 需要，但顶层 `CMakeLists.txt` 无条件构建 GUI，`find_package(Qt6 ...)` 为 REQUIRED——缺少 Qt6 时整个工程无法配置（即使只想构建 CLI）
  - `SQLite3`：已集成 amalgamation 源码（3.54.1），无需外部安装
  - `nlohmann/json`：单头文件（3.12.0），已内置，仅 Botzone 适配层使用

**平台支持**：源码不含任何平台特定 API，唯一的平台分支是 CLI 启动时把 Windows 控制台切到 UTF-8 代码页（`src/ui/cli/main_cli.cpp` 中的 `#ifdef _WIN32`）；SQLite 与 JSON 均为可移植的 amalgamation/单头文件源码。因此工程在源码层面不排斥 Linux、macOS 或 MSVC，任何满足 C++17 的编译器与 Qt6 环境都可尝试构建。需要说明的是，当前仓库只在 Windows + MSYS2 UCRT64（GCC 16.2.0 / Qt 6.11.2）上实测过，其他平台尚未验证，也没有 CI。

### 编译方法

```bash
# 1. 配置（单配置生成器用 -DCMAKE_BUILD_TYPE；VS / Xcode 等多配置生成器改用 --config）
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
# 2. 构建
cmake --build build -j
# 3. 运行回归测试（可选，测试目标默认随构建一起编译）
ctest --test-dir build --output-on-failure
```

> 可执行文件统一输出到 `build/bin/`，共三个可执行程序目标：`einstein_gui`、`einstein_cli`、`einstein_botzone`（Windows 下带 `.exe` 后缀）；回归测试的可执行文件也输出到同一目录。

**让 CMake 找到 Qt6**：`find_package(Qt6)` 会搜索默认前缀、`PATH`、`CMAKE_PREFIX_PATH` 等位置，Qt6 装在这些位置之外时配置会直接失败。可任选一种处理：

- 配置前把 Qt 的 `bin` 目录加入 `PATH`。Windows 上还需注意 `moc`/`rcc` 能加载 Qt 运行时 DLL，否则 `AUTORCC` 的测试执行会以 `0xc0000139` 中断，生成阶段随之中断；
- 或显式指定前缀：`cmake -S . -B build -DCMAKE_PREFIX_PATH=<Qt6 安装前缀>`。

> **构建类型**：Ninja、MinGW Makefiles、Unix Makefiles 都是单配置生成器，`--config Release` 不会生效，构建类型只能在配置阶段用 `-DCMAKE_BUILD_TYPE` 指定；Visual Studio、Xcode 则是多配置生成器，用 `--config <Config>`。留空时走 `einstein_warnings` 中的 `-O2` 分支。

> **生成器**：不写 `-G` 时 CMake 会挑默认生成器，可能与手上的 Qt6 不匹配（例如 Windows 上装了 Visual Studio，却要用 MSYS2 的 GCC 配 UCRT64 版 Qt6）——此时用 `-G "MinGW Makefiles"` 显式指定。只想产出可执行文件、不编译测试时，配置时加 `-DEINSTEIN_BUILD_TESTS=OFF`。

## 🎮 操作说明

### 运行前提

- `einstein_gui` 动态链接 `Qt6Core`/`Qt6Gui`/`Qt6Widgets`，运行时必须能加载这些库：
  - Windows：把 Qt 的 `bin` 目录加入 `PATH`，或用 `windeployqt` 把依赖收集到可执行文件同目录；
  - Linux：使用发行版提供的 Qt6 运行时包，或通过 `LD_LIBRARY_PATH`、rpath 指向 Qt 库目录；
  - macOS：用 `macdeployqt` 打包，或设置 `DYLD_LIBRARY_PATH`。
- `einstein_cli` 与 `einstein_botzone` 不依赖 Qt。若在 Windows 上以 MinGW 动态运行时构建，需保证 `libstdc++-6.dll`、`libgcc_s_seh-1.dll` 可被加载（把工具链的 `bin` 加入 `PATH`，或用 `-static-libgcc -static-libstdc++` 静态链接）；Linux/macOS 通常无需额外处理。
- 存档写入 `einstein_save.db`（相对路径，见 `storage::kDefaultDbPath`）。GUI 启动时会执行 `QDir::setCurrent(applicationDirPath())`，把工作目录切到可执行文件所在目录，存档因此与之同目录；CLI 不切换工作目录，存档跟随启动目录。两者工作目录相同时才会共用同一份存档。

### GUI 版本 (推荐)

1. 启动 `./build/bin/einstein_gui`（Windows 下为 `einstein_gui.exe`）。
2. 点击“开始新游戏”，在弹窗中选择阵营（红方先手），再点击格子循环切换 1~6 号棋子完成布阵。
3. 掷骰后，可选棋子会以绿色虚线框标出；点击选中后边框变为金色实线，同时显示所有可达落点。
4. 对局中可随时点击“保存”，输入存档名后存档；保存成功等提示会以淡入淡出的浮层短暂显示。

### CLI 版本 (备用)

1. 在终端运行 `./build/bin/einstein_cli`（Windows 下为 `einstein_cli.exe`）。
2. 按菜单提示输入指令（1 开始游戏 / 2 读档 / 3 退出），走法格式如 `0 2 1 2`。

### Botzone 版本 (对战平台)

无状态单次调用适配层：从 stdin 读取**一行** JSON 请求，向 stdout 写出**一行** JSON 响应后退出。不链接 `storage`，也不写任何文件。

以下示例按 POSIX shell 书写（Windows 下把可执行文件名换成 `einstein_botzone.exe`，管道与 JSON 内容不变）：

```bash
# 布阵：每条 request 由裁判给出 color 与 region（这里以蓝方为例）
echo '{"requests":[{"phase":"layout","color":1,"region":[[4,4],[4,3],[4,2],[3,4],[3,3],[2,4]]}],"responses":[]}' \
  | ./build/bin/einstein_botzone
# {"response":{"layout":[2,5,1,6,3,4]}}            ← 布阵仍为随机排列，每次不同

# 走子：蓝方（color=1）掷出 4 点，初始局面下 4 号棋子在 (3,4)
echo '{"requests":[{"phase":"move","color":1,"dice":4,"board":[[1,2,3,0,0],[4,5,0,0,0],[6,0,0,0,-6],[0,0,0,-5,-4],[0,0,-3,-2,-1]]}],"responses":[]}' \
  | ./build/bin/einstein_botzone
# {"response":{"x0":3,"x1":3,"y0":4,"y1":3}}        ← 由 Expectiminimax 搜索选择，合法落点之一

# 骰点降级：红方掷出 3 点但 3 号不在场，按规则降级为可选 1 号
echo '{"requests":[{"phase":"move","color":0,"dice":3,"board":[[1,0,0,0,0],[0,0,0,0,0],[0,0,0,0,0],[0,0,0,0,0],[0,0,0,0,-1]]}],"responses":[]}' \
  | ./build/bin/einstein_botzone
# {"response":{"x0":0,"x1":1,"y0":0,"y1":1}}        ← 由 Expectiminimax 搜索选择，合法落点之一
```

- 输入：`requests` 为完整历史，适配层只取最后一条。平台不会给 Bot 下发顶层 `initdata`；本方阵营来自该条 request 的 `color` 字段（0 = 红方先手，1 = 蓝方）。`requests` 缺失或为空、以及 `phase` 为 `layout` 时返回布阵（布阵与颜色无关，按裁判 `region` 顺序落位，与 core 的初始区逐格一致）；`phase` 为 `move` 时，依据 `dice` 与 5×5 `board`（外层行 x、内层列 y，正数红方 1~6、负数蓝方 1~6、0 空格）返回走法；`board` 不足 5×5 时，缺失格按空位处理；其他 `phase` 返回空 `response`。
- 输出：`response.layout`（6 个编号）或 `response.x0/y0/x1/y1`。请求非法时（JSON 解析失败、`dice` 缺失或不在 1~6、`board` 缺失或类型错误、`color` 不是 0/1、棋子编号越界或重复），stderr 打印 `Bot error: ...`，stdout 输出 `{"response":{}}`，进程退出码仍为 0。

## 📂 项目结构

```text
.
├── src/
│   ├── core/            # 核心规则（纯 C++，零外部依赖）
│   │   ├── types.h          # Pos / Side / Piece
│   │   ├── board.h|.cpp     # 5×5 棋盘，每方 6 枚棋子
│   │   ├── game_state.h     # GameState 与 Phase 状态机
│   │   ├── rules.h|.cpp     # 可选棋子、走法校验、胜负判定
│   │   └── game.h|.cpp      # 对局门面：提交布阵、掷骰、走子、换手
│   ├── ai/              # 两代 AI（随机基线 / 竞速胜率 Expectiminimax），仅依赖 core
│   ├── storage/         # save_manager 存档序列化 + sqlite_wrapper RAII 封装
│   └── ui/              # 三个并列入口，各自独立可执行目标
│       ├── gui/             # Qt6：main_window + views/ + components/ + dialogs/ + style.qss
│       ├── cli/             # 终端菜单与对局循环
│       └── botzone/         # Botzone stdin/stdout JSON 适配层
├── third_party/
│   ├── sqlite/          # amalgamation 3.54.1（sqlite3.c 随仓库提供）
│   └── nlohmann/        # json.hpp 单头文件 3.12.0
└── CMakeLists.txt       # C++17、统一启用 -Werror，产物输出到 build/bin
```

### 构建目标与依赖

| 目标               | 类型   | 依赖                                                         |
| ------------------ | ------ | ------------------------------------------------------------ |
| `einstein_core`    | 静态库 | 无                                                           |
| `einstein_ai`      | 静态库 | core                                                         |
| `einstein_storage` | 静态库 | core、`sqlite3`（内部封装，不外泄 `<sqlite3.h>`）            |
| `einstein_cli`     | 可执行 | core、ai、storage                                            |
| `einstein_gui_lib` | 静态库 | core、ai、storage、`Qt6::Widgets`；GUI 逻辑供 exe 与测试共享 |
| `einstein_gui`     | 可执行 | `einstein_gui_lib`、`Qt6::Widgets`、`resources.qrc`          |
| `einstein_botzone` | 可执行 | core、ai + `nlohmann/json`（不接存档，保持无状态）           |

各库通过 `target_include_directories(... PUBLIC ..)` 将 `src/` 暴露为公共头文件目录，因此跨模块 include 统一写作 `core/game.h`、`ai/ai.h` 这类限定路径。顶层另定义 INTERFACE 目标 `einstein_warnings`（`-Wall -Wextra -Wpedantic -Wshadow -Werror`），`src/` 下的每个目标都必须链接它。

## ⚠️ 当前限制

- **AI 提供两档强度**：公共接口用 `ai::Level` 选择版本，`ai::kDefaultLevel` 当前指向最新版 `Level::kSearch`，因此 `ai::decide_move(state)` 就是搜索版。`Level::kRandom` 复用第一版随机基线（在合法着法中均匀随机），用于最低难度或对照；显式指定时写作 `ai::decide_move(state, ai::Level::kRandom)`。搜索版使用固定深度的 Expectiminimax（默认 4 回合），叶节点用 46656 个单方竞速状态预计算出的到达时间分布合成胜率，**不设材料价值项**——吃对方子、送己方子都只通过竞速分布影响胜率；不含 alpha-beta、置换表与时间预算。同分时先按“走完后离终点更近”过滤，再优先斜向，最后才随机，避免无进展的轴向着法。`decide_layout()` 两版共用同一套随机打乱，因此不接受 Level。更深的搜索或布局优化留待后续，UI 暂未暴露强度选择。
- **验证平台单一**：源码可移植，但构建与运行只在 Windows + MSYS2 UCRT64（GCC 16.2.0 / Qt 6.11.2）上实测过，Linux 与 macOS 未经构建验证，也没有 CI，`-Werror` 下其他编译器的告警差异可能带来额外修改。
- **测试覆盖有限**：CTest 目前有 8 个用例——`board_geometry`（棋盘几何）、`ai_random`（AI 随机下限）、`core_rules`（合法着法枚举与走子结算）、`ai_search`（搜索合法性、战术与确定性）、`ai_level`（版本选择、默认最新版与随机版可复现）、`gui_reconcile`（GUI 状态收敛与旧定时器不落到新局）、`toast_fade`（Toast 淡入淡出）、`cli_eof`（CLI 的 EOF 处理）；存档与 GUI 的完整分支仍未全部覆盖。
- **Toast 是绝对定位的浮层**：位置只在 `show_message()` 时计算一次，父窗口缩放后提示条会停在旧坐标，直到下一次提示；这是有意取舍（提示条短命，不值得为它监听父控件尺寸变化）。
- **Windows 下 GUI 附带控制台窗口**：`einstein_gui` 未标记 `WIN32` 子系统，启动时会多出一个控制台窗口，不影响功能。

## 👥 作者与分工

- **Spamton NEO**：独立完成
