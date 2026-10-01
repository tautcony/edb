# 代码审查与修复汇总

> 日期: 2026-10-01
> 范围: EDB 全仓库（CLI、烧录协议、三平台 transport、测试、构建和发布脚本）
> Phase 数: 5
> 发现: P0(0) / P1(0) / P2(5) / INFO(0)
> 修复状态: 5 项实现已完成；3 项本机测试完整通过，2 项 Windows/最低 CMake 版本验证待补

## 发现与修复

- [P2-P2-1](./fix-checklist.md#p2-p2-1--p2-windows-串口写入后清空-txrx-队列)：移除 Windows 串口写后清队列；待 Windows CDC 运行时验证。
- [P1-P2-1](./fix-checklist.md#p1-p2-1--p2-重复-f-参数聚合后拒绝)：按 option occurrence 解析重复 `-f`，新增单测和 CLI smoke check，已通过。
- [P2-P2-2](./fix-checklist.md#p2-p2-2--p2-windows-com10-路径缺少设备命名空间前缀)：规范化 Windows COM 端口路径；测试已加入 Windows 测试目标，待 Windows runner 执行。
- [P2-P2-3](./fix-checklist.md#p2-p2-3--p2-posix-串口单次写调用不能完成短写)：增加 POSIX write-all 和 EINTR 重试，新增 fake syscall 测试，已通过。
- [P3-P2-1](./fix-checklist.md#p3-p2-1--p2-cmake-最低版本与-fetchcontent-参数版本不匹配)：最低 CMake 版本改为 3.24，README 已同步；本机当前 CMake 配置/构建通过，未单独跑 CMake 3.24。

## 架构结论

- **事实：**`main.cpp` 接收 CLI，`EDBInterface` 编排设备命令，`EDBTransport` 委派平台 I/O；POSIX CDC 状态 reader 会把多次读取的 chunk 追加成一段输出。
- **推定（高置信度）：**串口的写入边界必须完整发送小型控制命令；Windows 写后 purge 和 POSIX 单次写均可能破坏该要求。读取分段本身由状态检查循环支持，因此原“短读导致 ACK/checksum 失败”候选已收窄并未作为 finding 保留。
- **建议：**保持 transport 的 write-all/失败语义一致；平台相关 Windows 串口行为需要 CI 与目标设备运行时验证。

## 验证

- `BUILD_DIR=build-tests ./scripts/test.sh`: 配置、构建、CTest 成功；19 个单测通过。
- CLI smoke check：两个 `-f` 镜像成功完成解析，并到达后续动作组合校验，未触发 `--file requires`。
- `./scripts/lint.sh`: clang-tidy 和 cppcheck 通过，新增 CLI/POSIX 源码已加入 lint 输入。
- `./scripts/clang-format-check.sh`: 通过；新增测试文件也单独通过 clang-format 检查。
- Windows COM helper 单测添加到 Windows test target，本机 macOS 未执行；Windows purge 尚需 CDC 回环/实机验证。
- 本机测试先前遇到系统默认 SDK 的 linker/TAPI 不兼容；使用 Xcode 26 SDK 后应用和测试目标都成功构建。

## 未决疑点与限制

页 0 是否允许擦除、最大 flash 页、VM suspend 失败恢复和 MSC 逻辑扇区对齐仍需要设备端协议或实机证据，未纳入确认 finding。未在 Windows/Linux 原生环境运行完整 workflow。

差异化反证已重新检查未知 CLI、传输失败、状态写入顺序、资源关闭、打包清理、路径/校验和边界及平台实现选择；没有新增确认问题。

## 详细报告

- [Phase 0 Baseline](./phases/phase-0-baseline.md)
- [Phase 1 核心流程](./phases/phase-1-core-flow.md)
- [Phase 2 平台传输](./phases/phase-2-transport.md)
- [Phase 3 构建与发布](./phases/phase-3-platform.md)
- [Phase 4 跨模块复查](./phases/phase-4-crosscutting.md)
- [逐项修复 checklist](./fix-checklist.md)
- [修复批次](./fixes-plan.md)
- [Agent 候选核验](./agent-findings.md)
