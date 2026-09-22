# v1.0 架构与实现约定

## 调用链

```text
MouseClickPage / KeyboardClickPage / ScriptPage
  → MainWindow：保存当前界面参数，发起操作
  → AppController：应用设置、全局热键、错误与文档桥接
  → TaskManager：同一时刻一个任务，启动失败与停止状态同步
  → MouseClickEngine / KeyboardClickEngine / ScriptPlayer
  → WindowsInputSimulator → SendInput

WH_MOUSE_LL / WH_KEYBOARD_LL（GUI 消息线程）
  → RawHookEvent 有界队列 + 条件变量
  → Hook 转换工作线程
  → ScriptRecorder：队列排空后补显示器元数据
  → ScriptDocument
  → AppController::recordedDocumentReady
  → ScriptPage / ScriptEventTableModel
  → ScriptSerializer / ScriptPlayer
```

全局快捷键由 `RegisterHotKey(nullptr, id, ...)` 注册到 GUI 线程，通过 `WindowsHotkeyManager` 的 native event filter 接收 `WM_HOTKEY`。按钮与功能热键汇入同一 MainWindow 启动路径，因此读取的是当前页面参数。热键捕获先保存新值，再恢复注册，避免恢复旧热键。

## 线程与生命周期

GUI 线程管理窗口、任务状态、Hook 安装/回调和热键注册；回调仅过滤、提取原始事件并入队，唤醒转换线程。`QThread::create` 运行连点、回放和录制转换循环。不能对 GUI 线程所属 QObject 调用 queued invokeMethod 后就假定其函数运行在工作线程。

引擎配置与脚本文档在运行中保持不变；显示器列表在启动时快照，工作线程不读取可由 GUI 热插拔刷新修改的容器。停止采用原子标记，等待分段检查标记。同步停止先等待线程完成，再释放输入。线程完成通知带代次校验，防止已排队旧通知影响新任务。

录制停止先禁止入队并卸载 Hook，再等待队列排空，最后发出完整文档。TaskManager 在调用同步停止前先置 Stopping，避免随后覆盖由 recordingStopped 设置的 Idle。信号发出前释放状态锁，允许槽同步回调。

AppController 用 Qt parent 管理常规对象，但显式先销毁依赖平台对象的引擎/录制器；不能让 InputSimulator 先销毁再被引擎析构访问。主程序在资源析构之后关闭日志。

## 输入与坐标

SendInput 带固定 `APP_EXTRA_INFO`，录制默认过滤 KMM 自身输入。模拟器跟踪实际按下的键、扩展键标志和鼠标按钮；任务结束、停止与复位通过该跟踪清理。`InputStateManager` 保留为既有辅助接口，目前输入释放的真实数据源是 WindowsInputSimulator，不是第二套独立追踪。

录制和回放使用物理像素：虚拟桌面绝对位置、显示器内部位置、比例位置。`CoordinateMapper` 统一负责转换和 SendInput 的 0～65535 归一化，包含负原点。鼠标回放的释放、滚轮也定位到事件坐标。设备按名称匹配；本版没有 EDID 和替代设备交互。布局变化停止坐标任务。

DPI 感知在 QApplication 之前设置；Qt 5 高 DPI 属性有版本条件。平台枚举提供桌面物理区域，QScreen 提供 UI 缩放信息；物理坐标不再按 Qt devicePixelRatio 二次缩放。

## 文件、计时与配置

`.kms` 保持 `KeyMouseScript`、版本 1 和既有字段不变。保存保留原有中文坐标模式值；读取兼容中文和早期文档的英文值。导入在临时文档内验证，成功后整体替换；QSaveFile 检查写入长度和 commit，失败保留原文件。事件时间戳为相对毫秒，要求行顺序非递减。

回放使用单调时钟与绝对目标时间，按速度缩放；暂停补偿暂停时间。禁用事件可跳过，每轮释放输入。等待编辑通过顺延时间戳实现，不添加新事件类型。

QSettings 保存用户参数和组合键完整信息。日志为互斥保护的同步文件写入，无独立日志线程；超限轮转持同一把锁。高频连点日志限频，避免每毫秒写盘。
