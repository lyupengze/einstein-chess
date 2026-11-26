# 第三方组件与许可证说明

本项目以 **MIT License** 发布，全文见 [项目许可证](LICENSE)。本文件说明仓库里包含哪些第三方代码、构建时依赖哪些组件，以及分发二进制时需要附带什么。它不是法律意见；具体义务以各许可证原文为准。

## 仓库内置的第三方代码

这些文件随仓库一起分发，使用时需要保留各自的许可证与版权声明。

### SQLite

- **版本**：3.54.1
- **许可证**：Public Domain（SQLite 作者明确声明放弃版权）
- **位置**：`third_party/sqlite/`，以 amalgamation 形式内置
- **项目主页**：<https://www.sqlite.org/>
- **说明**：Public Domain 不要求署名或附带许可证文本；此处列出是出于透明与致谢。项目不修改 SQLite 的许可证声明。

### nlohmann/json

- **版本**：3.12.0
- **许可证**：MIT
- **位置**：`third_party/nlohmann/json.hpp`，单头文件内置
- **项目主页**：<https://github.com/nlohmann/json>
- **版权声明**：Copyright (c) 2013-2026 Niels Lohmann

该组件的 MIT 许可证文本如下，随源码分发时请保留：

```text
MIT License

Copyright (c) 2013-2026 Niels Lohmann

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## 构建时依赖

这些组件不随仓库分发，由构建环境提供。只使用源码时，不需要在仓库里附带它们的许可证全文。

### Qt 6

- **版本**：6.11.2（开发与测试环境实测版本）；CI 由 MSYS2 包管理器安装 Qt6，不固定小版本
- **使用模块**：Core、Gui、Widgets
- **许可证**：LGPLv3（本项目按 Qt 的 LGPLv3 选项使用；Qt 同时提供 GPLv3 与商业许可）
- **获取方式**：构建 GUI 时由 CMake 的 `find_package(Qt6)` 在用户环境中查找；仓库不包含 Qt 的源码、DLL 或静态库
- **链接方式**：动态链接，用户可替换 Qt 库
- **项目主页**：<https://www.qt.io/>
- **LGPLv3 文本**：<https://www.gnu.org/licenses/lgpl-3.0.html>
- **GPLv3 文本**：<https://www.gnu.org/licenses/gpl-3.0.html>
- **Qt 源码**：<https://code.qt.io/>
- **说明**：本项目使用的 Core、Gui、Widgets 不在 Qt 的 GPL-only 模块清单内。

### 编译器与构建工具

- **构建工具**：CMake ≥3.20、支持 C++17 的编译器、Ninja 或 Make。它们不随仓库分发，其许可证不约束本项目的源码。
- **验证环境**：Windows + MSYS2 UCRT64 的 GCC 16.2 与 Qt 6.11，详见 [构建说明](docs/BUILD.md)。
- **运行时**：生成的二进制会链接编译器运行时，分发时的义务取决于实际工具链，见下一节。

## 分发二进制时

只发布源码时无需附许可证全文；把 `einstein_gui.exe` 等可执行文件与相关 DLL 打包分发时，按下面的条目处理。

1. **Qt**：附 LGPLv3 与 GPLv3 两份文本，保留 Qt 的版权与许可证声明，保留用户替换 Qt 动态库的能力，提供 Qt 源码的获取方式或书面要约，并在显著位置说明该二进制使用了 Qt。
2. **编译器运行时**：GCC/MinGW-w64 的 `libstdc++` 与 `libgcc` 为 GPLv3 加 GCC Runtime Library Exception，例外条款让本项目自身不受 GPL 约束，但单独分发这两个 DLL 要附 GPLv3 文本，并按 GPLv3 第 6 节提供源码或书面要约；`libwinpthread` 与 mingw-w64 运行时为 BSD 式许可，需保留版权与许可声明。MSVC 运行时按微软的可再分发条款处理；Clang/libc++ 为 Apache-2.0 WITH LLVM-exception。
3. **`windeployqt` 复制的内容**：除 Qt 库外还会带上 Qt 的第三方依赖，每个组件各有许可证。清单随 Qt 发行版而变，以 `windeployqt` 的实际输出为准。
4. **Windows 系统组件**：UCRT、KERNEL32 等属于操作系统，不属于再分发内容。

新增第三方代码或调整依赖时，同步更新本文件对应的条目。
