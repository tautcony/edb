# Review Findings Fix Checklist

> 来源: `docs/review-2026-10-01/`
> 最后更新: 2026-10-01

状态约定：`[ ]` 未开始；`[~]` 部分完成或平台验证待补；`[x]` 所有必需项完成且有验证证据；`N/A` 明确不适用。

## P2-P2-1 — [P2] Windows 串口写入后清空 TX/RX 队列

- 来源: [Phase 2](./phases/phase-2-transport.md#p2-p2-1-p2-windows-串口每次写入后清空-txrx-队列)
- 位置: `edb/CComHelper.cpp:93-103`
- 总体状态: [~]
- 影响摘要: `PurgeComm` 可能丢弃未发送数据或刚到达的 CDC 状态响应。

### Checklist
- [x] 复现/确认：核对写后 purge 调用和 Windows API 队列清除语义。
- [x] 实施修复：移除 `Write` 写后 purge，保留串口初始化阶段的旧队列清理。
- [~] 回归测试：代码路径已复核；Windows 串口 mock/回环未在本机可用。
- [x] 相邻路径：确认 CDC status 命令后无其他 TX/RX 清队列调用。
- [~] 验证：需 Windows CI 和真实 CDC 状态检查设备验证。
- [~] 发布动作：Windows 发布前完成 CDC 状态检查 smoke test。

## P1-P2-1 — [P2] 重复 `-f` 参数聚合后拒绝

- 来源: [Phase 1](./phases/phase-1-core-flow.md#p1-p2-1-p2-重复--f-参数会被当成一组值并拒绝)
- 位置: `edb/EDBCLIOptions.cpp:12-39`
- 总体状态: [x]
- 影响摘要: 多镜像 CLI 输入不能到达后续烧录流程。

### Checklist
- [x] 复现/确认：CLI11 `TakeAll` 聚合重复 option 结果。
- [x] 实施修复：提取文件 option 注册并启用 `trigger_on_parse`，逐次解析。
- [x] 回归测试：新增多个 `-f`、页面顺序和 `b` 标记测试。
- [x] 相邻路径：真实 CLI smoke check 到达后续 `--check`/flash 参数冲突校验。
- [x] 验证：完整 `scripts/test.sh` 通过，CLI smoke check 通过。
- [x] 文档：README 已说明重复 `-f` 和示例。

## P2-P2-2 — [P2] Windows COM10+ 路径缺少设备命名空间前缀

- 来源: [Phase 2](./phases/phase-2-transport.md#p2-p2-2-p2-windows-com10-及以上端口路径没有设备命名空间前缀)
- 位置: `edb/CComHelper.cpp:19-43`
- 总体状态: [~]
- 影响摘要: 自动发现或显式选择 COM10 及更大编号时，原路径无法通过 `CreateFileW` 打开。

### Checklist
- [x] 复现/确认：Windows `CreateFile` 对大于 COM9 的路径要求 `\\.\`。
- [x] 实施修复：标准 COM 名称在 `Open` 中统一规范化，已带前缀和自定义路径保持原样。
- [x] 回归测试：新增 COM1/9/10/56、已带前缀及非 COM 字符串测试。
- [x] 相邻路径：显式端口和自动发现都走 `CComHelper::Open`。
- [~] 验证：Windows 测试已加入测试目标；本机 macOS 不能执行该 Windows 测试。
- [x] 发布动作：N/A（不改变设备协议）。

## P2-P2-3 — [P2] POSIX 串口单次写调用不能完成短写

- 来源: [Phase 2](./phases/phase-2-transport.md#p2-p2-3-p2-posix-串口写入不能完成短写)
- 位置: `edb/EDBSerialPosix.cpp:23-42,145-147`
- 总体状态: [x]
- 影响摘要: CDC `getstatus\n` 命令部分写入时，状态检查会失败且不会补发余下字节。

### Checklist
- [x] 复现/确认：代码路径显示上层要求完整命令写入。
- [x] 实施修复：新增 write-all 循环，重试 `EINTR`，其他错误返回已写字节数。
- [x] 回归测试：fake syscall 覆盖短写、EINTR 和部分写后错误。
- [x] 相邻路径：串口读仍按 chunk 交给 status reader 累积；MSC I/O 不经过该 helper。
- [x] 验证：完整 `scripts/test.sh` 与 `scripts/lint.sh` 通过。
- [x] 发布动作：N/A（无协议格式变化）。

## P3-P2-1 — [P2] CMake 最低版本与 FetchContent 参数版本不匹配

- 来源: [Phase 3](./phases/phase-3-platform.md#p3-p2-1-p2-cmake-最低版本低于-fetchcontent-参数的引入版本)
- 位置: `CMakeLists.txt:1,10-14`
- 总体状态: [~]
- 影响摘要: 声明支持的 CMake 3.16–3.23 不支持使用的 `DOWNLOAD_EXTRACT_TIMESTAMP` 参数。

### Checklist
- [x] 复现/确认：参数从 CMake 3.24 开始提供。
- [x] 实施修复：最低版本提升到 3.24，README 同步标明。
- [x] 相邻路径：项目所有平台构建入口共用同一 CMake 声明。
- [x] 验证：本机 CMake 4.x 下完整配置、构建和 CTest 通过。
- [~] 最低版本复测：尚未使用 CMake 3.24 本身运行配置。
- [x] 文档：README 已说明 CMake 3.24+。
