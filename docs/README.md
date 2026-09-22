# 键鼠大师 KeyMouseMaster

KeyMouseMaster（KMM）是 Windows 桌面键鼠自动化工具。当前版本 **1.0.0**，本轮工作为 v1.0 稳定性修复，不包含 OCR、图像识别、插件或云服务。

提供鼠标/键盘连点、全局快捷键、键鼠脚本录制与回放、`.kms` 保存导入、基本事件编辑、系统托盘与紧急停止。支持虚拟桌面负坐标、显示器相对及比例坐标。实现状态和实测范围请看[测试记录](test-plan.md)，不要将代码实现等同于所有环境均已验证。

## 工程结构

- `src/ui`：三个页面、主窗口、按键捕获及显示器预览。
- `src/core`：控制器、任务管理、连点、录制、回放及坐标转换。
- `src/platform/windows`：Hook、热键、SendInput、显示器及 DPI。
- `src/models`、`src/settings`、`src/utils`：数据、配置、日志与工具。
- `resources`：图标、资源清单及样式；`tests`：Qt Test 回归；`docs`：项目文档与离线 HTML 帮助。
- `build`：忽略提交的构建、部署与测试输出。项目原先没有 samples 目录；本次未虚构样例或空目录。

## 开发与编译

Windows 10/11 x64、C++17、Qt Widgets、qmake。兼容目标为 Qt 5.15 / Qt 6，本机验收仅使用 **Qt 6.9.0 + MinGW 13.1 x64**。Qt 5.15 未验证；MSVC 按用户要求不作为本轮交付验证工具链。

在项目根目录的 PowerShell 中：

```powershell
$env:PATH = 'F:\Qt\QtProgram\Tools\mingw1310_64\bin;F:\Qt\QtProgram\6.9.0\mingw_64\bin;' + $env:PATH
New-Item -ItemType Directory -Force build | Out-Null
qmake KeyMouseMaster.pro -o build/Makefile 'CONFIG+=debug_and_release'
mingw32-make -C build -f Makefile.Debug -j6
mingw32-make -C build -f Makefile.Release -j6
```

输出为 `build/debug/KeyMouseMaster_d.exe` 和 `build/release/KeyMouseMaster.exe`。运行库部署、测试和清理步骤见[开发说明](development.md)。

## 文档导航

- [运行与使用说明](user-guide.html)
- [架构与线程/坐标约定](architecture.md)
- [开发、构建与部署](development.md)
- [测试计划及实际结果](test-plan.md)
- [版本变化](CHANGELOG.md)
- [完成、验证与待办](TODO.md)

许可：[MIT](../LICENSE)。
