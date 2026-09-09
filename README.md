# EDB 工具介绍

EDB (Embedded Device Bootloader) 是一个用于向嵌入式设备烧录固件的命令行工具。该工具支持通过串行端口或USB MSC高速模式与设备通信，并提供固件烧录、重启等功能。

## 功能特点

- 通过串行端口或USB MSC高速模式与设备通信
- 支持烧录二进制固件文件到设备的指定页面
- 支持设置启动镜像
- 支持操作完成后自动重启设备
- 支持进入MSC（Mass Storage Class）模式

## 命令行参数

EDB 工具支持以下命令行参数：

```text
-f <bin file> <page> [b] (Specify 'b' to flash as boot image.)
-p <serial port> Use serial transport instead of USB MSC.
--serial       Auto-detect a serial transport.
-s             Use USB MSC transport.
-r             Reboot if all operations are done.
-m             Enter Mass Storage mode.
-c, --check    Check device connection and mount access only.
```

## 使用示例

### 基本烧录操作

将二进制文件烧录到指定页面：

```bash
edb.exe -f firmware.bin 0
```

### 使用USB MSC高速模式烧录

```bash
edb.exe -f firmware.bin 0 -s
```

### 烧录并重启设备

```bash
edb.exe -f firmware.bin 0 -r
```

### 烧录为启动镜像

```bash
edb.exe -f bootloader.bin 0 b
```

### 进入MSC模式

```bash
edb.exe -m
```

### HP39GII系统安装完整流程

以下是安装操作系统的完整流程示例：

1. 准备edb.exe与sb_loader.exe工具

2. 如果此前未烧录过OSLoader，首先使用sb_loader.exe将OSLoader写入内存：

```bash
.\sb_loader.exe -f OSLoader.sb
```
3. 写入完毕后，等待计算器连接上设备

4. 使用USB MSC高速模式将OSLoader写入闪存（作为启动镜像）：

```bash
.\edb.exe -r -s -f .\OSLoader.sb 1408 b
```
5. 写入完毕后拔下数据线，然后重新插上

6. 然后将系统写入闪存：

```bash
.\edb.exe -r -s -f .\ExistOS.sys 1984
```

## 工作原理

EDB 工具主要通过以下流程与设备交互：

1. 初始化通信接口（串行端口或USB MSC）
2. 向设备发送命令，如重置缓冲区、擦除块、编程页面等
3. 发送固件数据并验证校验和
4. 根据需要设置启动镜像或重启设备

## 项目结构

```
edb/
├── main.cpp                    主程序入口
├── EDBInterface.cpp/h          设备通信接口实现
├── EDBTransport.h              平台传输接口
├── EDBTransportUnix.cpp        Linux 后端
├── EDBTransportMac.cpp         macOS 后端
├── EDBTransportWindows.cpp     Windows 后端
├── EDBSerialPosix.cpp/h        Posix 串口实现
├── CComHelper.cpp/h            Windows 串口通信辅助类
├── WinReg.cpp/h                Windows注册表操作
└── edb.vcxproj                 Visual Studio工程文件
```

## 编译说明

### Windows

该项目使用Visual Studio进行开发和编译。可以通过以下步骤编译项目：

1. 使用Visual Studio打开 `edb.sln` 解决方案文件
2. 选择目标平台和配置（Debug/Release）
3. 构建解决方案

编译完成后，可执行文件将位于相应的输出目录中。

Windows 后端使用 Windows SDK 提供的 Win32 API，链接系统库 `Advapi32.lib`。

### Linux 和 macOS

项目使用 CMake 构建：

```bash
./scripts/build.sh
```

## 注意事项

- 请确保在烧录前选择正确的目标页面
- 使用USB MSC模式时，确保设备已正确连接并识别
- 烧录启动镜像时需要额外添加参数 `b`
- 操作过程中请勿断开设备连接

## 故障排除

- 如果设备无响应，检查连接并尝试重新执行命令
- 确保使用的固件文件格式正确且兼容
- 对于USB连接问题，可以尝试更换USB端口或使用串行端口模式
