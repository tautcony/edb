# Code Review Index

> 日期: 2026-10-01
> 范围: EDB 全仓库
> 结论: 发现并修复 5 个 P2；Windows 运行时和最低 CMake 版本验证仍待补。

## 从这里开始

1. 了解结论与验证：读 [`summary.md`](./summary.md)。
2. 继续修复/验证：看 [`fix-checklist.md`](./fix-checklist.md) 和 [`fixes-plan.md`](./fixes-plan.md)。
3. 核对原始证据：进入对应 phase。
4. 检查候选处置：读 [`agent-findings.md`](./agent-findings.md)。

## 当前状态

- P0: 0 / P1: 0 / P2: 5 / INFO: 0
- 已完成: 2 / 部分完成（平台验证待补）: 3 / 未开始: 0

## Findings

| Finding | 级别 | 状态 | 问题 | 证据 |
|---|---|---|---|---|
| [P2-P2-1](./fix-checklist.md#p2-p2-1--p2-windows-串口写入后清空-txrx-队列) | P2 | [~] | Windows 写后清队列 | [Phase 2](./phases/phase-2-transport.md) |
| [P1-P2-1](./fix-checklist.md#p1-p2-1--p2-重复-f-参数聚合后拒绝) | P2 | [x] | 重复 `-f` 解析 | [Phase 1](./phases/phase-1-core-flow.md) |
| [P2-P2-2](./fix-checklist.md#p2-p2-2--p2-windows-com10-路径缺少设备命名空间前缀) | P2 | [~] | COM10+ 路径前缀 | [Phase 2](./phases/phase-2-transport.md) |
| [P2-P2-3](./fix-checklist.md#p2-p2-3--p2-posix-串口单次写调用不能完成短写) | P2 | [x] | POSIX serial 短写 | [Phase 2](./phases/phase-2-transport.md) |
| [P3-P2-1](./fix-checklist.md#p3-p2-1--p2-cmake-最低版本与-fetchcontent-参数版本不匹配) | P2 | [~] | CMake 最低版本 | [Phase 3](./phases/phase-3-platform.md) |

## 文档地图

| 文档 | 用途 |
|---|---|
| [`summary.md`](./summary.md) | 结论、修复状态、测试和限制 |
| [`fix-checklist.md`](./fix-checklist.md) | 每项问题的验证/修复跟踪 |
| [`fixes-plan.md`](./fixes-plan.md) | 修复批次和剩余验证 |
| [`phases/`](./phases/) | 分层证据和漏检复盘 |
| [`agent-findings.md`](./agent-findings.md) | 候选采纳、降级和主审核验 |

## Phase 导航

- [Phase 0: Baseline](./phases/phase-0-baseline.md)
- [Phase 1: 核心流程](./phases/phase-1-core-flow.md)
- [Phase 2: 平台传输](./phases/phase-2-transport.md)
- [Phase 3: 构建与发布](./phases/phase-3-platform.md)
- [Phase 4: 跨模块复查](./phases/phase-4-crosscutting.md)

## 限制

当前环境为 macOS。Windows COM 路径测试已加入 Windows 测试目标，但未在本机执行；Windows serial purge 需 CDC 设备验证；CMake 3.24 最低版本未单独测试。未决硬件假设见 [`summary.md`](./summary.md)。
