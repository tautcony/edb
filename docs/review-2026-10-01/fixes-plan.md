# 修复计划与执行状态

## Batch A：Windows 串口队列正确性

- Finding: [P2-P2-1](./phases/phase-2-transport.md#p2-p2-1-p2-windows-串口每次写入后清空-txrx-队列)
- 已完成：移除 `CComHelper::Write` 写后 purge。
- 待完成：Windows CI 运行、CDC 状态检查回环/实机验证。
- 涉及：`edb/CComHelper.cpp`。

## Batch B：其余修复

- [P1-P2-1](./phases/phase-1-core-flow.md#p1-p2-1-p2-重复--f-参数会被当成一组值并拒绝)：重复 `-f` 独立解析；CLI 单测、smoke check 和文档已完成。
- [P2-P2-2](./phases/phase-2-transport.md#p2-p2-2-p2-windows-com10-及以上端口路径没有设备命名空间前缀)：COM 路径规范化与 Windows 单测已实现；待 Windows runner 验证。
- [P2-P2-3](./phases/phase-2-transport.md#p2-p2-3-p2-posix-串口写入不能完成短写)：write-all、EINTR 处理及 fake syscall 测试已完成。
- [P3-P2-1](./phases/phase-3-platform.md#p3-p2-1-p2-cmake-最低版本低于-fetchcontent-参数的引入版本)：CMake minimum/README 已更新并在当前 CMake 配置构建通过；待最低版本 3.24 runner 验证。

本批变更均已实现。提交前已运行完整测试脚本、静态分析和格式检查；Windows 特定测试未能在本机执行，状态和证据见 [`fix-checklist.md`](./fix-checklist.md)。
