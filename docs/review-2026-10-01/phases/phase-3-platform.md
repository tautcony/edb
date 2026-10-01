# Phase 3 审查报告：构建、CI 与发布脚本

> 日期: 2026-10-01
> 文件数: 12
> 发现: P0(0) / P1(0) / P2(1) / INFO(0)
> 导航: [返回 review index](../index.md) | [查看修复 checklist](../fix-checklist.md)

## 已审查文件

- `CMakeLists.txt`, `.github/workflows/build.yml`, `.gitignore`
- `scripts/build.sh`, `scripts/build.ps1`
- `scripts/test.sh`, `scripts/test.ps1`
- `scripts/lint.sh`, `scripts/lint.ps1`, `scripts/clang-format-check.sh`
- `scripts/package.sh`, `scripts/package.ps1`

## Findings

### P3-P2-1: [P2] CMake 最低版本低于 FetchContent 参数的引入版本

- 位置: `CMakeLists.txt:1`, `CMakeLists.txt:10-14`
- 触发条件: 使用 CMake 3.16–3.23 配置工程。
- 影响: 工程声明最低支持 CMake 3.16，但 `DOWNLOAD_EXTRACT_TIMESTAMP` 在 CMake 3.24 才加入 `ExternalProject`/`FetchContent` 参数。低版本无法识别该参数，配置会因 FetchContent 声明失败；用户按工程声明选择 3.16–3.23 时无法构建。
- 修复方向: 将最低版本提升至 3.24，或按 `CMAKE_VERSION` 条件传入该参数并兼容旧版本。官方文档确认该参数从 3.24 开始提供：[CMake 3.24 release notes](https://cmake.org/cmake/help/v3.24/release/3.24.html)。
- 置信度: 高
- 证据类型: 静态配置 + 官方版本文档；本机未安装 3.16–3.23 进行旧版本复现。

#### 修复 Checklist

- [x] 复现/确认：官方文档注明参数从 3.24 提供，而项目之前声明 3.16。
- [x] 实施修复：将 `cmake_minimum_required` 提升到 3.24。
- [~] 回归测试：在本机 CMake 4.x 配置/构建通过；没有 CMake 3.24 二进制做最低版本复测。
- [x] 验证相邻路径：Linux/macOS/Windows 共用顶层 CMake 最低版本。
- [x] 执行验证并记录证据：`BUILD_DIR=build-tests ./scripts/test.sh` 成功完成配置、构建和 CTest。
- [x] 发布/监控/文档动作：README 已注明 CMake 3.24+。
- 当前状态: [~] 修复完成；待最低版本 CMake 复测
- 负责人/批次: 未分配 / Batch B
- 最后更新: 2026-10-01

## 漏检复盘

- CI 的静态分析覆盖 Linux/macOS/Windows；构建矩阵覆盖 Linux x86_64/arm64、macOS arm64、Windows x86_64。
- Unix 和 PowerShell 脚本均检查 native command exit status；package 脚本先构建，再替换包目录/归档。
- `BUILD_TESTING` 默认关闭，测试入口显式打开；测试目标由 CMake 注册到 CTest。
- 检查了临时目录/构建产物排除、安装目标与上传 artifact 路径。未发现其他确认问题。

## 未覆盖区域

- 未在 Windows 主机运行 PowerShell、MSVC/clang-cl lint 或打包。
- 未在 CMake 3.16–3.23 运行配置复现；版本不兼容由参数引入版本确认。
