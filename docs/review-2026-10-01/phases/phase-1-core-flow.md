# Phase 1 审查报告：接入与烧录流程

> 日期: 2026-10-01
> 文件数: 10
> 发现: P0(0) / P1(0) / P2(1) / INFO(0)
> 导航: [返回 review index](../index.md) | [查看修复 checklist](../fix-checklist.md)

## 已审查文件

- `README.md`
- `edb/main.cpp`
- `edb/EDBInterface.cpp`, `edb/EDBInterface.h`
- `edb/EDBUtils.cpp`, `edb/EDBUtils.h`
- `edb/EDBLog.h`
- `tests/EDBInterfaceTests.cpp`, `tests/EDBUtilsTests.cpp`, `tests/EDBTransportFactoryStub.cpp`

## Findings

### P1-P2-1: [P2] 重复 `-f` 参数会被当成一组值并拒绝

- 位置: `edb/main.cpp:37-63`, `edb/main.cpp:172-186`
- 触发条件: 同一命令中提供两个或更多 `-f/--file` 选项，例如 `edb -f a.bin 64 -f b.bin 128`。
- 影响: CLI11 的 `TakeAll` 将重复选项收到的参数汇总到 callback 的 `values`；callback 只接受 2 或 3 项，因此汇总后的多镜像输入抛出验证错误。后面的 `imglist` 和逐镜像循环显示程序本身预期可处理多个镜像，但 CLI 无法填充该列表。
- 修复方向: 按选项出现次数分组解析每个 `<path> <page> [b]`，或移除多镜像执行路径并明确拒绝重复选项；与 README 的 CLI 用法保持一致。
- 置信度: 高
- 证据类型: 静态路径 + CLI11 2.4.2 本地依赖实现（`TakeAll` 收集全部结果）

#### 修复 Checklist

- [x] 复现/确认：CLI11 的 `TakeAll` 在 callback 中聚合重复项。
- [x] 实施修复：提取 `addFileOption` 并启用 `trigger_on_parse`，每次出现单独处理。
- [x] 回归测试：新增多镜像、可选 `b` 的 CLI option 单测。
- [x] 验证相邻路径：新增真实 CLI smoke check，检查两个镜像能解析并到达后续动作校验。
- [x] 执行验证并记录证据：`BUILD_DIR=build-tests ./scripts/test.sh` 通过，CLI smoke check 通过。
- [x] 文档动作：README 说明可重复 `-f` 并给出示例。
- 当前状态: [x] 已完成
- 负责人/批次: 未分配 / Batch A
- 最后更新: 2026-10-01

## 未决疑点

- `EDBInterface::flash` 将 `last_block` 初始化为 0，导致页 0–63 不触发 `ERASEB:0`。README 示例使用页 0；如果 block 0 可正常擦除，则已有数据中的 0 位可能无法通过编程恢复为 1，短镜像也可能留下旧内容。但仓库没有设备端协议或 flash 布局文档，无法确认 block 0 是否受保护，因此暂不列为确认 finding。
- 页游标 `uint32_t` 每次加 16，`parsePage` 接受 `UINT32_MAX`，存在算术回绕。但仓库未说明设备最大页号；设备很可能会先拒绝越界 PROGP，现有证据不足以确认低地址写入可达。
- 烧录前暂停 VM 后遇到失败会跳过 resume/reboot。设备可能有意保持暂停以保留故障现场；仓库没有失败恢复承诺，列为策略疑点。

## 漏检复盘

- 未知 CLI 选项由 CLI11 默认拒绝；页面字符串检查非空、负号和 uint32 上界。
- `waitStr` 对 ACK 不匹配会重试两次；checksum 解析要求固定 `CHKSUM:`、两位十六进制和换行。
- 烧录失败会向上传播为非零退出；成功后才恢复 VM 或重启。
- 文件资源由 `flashImg` 所有，解析失败和列表析构会关闭文件。
- 渲染/导出链路不适用；二进制输入、页号和校验和边界已检查，设备容量仍未从协议源确认。

## 未覆盖区域

- 未连接设备；页 0 擦除语义、设备容量和失败后的 VM 恢复要求无法由本仓库确认。
- 修复后已运行单元测试与 CLI smoke check；未连接硬件。
