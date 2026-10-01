# Phase 4 审查报告：跨模块反证复查

> 日期: 2026-10-01
> 范围: CLI、烧录协议、平台 transport、构建/发布调用链
> 发现: 本 phase 无新增；保留前序 5 项确认 findings。
> 导航: [返回 review index](../index.md) | [查看修复 checklist](../fix-checklist.md)

## 复查项

- 所有操作入口：CLI 对未知选项、页面字符串、log level 和 option 必需值有解析校验；发现重复 `-f` 分组解析不闭合，现已通过逐次 callback 解析并有回归覆盖（见 [P1-P2-1](./phase-1-core-flow.md#p1-p2-1-p2-重复--f-参数会被当成一组值并拒绝)）。
- 同步协议失败：写、读、ACK、checksum、擦除和烧录失败向上返回；POSIX write-all 已补齐短写，Windows 写后 purge 已移除；Windows 行为待原生验证（见 [Phase 2](./phase-2-transport.md)）。
- 状态顺序 / 半完成：device VM suspend 后失败会保留暂停态，但缺少设备端恢复语义，暂列未决疑点；block 0 擦除和最大页范围也因设备约束不足未定性。
- 清理与重建：传输打开失败会关闭已打开句柄；发布归档先构建后清理旧归档，不新增发现。
- 渲染 / 导出 / 编码 / 时间 / checksum：仓库无内容渲染或导出链路；checksum 按 unsigned byte 累加，时间基于 steady clock；路径 UTF-8 转宽字符只用于 Windows 串口名。
- 高风险平台差异：CMake 根据平台选择单个 transport 实现；当前主机只能观察 macOS/Linux 编译配置，Windows 原生行为依赖静态证据。

## 架构结论

- **事实：**CLI 位于 `main.cpp`，烧录流程集中在 `EDBInterface`，平台 I/O 通过 `EDBTransport` 抽象，由 CMake 选择平台实现；同步串口协议和 MSC 文件端口都经过该接口。
- **推定（高置信度）：**CDC 状态读取按 chunk 累积，但控制命令必须完整写出；POSIX write-all 已解决本机可测短写，Windows 队列清理仍需平台运行时确认。
- **建议：**保留 write-all 和错误传播语义，并在 Windows CDC 回环/目标设备上验证写后响应与队列保留。
- 验证信号：POSIX fake write 的短写/EINTR 用例通过；Windows 状态查询在快速响应和写缓冲未排空场景下仍成功。

## 漏检复盘

差异化反证按默认分支、传输失败、状态写入顺序、清理窗口、编码和平台协议重新横扫；未新增确认 finding。页 0 硬件保护规则、Flash 最大页范围、Windows MSC 扇区要求和 VM 失败恢复策略依赖仓库外的设备协议/实现，保留为疑点。

## 未覆盖区域

- `BUILD_DIR=build-tests ./scripts/test.sh` 通过，19 个单测通过；CLI smoke check、lint 和格式检查也通过。
- 未运行 Windows 原生构建/传输，也未连接目标计算器；硬件协议疑点需要设备端规格或实机验证。
