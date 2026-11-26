# Botzone 协议与适配层

`einstein_botzone` 是对战平台的无状态单次调用入口：从 stdin 读取**一行** JSON 请求，向 stdout 写出**一行** JSON 响应后退出。它不链接 `storage`，也不读写任何存档文件。

## 运行方式

```bash
# 单次请求：把 JSON 从 stdin 传入，读取 stdout 的一行响应
echo '{"requests":[],"responses":[]}' | ./build/bin/einstein_botzone
```

Windows 下可执行文件为 `einstein_botzone.exe`，管道与 JSON 内容不变。

## 请求格式

顶层是 JSON 对象。适配层取 `requests` 数组的最后一条 request 作为决策依据；`responses` 等历史字段不参与决策。平台不会给 Bot 下发顶层 `initdata`，本方阵营只取该条 request 的 `color` 字段：

- `0`：红方（先手）
- `1`：蓝方

`color` 是必填字段；缺失、非整数或超出 `0/1` 都会按协议错误拒绝。本方阵营只由该字段决定，适配层不读取其他字段。

`requests` 缺失或为空、以及 `phase` 为 `layout` 时，返回布阵；`phase` 为 `move` 时返回走子；其他 `phase` 返回空 `response`。

### `phase: "layout"`

适配层返回 1–6 的一个排列。平台按 request 里的 `region` 顺序，把排列中的编号依次落到对应格子；因此适配层不需要读取 `region`，布阵结果与颜色无关，每次调用都是新的随机排列。

```json
{
  "requests": [
    {
      "phase": "layout",
      "color": 1,
      "region": [
        [4, 4],
        [4, 3],
        [4, 2],
        [3, 4],
        [3, 3],
        [2, 4]
      ]
    }
  ],
  "responses": []
}
```

### `phase: "move"`

适配层依据 `dice` 与严格 5×5 的整数 `board` 选择一步合法走法：

- `dice`：整数，必须在 `1..6`；
- `board`：5 行 × 5 列，外层行 `x`、内层列 `y`；
- 正数表示红方棋子（`1..6`），负数表示蓝方棋子（`-1..-6`），`0` 表示空格；
- `color`、`dice`、棋盘格的整数必须在 32 位有符号范围内；超出范围的值不能被截断后继续使用；
- 缺行、缺列、多余行/列、非整数或越界都会被拒绝；棋盘按原样校验，不补空位；
- 棋盘上的棋子编号在同方内不能重复。

```json
{
  "requests": [
    {
      "phase": "move",
      "color": 1,
      "dice": 4,
      "board": [
        [1, 2, 3, 0, 0],
        [4, 5, 0, 0, 0],
        [6, 0, 0, 0, -6],
        [0, 0, 0, -5, -4],
        [0, 0, -3, -2, -1]
      ]
    }
  ],
  "responses": []
}
```

构造出的局面会先经过 `core::validate_state` 校验；如果 request 的 `color`、`phase` 与棋盘组合不成合法局面，按协议错误处理，而不是带着错误状态继续搜索。

## 响应格式

布阵：

```json
{ "response": { "layout": [2, 5, 1, 6, 3, 4] } }
```

走子：

```json
{ "response": { "x0": 3, "x1": 3, "y0": 4, "y1": 3 } }
```

`layout` 是 1–6 的排列；`x0/y0` 是起点，`x1/y1` 是终点。适配层不检查平台最终是否会接受该着法，只保证返回的是当前局面下的合法着法之一；具体落点由 Expectiminimax 搜索决定，示例输出可能不同。

JSON 对象成员没有顺序语义；当前实现使用 `nlohmann::json`，其底层按 key 排序，因此实际输出为 `x0, x1, y0, y1`。调用方应按字段名读取，不应依赖打印顺序。

## 完整示例

```bash
# 布阵：返回 1–6 的随机排列，每次不同
echo '{"requests":[{"phase":"layout","color":1,"region":[[4,4],[4,3],[4,2],[3,4],[3,3],[2,4]]}],"responses":[]}' \
  | ./build/bin/einstein_botzone
# {"response":{"layout":[2,5,1,6,3,4]}}

# 走子：蓝方掷出 4 点
echo '{"requests":[{"phase":"move","color":1,"dice":4,"board":[[1,2,3,0,0],[4,5,0,0,0],[6,0,0,0,-6],[0,0,0,-5,-4],[0,0,-3,-2,-1]]}],"responses":[]}' \
  | ./build/bin/einstein_botzone
# {"response":{"x0":3,"x1":3,"y0":4,"y1":3}}

# 骰点降级：红方掷出 3 点但 3 号不在场，按规则降级为可选 1 号
echo '{"requests":[{"phase":"move","color":0,"dice":3,"board":[[1,0,0,0,0],[0,0,0,0,0],[0,0,0,0,0],[0,0,0,0,0],[0,0,0,0,-1]]}],"responses":[]}' \
  | ./build/bin/einstein_botzone
# {"response":{"x0":0,"x1":1,"y0":0,"y1":1}}
```

## 错误处理

以下情况都按协议错误处理：

- JSON 语法错误；
- `requests` 不是数组，或数组元素不是对象；
- `phase` 不是字符串；
- `color` 不是 `0/1`；
- `dice` 缺失、不是整数或不在 `1..6`；
- `board` 缺失、不是严格 5×5 整数矩阵，或棋子编号越界/同方重复；
- 构造出的局面不满足 `core::validate_state`。

协议错误会在 stderr 打印 `Botzone 协议错误: ...`；AI 内部异常打印 `Botzone 内部错误: ...`。两种情况 stdout 都只输出一行：

```json
{ "response": {} }
```

进程退出码仍为 0。适配层无法得知平台对空 `response` 的具体处理，因此这是“让平台看到本次没有决策”的防御性兜底，不会伪造一步棋。

## 本地验证

```bash
# 协议库单测
ctest --test-dir build -R botzone_protocol --output-on-failure

# 冒烟：空 requests 应返回布阵
echo '{"requests":[],"responses":[]}' | ./build/bin/einstein_botzone
```

协议解析与响应组装在 `src/ui/botzone/botzone_protocol.cpp`，可执行入口在 `src/ui/botzone/main_botzone.cpp`；协议库测试在 `tests/botzone_protocol_test.cpp`。
