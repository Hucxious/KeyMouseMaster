# v1.0 测试计划与记录

## 2026-09-23 用户补充验收

PASS：默认浏览器打开本地 HTML 后中文显示正常，由用户实测确认。MSVC / Qt 5.15 不纳入本轮验收要求。

## 验收环境

Windows 11 x64；Qt 6.9.0、MinGW 13.1 x64、C++17、qmake。使用[开发说明](development.md)所列的用户指定路径。Qt 5.15、Windows 10 未验证；MSVC 曾尝试检测，但按用户后续要求终止，不作为交付结果。

## 2026-09-22 两页调整回归

Qt 6.9.0 / MinGW 13.1：Debug、Release 编译 PASS，无 compiler warning/error。完整回归 44 PASS、0 FAIL、0 skipped；旧键盘模式四个数据行删除，新增三项页面/业务集成测试。结果位于 `build/layout-final-tests.txt` / `.xml`，构建日志为 `build/layout-debug.log`、`build/layout-release.log`。

- PASS：普通按键完整 down/up、长按实际保持时长、组合修饰键释放，两种模式的按钮启停与真实 RegisterHotKey 启停。
- PASS：旧设置值 1..4 / 非法值回退，旧长按值 5 保留，保存重载，运行状态下控件禁用及恢复。
- PASS：脚本页面按钮开始/停止录制、文档进入表格、经文件对话框保存/导入、正常回放与中途停止；测试采用临时 INI 与专用输入接收窗口。
- PASS：700×500、1000×800、1280×940 布局尺寸与截图检查；较大尺寸配置无滚动，700×500 仅配置局部滚动，运行按钮常驻；窗口增大时表格高度增长。
- Windows 桌面独立程序已检查两页布局、键盘下拉框及长按控件启用状态；未对所有 DPI/字体组合进行穷举。

## 2026-09-21 审计结果（历史快照）

2026-09-21，本机实际执行记录：

| 项目 | 结果 |
|---|---|
| Qt 6.9.0 / MinGW 13.1 qmake、Debug / Release 编译 | PASS；完整及最终增量构建无 compiler warning/error |
| Windows 原生 API + Qt 集成回归 | PASS；45 passed，0 failed，0 skipped |
| Debug / Release 独立 EXE | PASS；主窗口、三个页面、配置加载、正常退出 |
| 运行日志 | PASS；无 crash/assertion、Qt 生命周期警告或非预期 API 错误，退出有注销/卸载记录 |
| 托盘动作集成 | PASS；图标对象显示、恢复窗口、停止任务、输入复位；系统托盘实际鼠标菜单交互及托盘退出未验证 |
| 多屏 | PASS：本机枚举两屏、左屏负原点、不同分辨率与缩放的布局显示；坐标计算回归通过。跨屏实点/回放、热插拔及重排未验证 |
| Qt 5.15 / MSVC / Windows 10 | 未验证 |

执行 `qmake KeyMouseMaster.pro -o build/Makefile CONFIG+=debug_and_release`，随后分别 `mingw32-make -C build -f Makefile.Debug -j6` 与 `Makefile.Release`。测试使用 `qmake tests/tests.pro -o build/audit-tests/Makefile CONFIG+=debug CONFIG-=debug_and_release`、`mingw32-make -C build/audit-tests -j6` 和 `build/audit-tests/kmm_tests.exe`。工具 PATH 见开发说明。

本地证据：`build/audit-mingw/final-*.log`、`build/audit-tests/final-results.txt` / `.xml`、`build/audit-tests/ui-smoke.png`、两种 EXE 旁的 `log/`，不纳入版本库。中间复跑曾出现 Hook 水平滚轮漏收：测试在安装 Hook 的 GUI 线程连续注入，改为工作线程注入并保持 GUI 消息循环后通过，未放宽事件数量断言。托盘停止测试等待异步 finished 后断言 Idle。

## 自动回归范围

`tests/tst_kmm.cpp` 使用真实 Windows Hook / RegisterHotKey / SendInput 与 Qt 事件循环，不使用非 Windows 桩。测试隔离 QSettings，以专用空白窗口承接输入。

| 范围 | 检查 |
|---|---|
| 文件 | UTF-8 中文往返、64位时间戳、坐标模式、坏 JSON、缺字段/版本/事件、失败保留文档、写入及锁文件导致 commit 失败 |
| 坐标与模型 | 负原点绝对映射、显示器相对坐标、缺失设备、参数溢出/NaN、表格时间编辑和移动 |
| 热键 | 真实 WM_HOTKEY、冲突、注销、修改重注册、捕获结束先保存后注册 |
| Hook | 安装/卸载、鼠标/键盘/滚轮、KMM 注入过滤、未选类别过滤、完整文档传递 |
| 鼠标 | 5 按钮 × 5 模式，有限次数、任务状态及模拟器/Windows 按下状态清理 |
| 键盘 | 当前为普通按键/长按、组合修饰键、时长及完成/停止后状态清理；历史快照曾覆盖六种模式 |
| 回放 | 0.5/1/2倍速度、时间戳、两轮/间隔、禁用事件、暂停恢复 |
| 停止 | 无效参数回退、60000ms 启动/按压等待中紧急停止、回放中断、快速停止后重启 |
| UI 集成 | 主窗口、三个页面、组合键持久化、录制文档到表格、保存导入与回放 |

此表描述覆盖范围；以最终测试输出的 PASS/FAIL 为准。坐标计算测试不等于多屏硬件测试，XButton 注入不等于实体侧键录制实测。

## 人工/桌面验收清单

- Debug / Release 主程序启动、页面切换、配置加载、日志、正常退出。
- 无焦点及隐藏/最小化状态下热键触发；修改热键立即生效，冲突提示可见。
- 鼠标/键盘开始停止、录制结束表格有事件、回放中途停止。
- 托盘可见，恢复、菜单启动/停止、输入复位与退出可用。
- 检查 crash、assertion、Qt warning、QObject/QThread 生命周期警告和非预期 Windows API 失败。

## 未验证场景

物理单屏/上屏、双屏跨屏实际点击与录放、混合 DPI 下目标定位、切换主屏、断开/重排；管理员与普通权限目标、安全软件拦截；长时间压力、实体侧键、Windows 10、Qt 5.15、干净机器部署均需有对应硬件/环境再验证。

## 失败记录原则

测试中故意构造的无效脚本、冲突热键、锁文件等预期错误不算正常操作失败；应检查断言确实拒绝操作且状态不被破坏。任何未执行场景标为“未验证”，不能用静态审查代替 PASS。
