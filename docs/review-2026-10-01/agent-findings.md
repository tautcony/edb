# Sub Agent 候选发现与核验记录

> 日期: 2026-10-01
> 范围: EDB 全仓库

## 分派概览

- `core_flow`: CLI、烧录流程、协议、utils 与测试；重复 `-f` 候选采纳并修复。页 0 擦除、最大页和 suspend 恢复依赖设备端语义，保留为疑点。
- `platform_transport`: 三平台 transport、POSIX serial、Windows serial/registry；Windows purge、COM 路径和 POSIX 单次写候选采纳。主审核对调用方后，将 POSIX 候选收窄为短写；读取分段由 `readCdcStatus` 累积，不作为缺陷。
- 主审独立审查 CMake、CI、build/test/lint/package，并交叉检查平台行为和证据边界。

## 候选发现与处置

| 候选 | 来源 | 处置 | 修复/核验 |
|---|---|---|---|
| Windows 写后 purge 清 TX/RX | platform_transport | 采纳为 P2-P2-1 | 移除写后 `PurgeComm`；本机未运行 Windows CDC 回环验证。 |
| Windows COM10+ 路径缺少 `\\.\` | platform_transport | 采纳为 P2-P2-2 | 统一规范化标准 COM 名称；增加 Windows 条件测试，本机 macOS 未运行。 |
| POSIX serial 单次 read/write | platform_transport | 收窄为 P2-P2-3 | read chunk 在 `readCdcStatus` 中被累积；write 单次发送可能丢余下的 `getstatus\n`，已增加 write-all/EINTR 回归测试并通过。 |
| POSIX 自动选择首个 glob 结果 | platform_transport | 降级为疑点 | 多设备时可能选错，但自动发现意图和 `--port` 覆盖策略未定义。 |
| Windows MSC 512 对齐与 4K 扇区 | platform_transport | 降级为疑点 | 仓库无目标卷 geometry，需设备验证。 |
| block 0 未擦除 | core_flow | 降级为疑点 | 无设备端协议/布局证据确认 block 0 可擦。 |
| 页计数 uint32 回绕 | core_flow | 降级为疑点 | device maximum page 未知，超容量页可能先被设备拒绝。 |
| 重复 `-f` callback 聚合全部 token | core_flow | 采纳为 P1-P2-1 | 提取 file option 并启用 `trigger_on_parse`；单测、真实 CLI smoke check 通过。 |
| VM suspend 后失败不恢复 | core_flow | 降级为疑点 | 可能是故障保留策略；文档未定义失败恢复要求。 |
| CMake minimum 与 `DOWNLOAD_EXTRACT_TIMESTAMP` | 主审 | 采纳为 P3-P2-1 | minimum 更新为 3.24，README 同步；本机配置/构建通过。 |

## 复核说明

Windows purge 原候选被主审限定为 CDC 状态检查路径：CLI 当前禁止通过 CDC 烧录，所以影响不描述为烧录失败。Microsoft 文档确认 `PURGE_TXCLEAR` 会删除尚未发送的数据。Windows COM 路径依赖 Microsoft `CreateFile` 文档。其余采纳项均由主审复读代码和调用链。
