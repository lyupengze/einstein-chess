# 架构说明

本文描述模块划分、构建目标、对局状态机与一次回合的调用路径。规则细节见
[棋盘规则](RULES.md)，存档字段见 [存档格式](SAVE_FORMAT.md)，取舍理由见
[设计取舍](DESIGN.md)。

## 1. 分层与依赖

依赖方向是单向的，上层可以依赖下层，下层不反向依赖：

```text
core            纯 C++17，只用标准库；棋盘、规则、局面校验、对局对象
  ↑
ai              搜索与随机档，只依赖 core
  ↑
storage         SQLite 封装与存档读写，依赖 core、ai 与第三方 sqlite3
  ↑
ui/gui          Qt Widgets 界面
ui/cli          标准库命令行
ui/botzone      nlohmann/json 协议适配
```

`storage` 是唯一直接接触数据库的模块；`ui` 三个入口互不依赖，各自只依赖 `core`、
`ai`、`storage` 中需要的部分。`core` 不包含任何 Qt、SQLite、JSON 类型，因此可以脱离界面
单独测试，也可以被 Botzone 与基准程序复用。

## 2. 目录与构建目标

| 目标                        | 类型   | 源目录           | 直接依赖                             |
| --------------------------- | ------ | ---------------- | ------------------------------------ |
| `einstein_core`             | 静态库 | `src/core`       | 标准库                               |
| `einstein_ai`               | 静态库 | `src/ai`         | `core`                               |
| `einstein_storage`          | 静态库 | `src/storage`    | `core`、`ai`、`sqlite3`              |
| `einstein_cli`              | 可执行 | `src/ui/cli`     | `core`、`ai`、`storage`              |
| `einstein_gui_lib`          | 静态库 | `src/ui/gui`     | `core`、`ai`、`storage`、Qt6 Widgets |
| `einstein_gui`              | 可执行 | `src/ui/gui`     | `gui_lib`、Qt6                       |
| `einstein_botzone_protocol` | 静态库 | `src/ui/botzone` | `core`、`ai`、nlohmann/json          |
| `einstein_botzone`          | 可执行 | `src/ui/botzone` | `botzone_protocol`                   |
| `einstein_benchmark`        | 可执行 | `benchmarks`     | `core`、`ai`                         |

测试目标由 `tests/CMakeLists.txt` 按用例逐个创建，命名规则是 `test_<用例名>`，与被测库的对应关系
也写在该文件里。构建目标之间的完整依赖关系以各级 `CMakeLists.txt` 为准。

GUI 拆成静态库加薄入口有两个原因：测试要链接 `gui_lib` 而不需要 `main()`；`einstein_gui`
在 Release 下用 `WIN32_EXECUTABLE` 去掉控制台窗口，Debug 下保留控制台方便看 `qDebug`。

## 3. 对局状态机

局面用 `core::GameState` 表示，阶段是 `core::Phase`。五个阶段与其余字段的组合由
`core::validate_state` 约束：

| 阶段              | `current_side`                | `current_dice` | `winner` | 其他约束                                              |
| ----------------- | ----------------------------- | -------------- | -------- | ----------------------------------------------------- |
| `LayOut`          | `None`                        | 无             | `None`   | 双方不得同时 ready；允许空棋盘或部分棋子              |
| `ThrowingDice`    | 红或蓝                        | 无             | `None`   | 双方各至少 1 子；棋盘不得已满足胜利条件               |
| `Player1Thinking` | 等于 `player1_side`           | 1~6 必有       | `None`   | 同上                                                  |
| `Player2Thinking` | 红或蓝且不等于 `player1_side` | 1~6 必有       | `None`   | 同上                                                  |
| `GameOver`        | 任意（含 `None`）             | 无             | 红或蓝   | 不得双方皆空；`winner` 必须等于 `rules::check_winner` |

`Player1` 与 `Player2` 是“发起方 / 另一方”的角色名，不是固定的红蓝：GUI 与 CLI 里人类固定占
`player1_side`，AI 占另一方；人类选蓝时全部分支照常成立。

允许的阶段转换只有下面四条，其他路径都会被状态机守卫拒绝：

```text
prepare_new_game()              → LayOut
双方 submit_layout() 完成        → ThrowingDice（先手固定红方）
roll_dice()                     → Player1Thinking 或 Player2Thinking
apply_move_and_settle() 无胜者   → 换边并回到 ThrowingDice
apply_move_and_settle() 有胜者   → GameOver
```

`Game::load_game()` 在接管外部局面之前会再跑一次 `require_valid_state`：存档来自磁盘，
不能假设调用方已经校验过。

## 4. 一次回合的调用路径

人类回合：

1. 点击“掷骰子” → 按钮禁用并改文案 → `roll_delay_ms_`（默认 400 ms）后由 `roll_timer_` 调用
   `Game::roll_dice()`；
2. `update_ui()` 按 `current_dice` 高亮可动棋子，棋盘只接受这两类点击：换选起点、确认落点；
3. 确认落点 → `Game::make_move()` → 追加历史 → 若未终局，`after_move_delay_ms_`（默认 500 ms）
   后把回合交给 AI。

AI 回合：

1. `ai_timer_` 到点 → `Game::roll_dice()` → `think_delay_ms_`（默认 800 ms）后调用
   `ai::decide_move()`；
2. `Game::make_move()` → `update_ui()`，回合交回人类。

三段延迟都由 `GameWidget` 的成员 `QTimer` 承担，而不是 `QTimer::singleShot`：开新局或读档时
必须能取消上一局已经排队的回调。保存对话框打开期间，两个定时器按剩余时间暂停，关闭后恢复，
保证“点击保存时看到的局面”就是存进库里的局面。

## 5. 结算只有一份实现

`core::apply_move_and_settle(const GameState&, Pos, Pos)` 是“落子 → 判胜 → 换手 → 清骰”的
唯一实现，`Game::make_move()` 与 AI 内部的着法搜索都复用它。这样 GUI、CLI、Botzone 与搜索
不会各自维护一套“走完子之后怎么变”的逻辑。胜负条件与优先级见
[棋盘规则](RULES.md) 第 6 节。

## 6. 校验分层

四层校验各管一段，互不重复：

| 层         | 位置                        | 职责                                               | 失败表现               |
| ---------- | --------------------------- | -------------------------------------------------- | ---------------------- |
| 单步合法性 | `rules::check_move`         | 边界、起点归属、是否可动、方向                     | 返回中文原因字符串     |
| 局面一致性 | `core::validate_state`      | 字段值域、阶段与字段组合、棋盘编号、胜负与阶段一致 | 返回 `StateIssue` 列表 |
| 抛出边界   | `core::require_valid_state` | 把问题列表拼成一条 `std::invalid_argument`         | 抛异常                 |
| 存档边界   | `storage` 写读两端          | 格式、数值范围、档位必填，再转交 `validate_state`  | 抛异常，不写库         |

`validate_state` 只读不抛，因此可以一次报告全部问题；`require_valid_state` 才负责把问题变成
异常。GUI 与 CLI 在用户操作的边界上捕获这些异常并转成提示，不让异常穿出事件循环或 `main`。

## 7. 线程与异常约定

- 全部代码在单线程里运行：AI 是同步计算，GUI 只用定时器把同步计算切成三段，避免一次点击
  之内连续做“掷骰 + 思考 + 落子”。
- `core`、`ai`、`storage` 的错误一律用异常报告，类型只用 `std::invalid_argument`（参数或调用
  时序不合法）与 `std::runtime_error`（外部数据或 I/O 失败）；`what()` 为中文。
- GUI 在每个用户动作的入口捕获异常：可以恢复的用 Toast 提示，无法继续的进入“对局异常”中止态，
  只改界面、不改局面。
- Botzone 入口把协议错误与内部错误分开：前者是 `ProtocolError`，后者是其他 `std::exception`，
  两者都只写 stderr 并输出空响应，退出码保持 0。

## 8. 平台分支

平台差异只出现在两处，都不影响规则与存档格式：

- **源码分支**：`src/ui/cli/main_cli.cpp` 用 `#ifdef _WIN32` 打开 Windows 控制台代码页，
  让 UTF-8 中文提示在终端正常显示。
- **构建分支**：`src/ui/gui/CMakeLists.txt` 的 `WIN32_EXECUTABLE "$<CONFIG:Release>"`，
  Release 使用 Windows GUI 子系统、双击不附带控制台窗口，Debug 保留 Console 子系统以便
  查看日志与调试输出。

其余源码不区分平台。
