# ♟️ 爱因斯坦棋 (Einstein Chess)

<div align="center">

![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg?style=flat-square&logo=c%2B%2B)
![Qt](https://img.shields.io/badge/Qt-6.11.2-green.svg?style=flat-square&logo=qt)
![SQLite](https://img.shields.io/badge/SQLite-3.54.1-blue.svg?style=flat-square&logo=sqlite)
![nlohmann/json](https://img.shields.io/badge/nlohmann%2Fjson-3.12.0-ff69b4.svg?style=flat-square)
![CMake](https://img.shields.io/badge/CMake-3.20+-red.svg?style=flat-square&logo=cmake)
![License](https://img.shields.io/badge/License-MIT-yellow.svg?style=flat-square)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey.svg?style=flat-square&logo=windows)

**基于 C++17 与 Qt6 实现的 5x5 爱因斯坦棋人机对弈程序**

包含 GUI、CLI 与 Botzone 对战平台三个入口，支持图形化布阵、智能高亮提示以及基于 SQLite 的本地多存档系统。

</div>

## 🛠️ 环境依赖与编译说明

- **编译器**: GCC 16.2.0 (或任意支持 C++17 的编译器)
- **构建工具**: CMake ≥ 3.20 (顶层声明的下限，开发环境实测 4.4.3)
- **第三方库**:
  - `Qt6` (仅 GUI 需要，`find_package` 为 REQUIRED：缺少 Qt6 时整个工程无法配置)
  - `SQLite3` (已集成 amalgamation 源码，无需外部安装)
  - `nlohmann/json` (单头文件，已内置，仅 Botzone 适配层使用)

### 编译方法

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
```

_(注：可执行文件位于 `build/bin/`，共 `einstein_gui.exe` / `einstein_cli.exe` / `einstein_botzone.exe` 三个目标)_

> **PATH 要求**：配置阶段会试跑 Qt 的 `moc`/`rcc`，需先让其能解析到 Qt 运行时 DLL——把 `D:\msys64\ucrt64\bin` 加入 `PATH`，否则 `AUTORCC` 的测试执行会以 `0xc0000139` 失败、生成阶段中断。

> **构建类型**：Ninja 与 MinGW Makefiles 都是单配置生成器，`--config Release` 不生效；构建类型只能在配置阶段用 `-DCMAKE_BUILD_TYPE` 指定，留空则走 `einstein_warnings` 的 `-O2` 分支。

## 🎮 操作说明

### 运行前提

- `einstein_gui.exe` 动态链接 `Qt6Core/Qt6Gui/Qt6Widgets`，需让运行时能在 `PATH` 中找到这些 DLL（开发环境为 `msys64/ucrt64/bin`），发布前可用 `windeployqt` 收集。
- `einstein_cli.exe` 与 `einstein_botzone.exe` 不依赖 Qt，只需 MinGW 运行时（`libstdc++-6.dll`、`libgcc_s_seh-1.dll`）。
- 存档固定写入 `einstein_save.db`（相对路径，见 `storage::kDefaultDbPath`）：GUI 启动时会 `QDir::setCurrent(applicationDirPath())`，因此存档落在 exe 旁边；CLI 则跟随启动目录，两者只有指向同一目录时才共用存档。

### GUI 版本 (推荐)

1. 启动 `einstein_gui.exe`。
2. 点击“开始新游戏”，在弹窗中选择阵营（红方先手）并点击格子循环切换 1~6 号棋子进行布阵。
3. 掷骰子后，可选棋子会亮起绿色虚线框，点击选中后变为金色实线框，同时显示可达落点。
4. 支持随时通过“保存”按钮输入存档名并存档。

### CLI 版本 (备用)

1. 在终端运行 `einstein_cli.exe`。
2. 按照终端菜单提示输入指令（1 开始游戏 / 2 读档 / 3 退出，走法输入形如 `0 2 1 2`）。

### Botzone 版本 (对战平台)

无状态单次调用适配层：进程从 stdin 读取**一行** JSON 请求，向 stdout 写回**一行** JSON 响应后退出。不链接 `storage`，也不写任何文件。

```bash
# 请求布阵：requests 为空即进入布阵阶段
echo '{"initdata":{"player":0},"requests":[]}' | ./build/bin/einstein_botzone.exe
# {"response":{"layout":[1,3,2,4,5,6]}}          ← 布阵为随机排列，每次不同

# 请求走子：红方 1 号在 (0,0)，掷出 3 点但 3 号不在场，按规则降级为可选 1 号
echo '{"initdata":{"player":0},"requests":[{"phase":"move","dice":3,"board":[[1,0,0,0,0],[0,0,0,0,0],[0,0,0,0,0],[0,0,0,0,0],[0,0,0,0,-1]]}]}' \
  | ./build/bin/einstein_botzone.exe
# {"response":{"x0":0,"x1":1,"y0":0,"y1":0}}     ← 三个合法落点中随机，每次不同
```

- 输入：`initdata.player` 为己方颜色（0 先手）；`requests` 为完整历史，适配层只取最后一条。`requests` 缺失或为空、以及 `phase` 为 `layout` 都返回布阵；`phase` 为 `move` 时按 `dice` 与 5x5 `board`（外层 x、内层 y，正数红方、负数蓝方）返回走法，`board` 不足 5x5 时缺失格按空位处理；其他 `phase` 返回空 `response`。
- 输出：`response.layout`（6 个编号）或 `response.x0/y0/x1/y1`。请求非法时（JSON 解析失败、`dice` 缺失或不在 1~6、`board` 缺失或类型错误）stderr 打印 `Bot error: ...`、stdout 输出 `{"response":{}}`，进程退出码仍为 0。

## 📂 项目结构

```text
.
├── src/
│   ├── core/            # 核心规则（纯 C++，零外部依赖）
│   │   ├── types.h          # Pos / Side / Piece
│   │   ├── board.h|.cpp     # 5x5 棋盘，每方 6 枚棋子
│   │   ├── game_state.h     # GameState 与 Phase 状态机
│   │   ├── rules.h|.cpp     # 可选棋子、走法校验、胜负判定
│   │   └── game.h|.cpp      # 对局门面：提交布阵、掷骰、走子、换手
│   ├── ai/              # decide_layout() / decide_move()，仅依赖 core
│   ├── storage/         # save_manager 存档序列化 + sqlite_wrapper RAII 封装
│   └── ui/              # 三个并列入口，各自独立可执行目标
│       ├── gui/             # Qt6：main_window + views/ + components/ + dialogs/ + style.qss
│       ├── cli/             # 终端菜单与对局循环
│       └── botzone/         # Botzone stdin/stdout JSON 适配层
├── third_party/
│   ├── sqlite/          # amalgamation 3.54.1（sqlite3.c 随仓库提供）
│   └── nlohmann/        # json.hpp 单头文件 3.12.0
└── CMakeLists.txt       # C++17、统一 -Werror、产物输出到 build/bin
```

### 构建目标与依赖

| 目标               | 类型   | 依赖                                               |
| ------------------ | ------ | -------------------------------------------------- |
| `einstein_core`    | 静态库 | 无                                                 |
| `einstein_ai`      | 静态库 | core                                               |
| `einstein_storage` | 静态库 | core、`sqlite3`（内部封装，不外泄 `<sqlite3.h>`）  |
| `einstein_cli`     | 可执行 | core、ai、storage                                  |
| `einstein_gui`     | 可执行 | core、ai、storage、`Qt6::Widgets`                  |
| `einstein_botzone` | 可执行 | core、ai + `nlohmann/json`（不接存档，保持无状态） |

各库以 `target_include_directories(... PUBLIC ..)` 暴露 `src/`，因此跨模块 include 统一写作 `core/game.h`、`ai/ai.h` 这类限定路径。顶层还定义了 INTERFACE 目标 `einstein_warnings`（`-Wall -Wextra -Wpedantic -Wshadow -Werror`），`src/` 下的每个目标都必须链接它。

## ⚠️ 当前限制

- **AI 为占位实现**：`decide_move()` 只在合法走法中均匀随机挑选，`decide_layout()` 随机打乱，没有搜索与局面评估，后续优化。
- **无自动化测试**：仓库未包含测试用例，也未启用 CTest；Botzone 适配层目前靠手工 JSON 用例验证。
- **适配层未校验棋子编号**：`board` 里的 `|val|` 没有限制在 1~6，非法编号会被当成真实棋子装载，最终以「AI 无路可走」收场而不是明确报错（`save_manager` 的反序列化对此是有校验的）。

## 👥 作者与分工

- **Spamton NEO**：独立完成
