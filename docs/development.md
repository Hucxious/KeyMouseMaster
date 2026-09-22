# 开发、构建与部署

## 固定本机工具链

本轮仅使用用户指定工具链：

- qmake：`F:/Qt/QtProgram/6.9.0/mingw_64/bin/qmake.exe`
- 编译器：`F:/Qt/QtProgram/Tools/mingw1310_64/bin/g++.exe`
- make / gdb：同一 `mingw1310_64/bin` 目录
- Qt DLL / windeployqt：同一 `6.9.0/mingw_64/bin` 目录

`.vscode` 为本机忽略提交的 IDE 设置。qmake 与 make 的 PATH 都应先放上述目录，不能让系统的旧 MinGW 8.1 抢先。`F:/Temp/QtLearning/untitled/build/...` 是另一项目构建目录；本工程直接使用其对应的上述 Qt/MinGW 安装位置，不读写那个项目。

## 构建

完整命令见[入口](README.md#开发与编译)。推荐从根目录用 `-o build/Makefile` 构建，输出与现有 VS Code 启动配置一致。独立影子构建时输出在该构建目录的 debug/release 子目录，避免不同配置污染对象文件。

切换工具链时检查根目录和父构建目录的 `.qmake.stash`；旧编译器缓存可能导致检测失败。仅清理生成缓存，不删除源文件或用户配置。必要时运行：

```powershell
mingw32-make -C build -f Makefile.Debug clean
mingw32-make -C build -f Makefile.Release clean
```

工程保留 qmake、C++17、Qt Widgets。Qt 5.15 的 nativeEventFilter 使用 `long*`，Qt 6 使用 `qintptr*`；Qt 5 专属 API 有版本守卫。Windows 定义 NOMINMAX 防止 Windows 宏污染标准库。Qt 5.15 和 MSVC 的完整构建/运行本轮未验证。

## 自动回归

```powershell
New-Item -ItemType Directory -Force build/tests | Out-Null
qmake tests/tests.pro -o build/tests/Makefile 'CONFIG+=debug' 'CONFIG-=debug_and_release'
mingw32-make -C build/tests -j6
.\build\tests\kmm_tests.exe -o build/tests/results.txt,txt -o build/tests/results.xml,junitxml
```

测试使用 Qt Test 和真实 Win32 API，打开专用输入接收窗口；测试期间请避免手动输入，桌面必须解锁。拿不到接收窗口焦点应报告测试失败，不能向其他程序盲发输入。配置被重定向到临时 INI，不覆盖用户注册表设置；测试结束恢复光标并释放输入。

自动回归覆盖范围及限制见[测试记录](test-plan.md)。测试截图、日志、JUnit 和 EXE 都放 build 下，不提交编译产物。

## 部署

```powershell
windeployqt --release --compiler-runtime --no-translations build/release/KeyMouseMaster.exe
windeployqt --release --compiler-runtime --no-translations build/debug/KeyMouseMaster_d.exe
```

本机 MinGW Qt 安装仅包含 release Qt DLL/插件，Debug EXE 也链接这组库，因此两者均用 `--release` 部署；不能根据 EXE 的 `_d` 后缀选不存在的 debug 插件。

交付整个 release 目录，保留 `platforms`、图像插件和 Qt/MinGW 运行库；附带程序目录中的 docs/user-guide.html 与 LICENSE。运行库必须来自构建使用的同一套工具链。本轮生成的目录不是安装器；干净 Windows 机器上的部署仍需单独验收。

日志优先放程序旁边 log，写入失败回退到 QStandardPaths 的 AppLocalDataLocation。不得用库路径修改或替换来掩盖版本不匹配。依赖资源包括应用图标与 QRC；app.manifest 为项目携带的清单参考，本机 MinGW DPI 由启动前 API 实际设置。

## 维护边界

修改后重新编译并执行与故障相关的回归。Windows Hook 回调不能执行耗时槽；引擎不能访问正在变化的 GUI 配置。保持 .kms v1 字段和事件类型；新增识别、云服务、插件等不属于本轮。

帮助文件由 qmake 的依赖目标自动复制到 Debug / Release 的 `docs/user-guide.html`。HTML 修改后再次构建也会更新，无需重新链接源码；只部署这一份用户文档，不复制全部开发文档。
