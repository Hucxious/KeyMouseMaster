# TODO - 键鼠大师开发清单

## 第一版 (v1.0)

### 已完成
- [x] 项目工程文件 (.pro) 和构建系统
- [x] 核心类型定义 (AppTypes.h)
- [x] 脚本事件模型 (ScriptEvent, ScriptDocument)
- [x] 显示器信息模型 (MonitorInfo)
- [x] 脚本事件表格模型 (ScriptEventTableModel)
- [x] 日志系统 (Logger)
- [x] 按键映射工具 (KeyMapper)
- [x] 参数校验工具 (ValidationUtils)
- [x] 时间工具 (TimeUtils)
- [x] 配置管理器 (SettingsManager)
- [x] Windows 输入模拟器 (WindowsInputSimulator)
- [x] Windows 全局钩子管理器 (WindowsHookManager)
- [x] Windows 全局热键管理器 (WindowsHotkeyManager)
- [x] Windows 显示器后端 (WindowsMonitorBackend)
- [x] Windows 高DPI管理器 (WindowsDpiManager)
- [x] 坐标映射器 (CoordinateMapper)
- [x] 显示器管理器 (MonitorManager)
- [x] 输入状态管理器 (InputStateManager)
- [x] 鼠标连点引擎 (MouseClickEngine)
- [x] 键盘连点引擎 (KeyboardClickEngine)
- [x] 脚本录制器 (ScriptRecorder)
- [x] 脚本回放器 (ScriptPlayer)
- [x] 脚本序列化器 (ScriptSerializer)
- [x] 任务管理器 (TaskManager)
- [x] 应用控制器 (AppController)
- [x] 热键输入控件 (HotkeyEdit)
- [x] 按键捕获控件 (KeyCaptureEdit)
- [x] 显示器预览控件 (MonitorPreviewWidget)
- [x] 鼠标连点页面 (MouseClickPage)
- [x] 键盘连点页面 (KeyboardClickPage)
- [x] 脚本录制页面 (ScriptPage)
- [x] 主窗口 (MainWindow)
- [x] 系统托盘
- [x] 菜单栏 + 状态栏
- [x] 全局紧急停止
- [x] 输入状态复位
- [x] 资源文件 (.qrc, 样式表, manifest)
- [x] main.cpp 入口
- [x] 应用样式表
- [x] Linux平台编译通过 (Windows API 桩)

### 待人工测试 (需要 Windows 环境)
- [ ] 鼠标连点实际点击测试
- [ ] 键盘连点实际点击测试
- [ ] 脚本录制完整性测试
- [ ] 脚本回放准确性测试
- [ ] 多显示器（负坐标、不同DPI）测试
- [ ] 全局热键跨应用测试
- [ ] 系统托盘交互测试
- [ ] 紧急停止响应测试
- [ ] 高DPI渲染测试
- [ ] MinGW 编译
- [ ] MSVC 编译

### 已知待修复
- [ ] 脚本页面录制和回放逻辑需要完善（录制文档传递到页面）
- [ ] 主窗口中的录制/回放文档传递桥接
- [ ] Windows 下实际钩子测试和调试

## 第二版 (v2.0)

### 计划
- [ ] 图像识别点击
- [ ] 目标窗口绑定
- [ ] 窗口相对坐标模式
- [ ] 循环块编辑器 UI
- [ ] 脚本变量系统
- [ ] 自动更新机制
- [ ] 脚本市场/分享

## 第三版 (v3.0)

### 计划
- [ ] OCR 文字识别
- [ ] 插件系统
- [ ] 云端脚本同步
- [ ] EDID 显示器唯一识别
- [ ] 复杂主题切换
- [ ] 多语言支持
