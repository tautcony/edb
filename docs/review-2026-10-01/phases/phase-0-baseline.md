# Phase 0 审查报告：Baseline

> 日期: 2026-10-01
> 范围: EDB 全仓库
> 分析深度: systemic
> 审查方式: 并行探查 + 主审交叉核验

## 项目结构与架构事实

- README 将 EDB 描述为 C++ 命令行固件烧录工具，支持串口和 USB MSC，包含烧录、切换 MSC、检查传输和重启操作。
- CMake 以 C++11 构建；`edb/main.cpp` 负责 CLI，`EDBInterface` 执行设备协议流程，`EDBTransport` 抽象连接，平台源文件实现具体 I/O。
- CMake 按平台选择 Windows、macOS、Linux 传输实现；单元测试覆盖 interface/utils，并用 transport stub 隔离硬件。
- CI 对 Linux/macOS/Windows 运行静态分析；Linux/macOS/Windows 和 Linux ARM64 运行构建、测试或打包工作流。
- 未发现既有 `docs/` 架构文档或历史 review 报告；当前工作树起始时干净。

## 审查分层

1. 接入与应用流程：CLI 参数、烧录状态推进、协议失败和重启行为。
2. 传输与基础设施：平台发现、串口/MSC I/O、读写边界、超时与资源生命周期。
3. 平台与横切保障：CMake 目标选择、依赖获取、CI、lint、测试和打包脚本。
4. 交叉反证：未知输入、协议失败、部分写入、资源清理、平台差异和编码/长度边界。

## 漏检风险清单

- 默认分支 / 非法 CLI 或协议状态：待各 phase 检查。
- 异步失败 / 传输中断 / 超时：待各 phase 检查。
- 烧录过程的半完成状态和恢复路径：待各 phase 检查。
- 渲染 / 导出链路：仓库当前未见 UI 或文档导出链路；核查文件打包路径及内容。
- 隐式协议：重点核对长度、checksum、端口命名、串口模式和平台 I/O 语义。

## 未覆盖区域

- 尚未完成代码 phase 审查；结论将在后续 phase 报告和 summary 中更新。
- 当前主机为 macOS；Windows 原生 API / MSVC 运行时行为需依赖静态代码证据和 CI 配置，无法在本机直接观测。
