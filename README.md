# 键鼠大师 KeyMouseMaster v1.0

Windows 键鼠自动化工具，支持鼠标连点、键盘连点、键鼠脚本录制与回放。

## 第一版已实现功能

- **鼠标连点**：支持左/右/中/侧键 × 单击/双击/长按/仅按下/仅释放
- **键盘连点**：支持普通键、组合键、功能键、多种输入模式
- **脚本录制**：全局钩子录制鼠标移动/点击/滚轮/键盘事件
- **脚本回放**：支持多速度回放、循环执行、事件跳过
- **脚本保存/导入**：`.kms` JSON 格式，安全写入 (QSaveFile)
- **多显示器**：支持负坐标、多屏不同DPI/缩放、显示器变化检测
- **全局快捷键**：RegisterHotKey 实现，软件不在前台时仍可触发
- **紧急停止**：全局紧急停止 + 输入状态复位
- **系统托盘**：最小化到托盘，区分运行/空闲/错误状态
- **配置持久化**：QSettings 保存所有参数
- **基础脚本编辑**：删除/启用禁用/上下移动/插入等待/清空
- **日志系统**：文件日志 + 大小轮转

## 暂未实现功能（第二/三版）

- 图像识别点击、OCR
- 目标窗口绑定、窗口相对坐标
- 条件执行、循环块编辑器、脚本变量
- 插件系统、云端同步、自动更新
- 开机启动、复杂主题切换
- EDID 级显示器唯一识别

## 项目目录

```
KeyMouseMaster/
├── KeyMouseMaster.pro      # qmake 工程文件
├── README.md
├── TODO.md
├── CHANGELOG.md
├── docs/
│   ├── architecture.md
│   └── test_plan.md
├── resources/
│   ├── resources.qrc
│   ├── app.manifest          # Windows DPI 感知清单
│   ├── icons/
│   └── styles/
├── src/
│   ├── main.cpp
│   ├── ui/                   # 界面层
│   ├── core/                 # 核心引擎
│   ├── models/               # 数据模型
│   ├── platform/windows/     # Windows API 封装
│   ├── settings/             # 配置管理
│   └── utils/                # 工具类
├── samples/
└── tests/
```

## 开发环境

- **Qt 版本**：优先兼容 Qt 5.15，允许 Qt 6
- **编译器**：MinGW 64位 / MSVC 64位
- **构建系统**：qmake
- **目标系统**：Windows 10/11 64位
- **C++ 标准**：C++17

## qmake 编译步骤

### MinGW 编译

```bash
qmake KeyMouseMaster.pro
mingw32-make -j4
```

### MSVC 编译

```bash
# 在 Visual Studio 开发者命令提示符中
qmake KeyMouseMaster.pro
nmake
# 或使用 jom
jom
```

### 输出

- Debug: `build/debug/KeyMouseMaster_d.exe`
- Release: `build/release/KeyMouseMaster.exe`

## 运行方法

编译后直接运行 `build/release/KeyMouseMaster.exe`。

如需管理员权限（目标程序以更高权限运行时）：
- 右键 → 以管理员身份运行

## 多显示器说明

- 支持左侧/上方显示器（负坐标）
- 支持不同分辨率、不同DPI/缩放的显示器
- 脚本默认使用"显示器相对坐标"，回放时自动映射到当前显示器位置
- 录制后更改显示器排列会提示用户确认
- 断开脚本引用的显示器时禁止执行，需用户选择替代显示器

## 高 DPI 说明

- 应用程序声明为 Per-Monitor DPI Aware V2
- 区分 Qt 逻辑坐标、Windows 物理坐标、虚拟桌面坐标、显示器内部坐标
- 所有坐标转换通过 `CoordinateMapper` 统一处理

## 权限问题

- 软件默认以普通权限运行
- 如果目标程序以更高权限运行（如管理员权限），输入模拟可能被 UIPI 阻止
- 遇到此情况请以管理员身份运行键鼠大师

## 脚本格式说明

脚本文件扩展名 `.kms`，UTF-8 JSON 格式：

```json
{
  "format": "KeyMouseScript",
  "version": 1,
  "name": "示例脚本",
  "description": "",
  "created_at": "2025-01-01T00:00:00",
  "coordinate_mode": "MonitorRelative",
  "desktop": { "x": 0, "y": 0, "width": 3840, "height": 1080 },
  "monitors": [...],
  "playback": { ... },
  "events": [...]
}
```

每个事件包含：类型、时间戳、坐标（虚拟桌面+显示器内部+比例）、按键信息等。

## 快捷键说明

| 功能 | 默认快捷键 | 说明 |
|------|-----------|------|
| 鼠标连点启动 | 用户自定义 | 在鼠标连点页面设置 |
| 鼠标连点停止 | 用户自定义 | 在鼠标连点页面设置 |
| 键盘连点启动 | 用户自定义 | 在键盘连点页面设置 |
| 键盘连点停止 | 用户自定义 | 在键盘连点页面设置 |
| 紧急停止 | Ctrl+Shift+F12 | 全局停止所有任务并释放输入 |
| 输入状态复位 | Ctrl+Shift+R | 释放所有被压住的按键 |

快捷键使用 Windows `RegisterHotKey` API，系统级别生效。

## 已知问题

1. 本版本在 Linux 上使用 Qt 5.15.3 编译通过，但 Windows API 功能（钩子、SendInput、热键）使用非 Windows 平台桩实现，仅在 Windows 上实际生效
2. 侧键（XButton）识别依赖硬件和驱动支持
3. F12 键在某些系统上可能被其他程序占用，导致注册失败
4. 部分安全软件可能阻止全局钩子安装

## 后续版本计划

### 第二版
- 图像识别点击
- 目标窗口绑定
- 循环块编辑器
- 脚本变量支持
- 自动更新

### 第三版
- OCR 文字识别
- 插件系统
- 云端脚本同步
- EDID 显示器识别
- 复杂主题切换

## 许可

MIT License
