# AI 性能基准

这份文档记录当前 AI 在**固定机器、固定构建类型、固定随机种子**下的单步延迟和对随机基线胜率。它只描述当前实现，不是跨机器的性能承诺。

## 测量环境

| 项目 | 值 |
| --- | --- |
| CPU | Intel Core i9-14900HX，24 物理核 / 32 线程 |
| 内存 | 31.7 GB |
| 操作系统 | Windows 11 家庭版 中文版 10.0.26200 |
| 编译器 | GCC 16.2.0（MSYS2 UCRT64） |
| CMake | 4.4.3 |
| 生成器 | MinGW Makefiles |
| 构建类型 | Release（`-O2`） |
| 构建选项 | `-DEINSTEIN_BUILD_GUI=OFF -DEINSTEIN_BUILD_TESTS=OFF` |

## 复现命令

benchmark 目标随正常构建编译，但不注册为 CTest 用例。为了避免 Debug 的 `-Og` 污染耗时数据，建议单独建 Release 目录：

```bash
cmake -S . -B build-bench -G "MinGW Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DEINSTEIN_BUILD_GUI=OFF \
  -DEINSTEIN_BUILD_TESTS=OFF
cmake --build build-bench --target einstein_benchmark --parallel 8
./build-bench/bin/einstein_benchmark
```

`build-bench/` 是临时目录，不必提交；benchmark 源码在 `benchmarks/ai_benchmark.cpp`。

## 测量方法

### 单步延迟

对四种合法局面各测 20 次，先跑一次 warmup，再用 `std::chrono::steady_clock` 记录每次 `ai::decide_move(state, SearchConfig{4})` 的墙钟耗时：

- **opening**：6v6 初始布阵，骰点 5，恰好选中 5 号，只有一枚可选子；
- **fallback**：双方去掉 5 号后掷出 5，4/6 两枚都可选，分支因子最大；
- **midgame**：双方各 4 子，骰点 3；
- **endgame**：双方各 2 子，骰点 2。

表中给出中位数与 p95。p95 用最近秩法计算，更接近“用户偶尔遇到的那次卡顿”，而不是只看平均值。

### 对随机基线胜率

- 搜索版（深度 4）与随机版交替执红/执蓝，各 20 局，共 40 局；
- 每局使用固定种子的骰子序列，搜索/随机共享同一序列；
- 固定初始布阵，不跑布阵阶段；
- 单局最多 160 手；达到上限、无合法着法或异常都记为和棋，异常另计数；
- 胜率 = 搜索版胜局 / 40。

## 结果

### 单步延迟（单位：ms）

| 局面 | 中位数 | p95 | 最小值 | 最大值 |
| --- | ---: | ---: | ---: | ---: |
| opening | 7.56 | 8.33 | 7.16 | 8.59 |
| fallback | 23.98 | 24.81 | 23.08 | 25.14 |
| midgame | 7.91 | 8.25 | 7.61 | 8.41 |
| endgame | 0.84 | 0.92 | 0.79 | 0.93 |

多次重复的中位数范围：opening 7.56–8.13，fallback 23.98–25.88，midgame 7.91–8.44，endgame 0.84–0.87；对应 p95 范围分别为 8.21–9.65、24.81–28.67、8.25–9.25、0.92–1.36。

### 对随机基线

| 指标 | 结果 |
| --- | ---: |
| 搜索版胜 | 38 / 40（95.0%） |
| 随机版胜 | 2 / 40（5.0%） |
| 和棋 | 0 |
| 异常 | 0 |
| 平均步数 | 14.8 手 |
| 40 局总耗时 | 5.32 s |
| 进程峰值工作集 | 约 9.4 MB |

三次重复的胜负结果完全一致（固定种子），总耗时在 4.96–6.27 s 之间波动。峰值工作集由 PowerShell 以 50 ms 间隔采样，只作数量级参考。

## 解读

- 当前深度 4 的搜索在最大分支的 fallback 局面上 p95 也不到 30 ms；GUI 另有三段过渡延迟（掷骰确认 400 ms、走子后衔接 500 ms、AI 思考 800 ms，见 `src/ui/gui/views/game_widget.h`），因此当前实现下 AI 计算不是界面卡顿的主要来源。
- 搜索版对随机基线 40 局赢 38 局，说明评估和搜索方向明显优于随机；但 40 局样本有统计噪声，不能外推成“对任何对手 95% 胜率”。
- 固定种子下多次重复的胜负一致，耗时只在小范围内波动；这说明 benchmark 本身是稳定的，但机器负载、编译器和 CPU 变化仍会影响绝对耗时。

## 局限

- 局面是人工构造的代表性状态，不是从真实对局中采样；不覆盖所有分支形态。
- 不含布阵阶段、JSON 解析、GUI 事件循环和数据库 I/O；这些开销在 Botzone/GUI 实测中另算。
- 只测一台 Windows 机器和 GCC 16.2.0；不能代表其他平台或编译器。
- 40 局胜率只用于和随机基线做粗对比，不构成棋力证明。

## 相关文档

- [使用说明](USAGE.md)
- [设计取舍与边界](DESIGN.md)
- [构建与测试](BUILD.md)
