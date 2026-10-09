# Bluetooth-keyboard

基于 CH585M 的 48 键触摸键盘固件，支持 USB 有线键盘、BLE 蓝牙键盘，以及 NFC 辅助蓝牙配对。

硬件与原始项目来自 [嘉立创开源硬件平台的 PCB Bluetooth Keyboard 项目](https://oshwhub.com/zh_x/pcbbluetoothkeyboard)。本仓库保存固件工程及后续双模键盘修改；硬件资料请参考原项目。

## 当前功能

- USB 主机完成枚举后，有线输入优先；未识别到 USB 主机时使用已连接的 BLE 设备，否则等待连接。
- USB 使用 CH585M 的 USBHS 控制器，工作在 12 Mbps 全速模式：PB12 为 D−，PB13 为 D+。
- 充电器只供电、不完成 USB 枚举，不会在上电时进入有线模式。
- USB 空闲挂起时保留配置和按键队列，主机允许远程唤醒后可通过按键唤醒；恢复后继续发送按下和松开事件。
- 保留原触摸扫描、滤波和键位映射，以及先触摸修饰键再触摸普通键的组合键用法。Fn 功能仍未定义。
- 保留 NFC 辅助蓝牙配对功能。

原理图没有独立 VBUS 检测输入。如果在电脑休眠期间拔线，控制器仍保持挂起状态，软件不能严格区分拔线与休眠。这一边界需要实机验证；要可靠区分，应增加 VBUS 检测电路。

## 编译

1. 安装 MounRiver Studio 2 及 WCH RISC-V 工具链。
2. 打开 `PCB_Bluetooth_Keyboard.wvproj`，选择 `obj` 构建配置并重新编译。
3. 通过 ISP 或下载器烧录生成的 `obj/PCB_Bluetooth_Keyboard.hex`。

`obj` 配置包含 BLE、NFC 和加密模块所需的头文件及静态库路径。已有调试配置包含本机 MRS2 安装路径，换电脑后需在 IDE 中调整。构建输出不纳入 Git。

## 软件验证

在 PowerShell 中运行以下命令，需要主机 GCC 已加入 PATH：

```powershell
./tests/run-tests.ps1
```

测试覆盖 USB 枚举与 HID 控制请求、有线优先、报告顺序、挂起恢复、远程唤醒、无枚举供电，以及实际按键处理分支中的 Esc 和组合键。模拟测试不能代替实机验证。`tests` 已从 MRS 固件构建中排除。

## 工程结构

| 路径 | 用途 |
| --- | --- |
| `APP/usb_keyboard.c` | USB 描述符、控制请求、键盘报告及远程唤醒 |
| `APP/keyboard_transport.c` | USB/BLE 路由、报告队列与发送重试 |
| `APP/hidkbd.c` | 键位映射、组合键与 BLE 应用 |
| `TKYLIB/` | 触摸检测实现及库 |
| `NFCA_LIB/`、`T2T_COMMON/`、`PICC_COMMON/` | NFC 功能 |
| `USB_KEYBOARD_README.txt` | 使用说明及实机检查步骤 |

原工程中的 WCH 源文件和预编译库保留其原有版权与使用声明，请遵循各组件的许可条件。
