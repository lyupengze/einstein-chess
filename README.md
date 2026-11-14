# ♟️ 爱因斯坦棋 (Einstein Chess)

<div align="center">

![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg?style=flat-square&logo=c%2B%2B)
![Qt](https://img.shields.io/badge/Qt-6.11.2-green.svg?style=flat-square&logo=qt)
![SQLite](https://img.shields.io/badge/SQLite-3.54.1-blue.svg?style=flat-square&logo=sqlite)
![CMake](https://img.shields.io/badge/CMake-4.0+-red.svg?style=flat-square&logo=cmake)
![License](https://img.shields.io/badge/License-MIT-yellow.svg?style=flat-square)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey.svg?style=flat-square&logo=windows)

**基于 C++17 与 Qt6 实现的 5x5 爱因斯坦棋人机对弈程序**

包含 CLI 与 GUI 版本，支持图形化布阵、智能高亮提示以及基于 SQLite 的本地多存档系统。

</div>

## 🛠️ 环境依赖与编译说明

- **编译器**: GCC 16.2.0 (或任意支持 C++17 的编译器)
- **构建工具**: CMake 4.0+
- **第三方库**:
  - `Qt6` (用于 GUI 渲染)
  - `SQLite3` (已集成 amalgamation 源码，无需外部安装)

### 编译方法

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

_(注：编译后的可执行文件位于 `build/bin/` 目录下)_

## 🎮 操作说明

### GUI 版本 (推荐)

1. 启动 `einstein_gui.exe`。
2. 点击“开始新游戏”，在弹窗中选择阵营（红方先手）并点击格子循环切换 1~6 号棋子进行布阵。
3. 掷骰子后，可选棋子会亮起绿色虚线框，点击选中后变为金色实线框，同时显示可达落点。
4. 支持随时通过“保存”按钮输入存档名并存档。

### CLI 版本 (备用)

1. 在终端运行 `einstein_cli.exe`。
2. 按照终端菜单提示输入指令。

## 📂 项目结构

```text
.
├── src/
│   ├── core/       # 核心游戏逻辑（纯 C++，无外部依赖）
│   ├── ai/         # AI 决策模块
│   ├── storage/    # SQLite 存档序列化与持久化
│   └── ui/
│       ├── gui/    # Qt6 图形界面
│       └── cli/    # 命令行界面
├── third_party/    # 第三方库源码 (SQLite)
└── CMakeLists.txt  # 顶层构建脚本
```

## 👥 作者与分工

- **Spamton NEO**：独立完成
