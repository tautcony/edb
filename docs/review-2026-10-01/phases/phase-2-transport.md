# Phase 2 审查报告：平台传输与 I/O

> 日期: 2026-10-01
> 文件数: 10
> 发现: P0(0) / P1(0) / P2(3) / INFO(0)
> 导航: [返回 review index](../index.md) | [查看修复 checklist](../fix-checklist.md)

## 已审查文件

- `edb/EDBTransport.h`
- `edb/EDBTransportUnix.cpp`, `edb/EDBTransportMac.cpp`, `edb/EDBTransportWindows.cpp`
- `edb/EDBSerialPosix.cpp`, `edb/EDBSerialPosix.h`
- `edb/CComHelper.cpp`, `edb/CComHelper.h`
- `edb/EDBWinReg.cpp`, `edb/EDBWinReg.h`
- 补读 `edb/EDBInterface.cpp` 的读写/ACK 调用路径和 `CMakeLists.txt` 的平台选择。

## Findings

### P2-P2-1: [P2] Windows 串口每次写入后清空 TX/RX 队列

- 位置: `edb/CComHelper.cpp:73-83`
- 触发条件: 任意 Windows 串口命令或数据写入成功；`Write` 随后无条件调用 `PurgeComm`，同时传入 `PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR`。
- 影响: TX 队列中的未发送字节会被丢弃，RX 队列中已到达的 CDC 状态响应也会被清除。当前 CLI 只把 CDC 用于状态检查，因此表现为状态检查间歇失败；MSC 烧录不经过此串口路径。Windows API 文档明确指出清空输出缓冲区会导致其中字符不被发送。
- 修复方向: 不要在正常写入后清空 TX/RX。若要丢弃旧输入，只在发送新命令前按协议需要清理 RX；需要确保发送完成时使用适当的 flush/完成语义，而不是 `PURGE_TXCLEAR`。
- 置信度: 高
- 证据类型: 静态调用路径 + Microsoft `PurgeComm` 文档

#### 修复 Checklist

- [x] 复现/确认：静态核对 `Write` 后立即 purge，以及 Windows 文档描述的清队列语义。
- [x] 实施修复：移除 `CComHelper::Write` 的写后 purge；open/configure 时仍只清理旧队列。
- [~] 回归测试：无 Windows 串口回环设备；需要 Windows CI/设备确认写后响应保留。
- [x] 验证相邻路径：检查 command/status check 写路径和初始化时清理位置。
- [~] 执行验证并记录证据：本机静态检查通过；Windows 原生串口运行时未验证。
- [~] 发布/监控/文档动作：发布前在 Windows CDC 状态检查设备上验证。
- 当前状态: [~] 已移除写后 purge；待 Windows 串口运行时验证
- 负责人/批次: 未分配 / Batch A
- 最后更新: 2026-10-01

### P2-P2-2: [P2] Windows COM10 及以上端口路径没有设备命名空间前缀

- 位置: `edb/CComHelper.cpp:19-43`, `edb/EDBTransportWindows.cpp:59-61`, `edb/EDBWinReg.cpp:109-127`
- 触发条件: 自动发现或 `--port COM10` 及更大端口号后，端口名直接传给 `CreateFileW`。
- 影响: Windows 的 `CreateFile` 要求大于 COM9 的端口使用 `\\.\COM10` 形式；当前自动发现返回 `COMn`，因此高编号端口无法打开。
- 修复方向: 在 Windows 串口适配层统一规范化标准 `COMn` 名称；已带设备前缀或自定义路径保持原样。
- 置信度: 高
- 证据类型: 静态路径 + Microsoft `CreateFile` 文档

#### 修复 Checklist

- [x] 复现/确认：Microsoft `CreateFile` 文档要求 COM9 以上使用设备路径格式。
- [x] 实施修复：`CComHelper::Open` 统一规范化标准 COM 名称并保留已带前缀路径。
- [x] 回归测试：新增 COM1、COM9、COM10、COM56、已带前缀和非 COM 路径单测。
- [x] 验证相邻路径：显式端口与注册表自动发现都经过 `CComHelper::Open`。
- [~] 执行验证并记录证据：Windows 单测已加入 CI 构建；本机 macOS 无法运行该平台测试。
- [x] 发布/监控/文档动作：N/A（路径兼容修复；无需设备固件迁移）。
- 当前状态: [~] 已实现路径规范化和 Windows 单测；待 Windows CI 运行
- 负责人/批次: 未分配 / Batch B
- 最后更新: 2026-10-01

### P2-P2-3: [P2] POSIX 串口写入不能完成短写

- 位置: `edb/EDBSerialPosix.cpp:23-42`, `edb/EDBSerialPosix.cpp:145-147`, `edb/EDBInterface.cpp:291-319`
- 触发条件: POSIX 串口的 `write` 只接受 `getstatus\n` 命令的一部分字节。
- 影响: `readCdcStatus` 认为短写是发送失败并退出状态检查；剩余命令字节不会补发。串口读取本身按 chunk 追加到输出缓冲，因此不是此 finding 的触发路径。
- 修复方向: 对未写部分循环调用系统 `write`，重试 `EINTR`；零写或其他错误时返回实际完成字节数，让上层保持失败传播。
- 置信度: 中高
- 证据类型: 静态路径

#### 修复 Checklist

- [x] 复现/确认：代码调用路径显示状态命令短写会被当作发送失败。
- [x] 实施修复：增加 POSIX `writeAll`，循环处理短写并重试 `EINTR`。
- [x] 回归测试：fake write syscall 覆盖部分写、EINTR 和部分进度后错误。
- [x] 验证相邻路径：状态读取仍按 chunk 追加；MSC 传输不使用 POSIX serial helper。
- [x] 执行验证并记录证据：`scripts/test.sh`、`scripts/lint.sh` 通过。
- [x] 发布/监控/文档动作：N/A（无协议格式变更）。
- 当前状态: [x] 已完成并有本机验证证据
- 负责人/批次: 未分配 / Batch B
- 最后更新: 2026-10-01

## 未决疑点

- POSIX 自动发现按 glob 顺序选择首个 `/dev/ttyACM*`、`/dev/ttyUSB*` 或 macOS 对应设备；多设备时可能选错，但当前无法从命名确认用户预期，也有 `--port` 覆盖入口。
- Windows MSC 使用 `FILE_FLAG_NO_BUFFERING`，主机 buffer 对齐为 512 字节；设备逻辑扇区若要求更大对齐可能导致 I/O 失败。仓库没有目标卷 geometry 信息，需真实设备验证。

## 漏检复盘

- 核对了设备查找失败后的句柄关闭、串口 open 配置失败的关闭路径、MSC 双端口打开失败时的 cleanup，以及析构 close。
- 检查了 POSIX 短读/短写、Windows 队列 purge、ACK 读取调用、重启断开语义和跨平台源文件选择。
- 未连接硬件；MSC 扇区大小、Disk Arbitration 失败路径及各 USB 驱动实际返回分段未能运行时观察。
