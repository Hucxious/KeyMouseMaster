# CHANGELOG

## [1.0.0] - 2026-07-20

### 新增
- 完整项目架构：分层设计（UI/Core/Models/Platform/Settings/Utils）
- 鼠标连点功能：多种按键、点击模式、坐标模式、定时参数
- 键盘连点功能：按键捕获控件、组合键、多种输入模式
- 脚本录制：WH_MOUSE_LL/WH_KEYBOARD_LL 全局钩子、鼠标移动压缩、事件过滤
- 脚本回放：多速度回放、循环执行、事件跳过、显示器坐标解析
- 脚本保存/导入：.kms JSON 格式、QSaveFile 安全写入、结构验证
- 多显示器支持：负坐标、多DPI、虚拟桌面坐标映射、显示器变化监听
- 全局快捷键：RegisterHotKey、热键冲突检测、动态注册/注销
- 紧急停止：全局停止 + 修饰键释放 + 鼠标按钮释放 + 输入状态复位
- 系统托盘：状态指示、右键菜单、最小化到托盘
- 配置持久化：QSettings 管理所有用户参数
- 基础脚本编辑：删除/启用禁用/上下移动/插入等待事件/清空
- 日志系统：文件日志、大小轮转、多级别
- 高DPI支持：Per-Monitor DPI Aware V2、坐标系统分离
- Windows兼容类型桩：非Windows平台编译支持
- 默认浅色主题样式表
- 显示器布局预览控件
- 完整 README、TODO、CHANGELOG 文档

### 编译状态
- Linux (Qt 5.15.3, gcc): 编译通过，0 错误 0 警告
- Windows (MinGW): 待验证
- Windows (MSVC): 待验证
