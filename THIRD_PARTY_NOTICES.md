# 第三方组件与许可证说明

本项目自身以 **MIT License** 发布，完整文本见根目录 [LICENSE](LICENSE)。

本文件列出项目使用或依赖的第三方组件、版本、许可证与分发方式，供使用者、二次分发者和课程评审参考。它不是法律意见；如果要把本项目打包成二进制分发，请按下面各节的要求保留相应声明。

## Qt 6

- **版本**：Qt 6.11.2（开发环境实测版本）；CI 由 MSYS2 包管理器安装 Qt6，不固定小版本
- **使用模块**：Qt6 Widgets（及其依赖的 Core、Gui 模块）
- **许可证**：LGPLv3（本项目按 Qt 的 LGPLv3 选项使用；Qt 同时提供 GPLv3 与商业许可）
- **分发方式**：**构建时外部依赖，本仓库不包含 Qt 的源码、DLL、静态库或其他二进制文件。** 构建 GUI 时由 CMake 的 `find_package(Qt6)` 在用户环境中查找 Qt。
- **链接方式**：当前构建按动态链接使用 Qt 库；动态链接满足 LGPLv3 对“用户可以替换库”的要求。
- **项目主页**：<https://www.qt.io/>
- **LGPLv3 文本**：<https://www.gnu.org/licenses/lgpl-3.0.html>
- **Qt 源码**：<https://code.qt.io/>

### 如果分发包含 Qt 的二进制

仅发布本仓库源码时，不需要在仓库中附带 LGPLv3 全文；但**如果发布便携包，或在压缩包中附带 `Qt6Core.dll`、`Qt6Widgets.dll` 等 Qt 二进制**，则需要：

1. 附上 LGPLv3 全文（以及 LGPLv3 所引用的 GPLv3 文本）；
2. 保留 Qt 的版权声明与许可证声明；
3. 保留用户替换 Qt 动态库的能力（不要用静态链接或反规避手段锁死）；
4. 按 LGPLv3 的要求提供 Qt 源码的获取方式，或附上相应源码/书面要约；
5. 在显著位置说明该二进制使用了 Qt，并给出获取 Qt 源码与许可证的途径。

使用 `windeployqt` 生成便携包时，建议把上述材料放在 `licenses/` 目录，并与 `THIRD_PARTY_NOTICES.md` 一起分发。

## SQLite

- **版本**：3.54.1
- **许可证**：Public Domain（SQLite 作者明确声明放弃版权）
- **分发方式**：源码内置，位于 `third_party/sqlite/`，以 amalgamation 形式随仓库提供。
- **项目主页**：<https://www.sqlite.org/>
- **说明**：Public Domain 不要求署名或附带许可证文本；此处列出是出于透明与致谢。项目自身不修改 SQLite 的许可证声明。

## nlohmann/json

- **版本**：3.12.0
- **许可证**：MIT
- **分发方式**：单头文件内置，位于 `third_party/nlohmann/json.hpp`。
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

## 工具链

CMake、GCC/MinGW、Ninja/Make 等构建工具不随本项目源码分发，也不属于运行时依赖；它们的许可证不适用于本项目的源码。若分发由这些工具生成的二进制，只需遵守上面 Qt、SQLite、nlohmann/json 的条款。
