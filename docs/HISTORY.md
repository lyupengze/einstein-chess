# 缺陷台账

本文记录已经修复、且值得留下证据的缺陷：每条一个稳定锚点，说明当时的现象、触发方式、
根因、修复提交与回归用例。代码注释与测试需要引用历史缺陷时，引用锚点而不是数字编号。

- 锚点是本文的条目键，写成 `#board-remainder-click` 这样的形式，可在链接里直接跳转。
- “修复”列给出提交号，可用 `git show <提交号>` 查看完整改动。
- “来源”列指向不随仓库分发的外部材料（测试报告、评审记录），仅作追溯线索；
  事实以提交与测试为准，材料里的行号与提法可能随仓库演进而失配。

## 索引

| 锚点                                                       | 一句话                           |
| ---------------------------------------------------------- | -------------------------------- |
| [`#board-remainder-click`](#board-remainder-click)         | 棋盘余数像素算出越界坐标         |
| [`#cli-empty-dice`](#cli-empty-dice)                       | AI 走子后读已清空的骰点          |
| [`#gui-status-empty-dice`](#gui-status-empty-dice)         | 思考阶段无骰点时读空 optional    |
| [`#gui-load-stuck`](#gui-load-stuck)                       | 载入“电脑思考中”的存档后永久卡住 |
| [`#gui-ai-error-stuck`](#gui-ai-error-stuck)               | 电脑回合异常后停在思考中         |
| [`#gui-roll-button-stuck`](#gui-roll-button-stuck)         | 掷骰异常后按钮永久禁用           |
| [`#gui-dialog-outside-try`](#gui-dialog-outside-try)       | 存档对话框在异常边界之外构造     |
| [`#gui-stale-timer`](#gui-stale-timer)                     | 上一局的延迟回调落到新局         |
| [`#gui-toast-fade`](#gui-toast-fade)                       | Toast 没有淡入淡出               |
| [`#gui-combo-font-warning`](#gui-combo-font-warning)       | 强度下拉框展开时打印字体警告     |
| [`#gui-new-game-exception`](#gui-new-game-exception)       | 开局异常穿出菜单回调             |
| [`#storage-bind-ignored`](#storage-bind-ignored)           | SQLite 参数绑定失败被忽略        |
| [`#save-unchecked-state`](#save-unchecked-state)           | 存档字段各自合法但组合不可能     |
| [`#core-invariant-gaps`](#core-invariant-gaps)             | 公共 API 不强制阶段顺序          |
| [`#save-level-overflow`](#save-level-overflow)             | 档位溢出被当成缺少档位           |
| [`#layout-save-unrecoverable`](#layout-save-unrecoverable) | 布阵阶段的存档读不回来           |
| [`#save-level-unvalidated`](#save-level-unvalidated)       | 写入端不校验档位                 |
| [`#seed-not-affecting-dice`](#seed-not-affecting-dice)     | `--seed` 只影响 AI，骰子仍随机   |
| [`#cli-db-open-crash`](#cli-db-open-crash)                 | 存档库打不开时异常穿出 main      |
| [`#cli-eof-loop`](#cli-eof-loop)                           | stdin 关闭后菜单无限刷屏         |
| [`#cli-eof-after-ai`](#cli-eof-after-ai)                   | 终局前 stdin 结束会跳过结算展示  |
| [`#botzone-invalid-piece`](#botzone-invalid-piece)         | 非法棋子编号被当作真实棋子       |
| [`#botzone-int-truncation`](#botzone-int-truncation)       | 超大整数被静默截断               |
| [`#ai-shared-rng`](#ai-shared-rng)                         | 随机源不可复现且跨线程共享       |
| [`#win-rate-truncated`](#win-rate-truncated)               | 竞速表的概率行和小于 1           |

## 对局与存档

### `#board-remainder-click` 棋盘余数像素算出越界坐标

- **现象**：点击棋盘最右或最下 1~2 像素宽的区域，进程立即终止（退出码 134），对局无法继续。
- **触发**：402×402 窗口下点击 `(400, 200)`：棋盘绘制区是 400×400，右侧多出 2 像素。
- **根因**：命中检测与绘制各算一次几何尺寸，检测用 `min(width, height)`（402），绘制用
  `cell_size * 5`（400）；余数像素除以 80 得到行或列 5，越界坐标被送进 `Board::at`。
- **修复**：`aec4d76` 抽出 `BoardGeometry compute_geometry()`，命中检测改用 `span = cell_size * 5`
  并显式拒绝 `>= kSize` 的下标；`on_cell_clicked` 增加同样的边界检查。
- **回归**：`board_geometry`。
- **来源**：外部报告（2026-11-15，基线 `f2e8bf5`）第 1 条。

### `#save-unchecked-state` 存档字段各自合法但组合不可能

- **现象**：GUI 读档后永久卡住；CLI 读档后抛「AI 无路可走！」。
- **触发**：把元数据写成 `1 1 3 1 0 1 1`（发起红、当前红、阶段为 Player2 思考、骰点 1）
  或 `1 0 3 1 0 1 1`（当前方为空）。
- **根因**：反序列化只校验单个字段的值域，不校验阶段与当前方、骰点、棋盘胜负的组合一致性。
- **修复**：`f7b591c` 把 `core::validate_state` 接入读写两端；`aec4d76` 先加两条手工检查，
  `32aa7b0` 去掉重复检查。
- **回归**：`state_validation`、`storage_roundtrip`。
- **来源**：外部评审记录（2026-11-25）第 1 条。

### `#core-invariant-gaps` 公共 API 不强制阶段顺序

- **现象**：可以重复提交布阵、替非当前方走子、在终局后改棋盘，`Game` 进入自相矛盾的状态。
- **触发**：绕过界面直接调用 `prepare_new_game()`、`submit_layout()`、`roll_dice()`、
  `make_move()`、`load_game()`。
- **根因**：`Game` 的每个方法只检查自己需要的最小条件，没有统一的阶段约束。
- **修复**：`aee4b6a` 为五个入口补齐阶段检查，并新增 `game_state_guard` 覆盖非法调用。
- **回归**：`game_state_guard`。
- **来源**：外部评审记录（2026-11-25）第 2 条。

### `#seed-not-affecting-dice` `--seed` 只影响 AI，骰子仍随机

- **现象**：`--seed 42` 连续跑两局，骰子点数不同；读档继续后骰序与 AI 决策也接不上，
  命令行与文档承诺的“复现同一局”不成立。
- **触发**：带 `--seed` 开新局并记录骰点；或保存一局后读档继续。
- **根因**：骰子随机源在 `core::Game` 构造里直接用 `std::random_device`，参数只播种了 AI；
  存档既不记录种子，也不记录已掷骰次数，读档无法回到同一位置。
- **修复**：`efa0fe8` 让 `Game` 支持显式种子并计数掷骰；`319f7b0` 让 CLI 用同一棵种子
  同时播种两个随机源并打印本局种子；`4bb488f` 让 AI 只由“种子 + 局面”决定；
  存档追加随机种子与已掷次数（见 [存档格式](SAVE_FORMAT.md)）。
- **回归**：`game_seed`、`ai_random`、`save_level`。
- **来源**：代码审查（2026-11-27，基线 `f3942aa`）第 1 条。

## 图形界面

### `#gui-load-stuck` 载入“电脑思考中”的存档后永久卡住

- **现象**：读档后状态栏显示轮到电脑，但电脑不掷骰、不走子，只能返回主页。
- **触发**：保存一局在 `Player2Thinking` 阶段的存档，回主菜单再读取。
- **根因**：`handle_ai_turn()` 只处理 `ThrowingDice` 一个分支，其他阶段直接返回。
- **修复**：`aec4d76` 按 `phase` 分派：掷骰阶段先掷骰，思考阶段直接要着法。
- **回归**：`gui_reconcile`。
- **来源**：外部报告（2026-11-15，基线 `f2e8bf5`）第 3 条。

### `#gui-status-empty-dice` 思考阶段无骰点时读空 optional

- **现象**：进入对局或读档后界面刷新中止，窗口卡死。
- **触发**：构造 `phase` 为思考阶段、`current_dice` 为空的局面。
- **根因**：状态栏拼接骰点文案时直接解引用 optional，没有判空。
- **修复**：`aec4d76` 先判空，有值显示数字、无值显示 `?`。
- **回归**：`gui_status`。
- **来源**：外部报告（2026-11-15，基线 `f2e8bf5`）第 4 条。

### `#gui-ai-error-stuck` 电脑回合异常后停在思考中

- **现象**：电脑回合抛异常后，界面停在「电脑思考中」，棋盘与掷骰都不可用。
- **触发**：构造电脑无法走子的局面（例如无合法着法）。
- **根因**：捕获异常后只写日志，没有把界面恢复到可操作状态。
- **修复**：`4e8a3de` 让异常进入“对局异常”中止态：停定时器、禁用棋盘与掷骰、保留原局面等待返回主页。
- **回归**：`gui_reconcile`。
- **来源**：外部报告（2026-11-15，基线 `f2e8bf5`）第 9 条。

### `#gui-roll-button-stuck` 掷骰异常后按钮永久禁用

- **现象**：掷骰失败一次后，“掷骰子”按钮一直灰着，对局无法继续。
- **触发**：在非掷骰阶段触发一次掷骰（界面时序错乱，或读档后状态不一致）。
- **根因**：点击时先禁用按钮，异常路径没有恢复按钮与文案。
- **修复**：`aec4d76` 在异常分支恢复按钮状态并提示原因。
- **回归**：`gui_status`。
- **来源**：外部报告（2026-11-15，基线 `f2e8bf5`）第 8 条。

### `#gui-dialog-outside-try` 存档对话框在异常边界之外构造

- **现象**：打开“保存”或“读取存档”时，数据库异常直接终止进程。
- **触发**：`einstein_save.db` 存在但不可读，或表被同名对象占用。
- **根因**：`SaveLoadDialog` 的构造函数会查库，而它被放在 `try` 之外创建。
- **修复**：`8555743` 把对话框构造移进 `try`，失败时提示并保持界面可用。
- **回归**：`gui_dialogs`。
- **来源**：外部报告（2026-11-15，基线 `f2e8bf5`）第 6 条。

### `#gui-stale-timer` 上一局的延迟回调落到新局

- **现象**：人类走子后 500 ms 内点“返回主页”并开新局，新局会突然自动掷骰或自动走子。
- **触发**：走子后立刻读档或重开，且上一次的回调尚未触发。
- **根因**：延迟用 `QTimer::singleShot` 排队，没有句柄可以取消。
- **修复**：`9422138` 使用成员 `QTimer`，开新局与读档前调用 `cancel_pending_actions()`；
  保存对话框按剩余时间暂停与恢复。
- **回归**：`gui_reconcile`（150/150 ms 延迟配 250 ms 观察窗）。
- **来源**：无。

### `#gui-toast-fade` Toast 没有淡入淡出

- **现象**：提示框瞬现瞬隐，透明度动画不生效。
- **触发**：任意会弹 Toast 的操作（保存成功、非法点击）。
- **根因**：对子控件调用 `QWidget::setWindowOpacity` 只对顶层窗口生效。
- **修复**：`9422138` 用容器叠加内层文字控件，透明度用 `QGraphicsOpacityEffect` 作用在容器上。
- **回归**：`toast_fade`。
- **来源**：无。

### `#gui-combo-font-warning` 强度下拉框展开时打印字体警告

- **现象**：启动 GUI 后点开强度下拉框，stderr 打印 `QFont::setPointSize: Point size <= 0 (-1),
  must be greater than 0`；功能不受影响，启动、布阵对话框与对局页本身都不打印。
- **触发**：打开布阵对话框，展开强度下拉框的候选列表。
- **根因**：样式表给 `QComboBox` 写的是像素字号（`14px`），Qt 把它记成像素字号有效、`pointSize()`
  为 -1；弹出层继承这份字体并再次按点字号套用，于是触发 `QFont::setPointSize(-1)`。
- **修复**：`5d6da25` 把该规则改为 `font-size: 10.5pt`（96 DPI 下与 14px 等值），并在样式表
  注释里写明单位要求。
- **回归**：`gui_style`。
- **来源**：无。

### `#gui-new-game-exception` 开局异常穿出菜单回调

- **现象**：从主菜单点“开始新游戏”后，若开局路径抛异常（例如快照库所在目录不可写），
  异常穿出按钮回调，进程直接退出，玩家看不到任何提示。
- **触发**：`GameWidget::start_new_game` 在构造对局、写现场快照时抛 `std::exception`。
- **根因**：主菜单的“开始新游戏”lambda 没有异常边界，`start_new_game` 的失败直接冒到 Qt 事件循环。
- **修复**：`5ec8bf1` 把 `start_new_game` 与页面切换包进 `try/catch(std::exception)`，失败弹
  “开局失败”对话框并留在菜单页。
- **回归**：缺；该路径依赖 GUI 运行期写文件失败，当前没有稳定的自动化入口。
- **来源**：代码审查（2026-11-27，基线 `f3942aa`）低 1 条。

## 存储

### `#storage-bind-ignored` SQLite 参数绑定失败被忽略

- **现象**：绑定失败时语句继续执行，写入的值可能是上一轮残留或空值，读档得到不完整数据。
- **触发**：传入超长字符串，或在语句生命周期之外调用绑定。
- **根因**：`sqlite3_bind_*` 的返回码全部丢弃。
- **修复**：`aec4d76` 统一走 `check_bind()`，非 `SQLITE_OK` 抛异常。
- **回归**：`storage_bind`。
- **来源**：外部报告（2026-11-15，基线 `f2e8bf5`）第 11 条。

### `#save-level-overflow` 档位溢出被当成缺少档位

- **现象**：尾部档位写成超长数字时，读档静默使用默认档，保存时的强度丢失。
- **触发**：构造末尾档位为 30 个 9 的数据串。
- **根因**：流提取把溢出置为 `failbit`，判断 `!iss.eof()` 时误判为“读到结尾”，仍以默认档继续。
- **修复**：`3ffa578` 改用 `from_chars` 并检查完整消费与溢出。
- **回归**：`save_level`。
- **来源**：外部审查（基线 `32853af`）第 2 条。

### `#layout-save-unrecoverable` 布阵阶段的存档读不回来

- **现象**：布阵阶段可以存档，但 CLI 与 GUI 都没有恢复布阵的流程，读档后卡住或以空阵营继续。
- **触发**：布阵未确认时存档，再读档。
- **根因**：存储层只校验字段，允许 `LayOut`；应用层没有对应的续接路径。
- **修复**：`3ffa578` 在写入与读取两端都拒绝 `LayOut`。
- **回归**：`save_level`、`storage_roundtrip`。
- **来源**：外部审查（基线 `32853af`）第 3 条。

### `#save-level-unvalidated` 写入端不校验档位

- **现象**：`save_game()` 接受任意整数档位，写出的存档自己读不回。
- **触发**：调用 `save_game(name, state, static_cast<ai::Level>(99))`。
- **根因**：写入前没有校验档位是否属于 `kAllLevels`。
- **修复**：`3ffa578` 写入前用 `ai::is_valid_level` 拒绝。
- **回归**：`save_level`。
- **来源**：外部审查（基线 `32853af`）第 4 条。

## 命令行

### `#cli-empty-dice` AI 走子后读已清空的骰点

- **现象**：每次电脑走子后进程中止（断言失败），CLI 对局无法进行。
- **触发**：开始一局，等电脑回合自动走子。
- **根因**：`Game::make_move()` 结算时把 `current_dice` 置空，调用方随后仍解引用
  `*current_dice` 打印点数。
- **修复**：`aec4d76` 在走子前把点数存进局部变量，打印使用副本。
- **回归**：`cli_session`。
- **来源**：外部报告（2026-11-15，基线 `f2e8bf5`）第 2 条。

### `#cli-db-open-crash` 存档库打不开时异常穿出 main

- **现象**：启动 CLI 时存档库不可打开，进程以退出码 134 终止，没有可读提示。
- **触发**：在只读目录启动，或用目录路径作为 `--db`。
- **根因**：`SaveManager` 构造在 `main` 的 `try` 之外。
- **修复**：`aec4d76` 把构造放进 `try` 并打印指引；`45b841d` 补齐对局内的异常边界与
  `cli_broken_db`。
- **回归**：`cli_broken_db`。
- **来源**：外部报告（2026-11-15，基线 `f2e8bf5`）第 5 条。

### `#cli-eof-loop` stdin 关闭后菜单无限刷屏

- **现象**：管道输入结束或按 Ctrl+Z 后，主菜单提示反复打印，进程不退出。
- **触发**：`echo "" | ./einstein_cli`。
- **根因**：所有 `std::getline(std::cin, ...)` 的返回值被丢弃，读失败后循环继续。
- **修复**：`aec4d76` 引入 `read_line()` 返回 `bool`，各调用点读到结尾就退出；`3fa29e6` 补断言。
- **回归**：`cli_eof`。
- **来源**：无（外部报告未覆盖）。

### `#cli-eof-after-ai` 终局前 stdin 结束会跳过结算展示

- **现象**：AI 落子形成终局后，程序仍在等“按回车继续”；此时 stdin 已结束，进程直接返回，
  终局棋盘与获胜方都不打印，玩家看到的是半局。
- **触发**：脚本或管道在 AI 回合结束后不再提供输入，且 AI 这一手形成了终局。
- **根因**：AI 回合结束后无条件等待回车，EOF 时走 `return`，绕过了循环外的结算展示。
- **修复**：`5ec8bf1` 在 AI 走子后先判 `phase == GameOver` 并 `break`，把结算交给循环外的统一展示。
- **回归**：缺；需要构造“AI 一手终结对局”的固定局面。
- **来源**：代码审查（2026-11-27，基线 `f3942aa`）低 2 条。

## Botzone

### `#botzone-invalid-piece` 非法棋子编号被当作真实棋子

- **现象**：棋盘上出现 7 号、0 号或同方重复编号时，程序照常决策并返回空响应，直接判负。
- **触发**：请求里把某格写成 7，或把两个格子都写成 1。
- **根因**：`decode_board` 只检查 5×5 形状，不检查编号范围与同方唯一。
- **修复**：`9a50f5f` 增加范围与重复校验并按协议错误拒绝；`6f6a931` 补齐 `color`/`phase`
  组合校验。
- **回归**：`botzone_protocol`。
- **来源**：外部报告（2026-11-15，基线 `f2e8bf5`）第 7 条。

### `#botzone-int-truncation` 超大整数被静默截断

- **现象**：`dice=4294967297` 被当成 1、`color=4294967296` 被当成红方、格子 `4294967297`
  被当成红 1，程序按与请求不同的局面决策。
- **触发**：请求里用超过 32 位范围的整数。
- **根因**：`json::get<int>()` 对宽整数静默窄化。
- **修复**：`73f3d86` 增加 `checked_json_int()`，先按 64 位读取再检查范围。
- **回归**：`botzone_protocol`。
- **来源**：外部审查（基线 `32853af`）第 1 条。

## AI

### `#ai-shared-rng` 随机源不可复现且跨线程共享

- **现象**：相同的固定种子无法复现同一布阵；多线程同时调用时随机序列互相干扰。
- **触发**：用 `--seed` 启动两次，对比 AI 布阵。
- **根因**：`decide_layout()` 与 `decide_move()` 各有一个函数内 `static std::mt19937`，
  种子来自 `random_device`。
- **修复**：`aec4d76` 使用 `thread_local` 引擎加 `ai::set_random_seed()`。
- **回归**：`ai_random`。
- **来源**：外部报告（2026-11-15，基线 `f2e8bf5`）第 10 条。

### `#win-rate-truncated` 竞速表的概率行和小于 1

- **现象**：`estimate_win_rate()` 在远离终点的局面上可能返回略大于 1 或小于 0 的值；界面不展示
  该值，只有直接调用方会看到。
- **触发**：双方棋子离各自目标都很远，竞速表在 `kRaceMaxStep` 处截断。
- **根因**：建表时每行只累计截断内的到达概率，行和可能小于 1，`win_rate()` 直接拿它比较。
- **修复**：`52b08f7` 在建表时按行归一化，并在 `win_rate()` 里把结果夹到 `[0, 1]`。
- **回归**：`ai_search`。
- **来源**：代码审查（2026-11-27，基线 `f3942aa`）第 11 条。

