#include <QtTest>
#include <QDesktopServices>
#include <QClipboard>
#include <QTextBrowser>
#include <QStatusBar>
#include <QScreen>
#include <QMessageBox>
#include "ui/AboutDialog.h"
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QTabWidget>
#include <QSettings>
#include <QSignalSpy>
#include <QSaveFile>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QFileDialog>
#include <QScrollArea>
#include <QScrollBar>
#include <QGroupBox>
#include "ui/KeyboardClickPage.h"
#include <limits>
#include "core/AppController.h"
#include "core/TaskManager.h"
#include "core/MouseClickEngine.h"
#include "core/KeyboardClickEngine.h"
#include "core/ScriptPlayer.h"
#include "core/ScriptRecorder.h"
#include "core/CoordinateMapper.h"
#include "core/MonitorManager.h"
#include "core/ScriptSerializer.h"
#include "platform/windows/WindowsHookManager.h"
#include "platform/windows/WindowsHotkeyManager.h"
#include "platform/windows/WindowsInputSimulator.h"
#include "platform/windows/WindowsDpiManager.h"
#include "settings/SettingsManager.h"
#include "ui/MainWindow.h"
#include "ui/ScriptPage.h"
#include "ui/HotkeyEdit.h"
#include "ui/KeyCaptureEdit.h"
#include "models/ScriptEventTableModel.h"
#include "utils/ValidationUtils.h"
#include "utils/Logger.h"

// Integration tests send real Windows input only to this inert receiver.
class InputReceiver : public QWidget {
public:
    int keys = 0;
    QElapsedTimer inputClock;
    QVector<qint64> downTimes, upTimes;
    InputReceiver() { setWindowTitle("KMM integration input receiver"); resize(400, 220); inputClock.start(); }
    void keyPressEvent(QKeyEvent* ev) override {
        ++keys;
        if (ev->key() == Qt::Key_F24 && !ev->isAutoRepeat()) downTimes.append(inputClock.elapsed());
        ev->accept();
    }
    void keyReleaseEvent(QKeyEvent* ev) override {
        if (ev->key() == Qt::Key_F24 && !ev->isAutoRepeat()) upTimes.append(inputClock.elapsed());
        ev->accept();
    }
    void mousePressEvent(QMouseEvent* ev) override { ev->accept(); }
    void mouseReleaseEvent(QMouseEvent* ev) override { ev->accept(); }
    void contextMenuEvent(QContextMenuEvent* ev) override { ev->accept(); }
};

class UrlReceiver : public QObject {
    Q_OBJECT
public:
    QUrl last;
public slots:
    void receive(const QUrl& url) { last = url; }
};

class KmmTests : public QObject {
    Q_OBJECT
    InputReceiver receiver;
    QPoint originalCursor;
    static ScriptEvent key(ScriptEventType type, qint64 time, uint32_t vk = VK_F24) {
        ScriptEvent ev; ev.type = type; ev.timestampMs = time; ev.winVk = vk; return ev;
    }
    bool focusReceiver() {
        receiver.showNormal(); receiver.raise(); receiver.activateWindow();
        const DWORD foregroundThread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
        const DWORD ownThread = GetCurrentThreadId();
        const bool attached = foregroundThread && foregroundThread != ownThread
            && AttachThreadInput(ownThread, foregroundThread, TRUE);
        SetForegroundWindow(reinterpret_cast<HWND>(receiver.winId()));
        if (attached) AttachThreadInput(ownThread, foregroundThread, FALSE);
        QTest::qWait(100);
        return GetForegroundWindow() == reinterpret_cast<HWND>(receiver.winId());
    }
private slots:
    void initTestCase() {
        QSettings isolated(QSettings::defaultFormat(), QSettings::UserScope, "KeyMouseMaster", "KeyMouseMaster");
        QCOMPARE(isolated.format(), QSettings::IniFormat);
        QVERIFY(isolated.fileName().startsWith(QDir::tempPath()));
        originalCursor = WindowsMonitorBackend().getCurrentCursorPos();
        if (focusReceiver()) {
            POINT pt{100, 100}; ClientToScreen(reinterpret_cast<HWND>(receiver.winId()), &pt);
            SetCursorPos(pt.x, pt.y);
        }
    }
    void cleanupTestCase() {
        WindowsInputSimulator simulator; simulator.releaseAllInputs();
        SetCursorPos(originalCursor.x(), originalCursor.y()); receiver.hide();
    }
    void fileRoundTripAndFailures() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        ScriptDocument doc; doc.name = QStringLiteral("中文脚本");
        doc.events = {key(ScriptEventType::KeyDown, 3000000000LL), key(ScriptEventType::KeyUp, 3000000040LL)};
        doc.coordinateMode = CoordinateMode::VirtualDesktopAbsolute;
        doc.playbackSettings.coordinateMode = CoordinateMode::MonitorRatio;
        doc.playbackSettings.speedFactor = 2;
        QString error; QVERIFY2(doc.saveToFile(dir.filePath("roundtrip.kms"), &error), qPrintable(error));
        ScriptDocument loaded; QVERIFY(loaded.loadFromFile(doc.filePath, &error));
        QCOMPARE(loaded.name, doc.name); QCOMPARE(loaded.events[0].timestampMs, 3000000000LL);
        QCOMPARE(loaded.coordinateMode, doc.coordinateMode);
        QCOMPARE(loaded.playbackSettings.coordinateMode, CoordinateMode::MonitorRatio);
        QJsonObject bad = doc.toJson(); bad["events"] = QJsonArray{QJsonObject{{"type", 99}}};
        QVERIFY(!loaded.fromJson(bad)); QCOMPARE(loaded.name, doc.name); QCOMPARE(loaded.eventCount(), 2);
        bad = doc.toJson(); bad.remove("events"); QVERIFY(!loaded.fromJson(bad, &error));
        bad = doc.toJson(); bad["version"] = 9; QVERIFY(!loaded.fromJson(bad, &error));
        bad = doc.toJson(); auto pb = bad["playback"].toObject(); pb["speed_factor"] = 0;
        bad["playback"] = pb; QVERIFY(!loaded.fromJson(bad, &error));
        QFile malformed(dir.filePath("bad.kms")); QVERIFY(malformed.open(QIODevice::WriteOnly));
        malformed.write("{broken"); malformed.close(); QVERIFY(!loaded.loadFromFile(malformed.fileName(), &error));
        QVERIFY(!doc.saveToFile(dir.filePath("missing/sub/file.kms"), &error));
        // QSaveFile opens a temporary file, then cannot replace a directory on commit.
        QDir().mkdir(dir.filePath("directory.kms"));
        QVERIFY(!doc.saveToFile(dir.filePath("directory.kms"), &error));
        const QString lockedPath = dir.filePath("locked.kms");
        QVERIFY(doc.saveToFile(lockedPath, &error));
        HANDLE locked = CreateFileW(reinterpret_cast<LPCWSTR>(lockedPath.utf16()), GENERIC_READ,
                                    FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        QVERIFY(locked != INVALID_HANDLE_VALUE);
        doc.name = "must not replace locked file";
        const bool failedCommit = !doc.saveToFile(lockedPath, &error);
        CloseHandle(locked);
        QVERIFY(failedCommit);
        QVERIFY(loaded.loadFromFile(lockedPath));
        QCOMPARE(loaded.name, QStringLiteral("中文脚本"));
        bad = doc.toJson();
        bad["events"] = QJsonArray{key(ScriptEventType::KeyDown, 200).toJson(), key(ScriptEventType::KeyUp, 100).toJson()};
        QVERIFY(!loaded.fromJson(bad));
    }
    void coordinatesAndValidation() {
        int x, y; QRect bounds(-1920, -1080, 3840, 2160);
        CoordinateMapper::virtualToWindowsAbsolute(bounds.topLeft(), bounds, x, y);
        QCOMPARE(x, 0); QCOMPARE(y, 0);
        CoordinateMapper::virtualToWindowsAbsolute(bounds.bottomRight(), bounds, x, y);
        QCOMPARE(x, 65535); QCOMPARE(y, 65535);
        MonitorInfo m; m.deviceName = "left"; m.desktopRect = QRect(-1920, 0, 1920, 1080);
        QPoint result; QVERIFY(CoordinateMapper::resolveTargetCoordinate(CoordinateMode::MonitorRelative,
            {}, "left", QPoint(50, 20), {}, {m}, result)); QCOMPARE(result, QPoint(-1870, 20));
        QVERIFY(!CoordinateMapper::resolveTargetCoordinate(CoordinateMode::MonitorRelative,
            {}, "missing", {}, {}, {m}, result));
        QVERIFY(!ValidationUtils::validateClickInterval(3600000, 1000));
        QVERIFY(!ValidationUtils::validatePlaybackSpeed(std::numeric_limits<double>::quiet_NaN()));
    }
    void modelEdits() {
        ScriptDocument doc; doc.events = {key(ScriptEventType::KeyDown, 50), key(ScriptEventType::KeyUp, 100)};
        ScriptEventTableModel model; model.setDocument(&doc);
        QVERIFY(!model.setData(model.index(0, ScriptEventTableModel::ColTime), "not a number"));
        QVERIFY(!model.setData(model.index(0, ScriptEventTableModel::ColTime), 200));
        QVERIFY(model.setData(model.index(0, ScriptEventTableModel::ColTime), 60));
        model.moveRowDown(0); QCOMPARE(doc.events[0].timestampMs, 60LL); QCOMPARE(doc.events[1].timestampMs, 100LL);
        model.toggleEnabled(0); QVERIFY(!doc.events[0].enabled);
    }
    void hotkeyNativeDelivery() {
        WindowsHotkeyManager manager; qApp->installNativeEventFilter(&manager);
        HotkeyInfo hk; hk.key = Qt::Key_F11; hk.ctrl = true; hk.shift = true;
        QSignalSpy received(&manager, &WindowsHotkeyManager::hotkeyPressed);
        const int id = manager.registerHotkey(hk); QVERIFY(id > 0);
        QCOMPARE(manager.registerHotkey(hk), -1);
        WindowsInputSimulator simulator;
        QVERIFY(focusReceiver());
        QVERIFY(simulator.sendCombo(VK_F11, true, true, false, false, 10));
        QTRY_COMPARE_WITH_TIMEOUT(received.count(), 1, 1000);
        QVERIFY(manager.unregisterHotkey(id));
        QVERIFY(simulator.sendCombo(VK_F11, true, true, false, false, 10));
        QTest::qWait(100); QCOMPARE(received.count(), 1);
        hk.key = Qt::Key_F10; QVERIFY(manager.registerHotkey(hk) > 0);
        QVERIFY(simulator.sendCombo(VK_F10, true, true, false, false, 10));
        QTRY_COMPARE_WITH_TIMEOUT(received.count(), 2, 1000);
        manager.unregisterAll(); qApp->removeNativeEventFilter(&manager);
    }
    void hookRecordingAndFiltering() {
        QVERIFY(focusReceiver());
        WindowsHookManager hooks; MonitorManager monitors; monitors.initialize();
        ScriptRecorder recorder(&hooks, &monitors); WindowsInputSimulator simulator;
        RecordingSettings settings; settings.ignoreOwnWindow = false;
        QVERIFY(recorder.startRecording(settings));
        QVERIFY(simulator.keyPress(VK_F24, 1)); QTest::qWait(60);
        recorder.stopRecording();
        auto filtered = recorder.takeDocument();
        for (const auto& ev : filtered.events) QVERIFY(ev.winVk != VK_F24);
        settings.ignoreSimulated = false;
        QVERIFY(recorder.startRecording(settings));
        // 与生产 Player 一样在工作线程注入，让安装 Hook 的 GUI 线程持续泵消息。
        std::atomic_bool sent{false};
        bool inputOk = false;
        auto* injector = QThread::create([&]() {
            inputOk = simulator.keyPress(VK_F24, 1)
                && simulator.mouseClick(MouseButton::Left, 1)
                && simulator.mouseWheel(120) && simulator.mouseHorizontalWheel(-120);
            sent = true;
        });
        injector->start();
        QElapsedTimer wait; wait.start();
        while (!sent.load() && wait.elapsed() < 5000) QTest::qWait(10);
        injector->wait(); delete injector;
        QVERIFY(inputOk);
        QTest::qWait(100); recorder.stopRecording();
        const auto doc = recorder.takeDocument();
        int downs = 0, ups = 0, wheels = 0;
        for (const auto& ev : doc.events) {
            if (ev.type == ScriptEventType::KeyDown && ev.winVk == VK_F24) ++downs;
            if (ev.type == ScriptEventType::KeyUp && ev.winVk == VK_F24) ++ups;
            if (ev.type == ScriptEventType::MouseWheel || ev.type == ScriptEventType::MouseHWheel) ++wheels;
            if (ev.isMouseEvent()) QVERIFY(!ev.monitorDeviceName.isEmpty());
        }
        QCOMPARE(downs, 1); QCOMPARE(ups, 1); QCOMPARE(wheels, 2);
        QVERIFY(!hooks.isMouseHookInstalled()); QVERIFY(!hooks.isKeyboardHookInstalled());
        QVERIFY(!recorder.isRecording());
        // Previously these disabled event categories produced bogus MouseMove(0,0).
        settings.recordMouseMove = false; settings.recordMouseClick = false; settings.recordWheel = true;
        QVERIFY(recorder.startRecording(settings));
        QVERIFY(simulator.mouseMoveRelative(1, 0)); QVERIFY(simulator.mouseClick(MouseButton::Left, 1));
        QTest::qWait(50); recorder.stopRecording();
        for (const auto& ev : recorder.takeDocument().events) QVERIFY(ev.type != ScriptEventType::MouseMove);
    }
    void mouseModes_data() {
        QTest::addColumn<int>("button"); QTest::addColumn<int>("mode");
        for (int button = 0; button < 5; ++button)
            for (int mode = 0; mode < 5; ++mode)
                QTest::newRow(qPrintable(QString("button%1-mode%2").arg(button).arg(mode))) << button << mode;
    }
    void mouseModes() {
        QFETCH(int, button); QFETCH(int, mode); QVERIFY(focusReceiver());
        WindowsInputSimulator simulator; MonitorManager monitors; monitors.initialize();
        MouseClickEngine engine(&simulator, &monitors); TaskManager tasks; tasks.setMouseClickEngine(&engine);
        MouseClickEngine::Config cfg; cfg.button = static_cast<MouseButton>(button);
        cfg.clickMode = static_cast<ClickMode>(mode); cfg.pressDurationMs = 10; cfg.doubleClickIntervalMs = 10;
        cfg.repeatCount = 2; cfg.intervalMs = 30; engine.setConfig(cfg);
        QSignalSpy done(&engine, &MouseClickEngine::finished);
        QVERIFY(tasks.requestStartMouseClick()); QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 2000);
        QCOMPARE(engine.currentCount(), 2); QCOMPARE(tasks.currentState(), TaskState::Idle);
        QVERIFY(!simulator.hasPressedKeys());
        const int vks[] = {VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2};
        QTest::qWait(30); QVERIFY(!(GetAsyncKeyState(vks[button]) & 0x8000));
    }
    void keyboardModes_data() {
        QTest::addColumn<int>("mode");
        for (int mode : {int(KeyInputMode::Normal), int(KeyInputMode::Hold)})
            QTest::newRow(qPrintable(QString::number(mode))) << mode;
    }
    void keyboardModes() {
        QFETCH(int, mode); QVERIFY(focusReceiver()); WindowsInputSimulator simulator;
        KeyboardClickEngine engine(&simulator); KeyboardClickEngine::Config cfg;
        cfg.winVk = VK_F24; cfg.inputMode = static_cast<KeyInputMode>(mode);
        cfg.pressDurationMs = 180; cfg.intervalMs = 250; cfg.repeatCount = 2;
        receiver.downTimes.clear(); receiver.upTimes.clear();
        cfg.hasCtrl = true; cfg.hasShift = true; engine.setConfig(cfg);
        QSignalSpy done(&engine, &KeyboardClickEngine::finished); engine.start();
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 2000); QVERIFY(!simulator.hasPressedKeys());
        QTest::qWait(30);
        QCOMPARE(receiver.downTimes.size(), 2); QCOMPARE(receiver.upTimes.size(), 2);
        for (int i = 0; i < 2; ++i) {
            const qint64 held = receiver.upTimes[i] - receiver.downTimes[i];
            if (mode == int(KeyInputMode::Hold)) QVERIFY2(held >= 160 && held < 500, "Hold duration mismatch");
            else QVERIFY2(held < 150, "Normal mode must ignore the saved hold duration");
        }
        for (int vk : {VK_F24, VK_CONTROL, VK_SHIFT, VK_MENU, VK_LWIN}) QVERIFY(!(GetAsyncKeyState(vk) & 0x8000));
    }
    void playbackTiming_data() {
        QTest::addColumn<double>("speed");
        QTest::newRow("half") << 0.5; QTest::newRow("normal") << 1.0; QTest::newRow("double") << 2.0;
    }
    void playbackTiming() {
        QFETCH(double, speed); QVERIFY(focusReceiver());
        WindowsInputSimulator simulator; MonitorManager monitors; monitors.initialize();
        ScriptPlayer player(&simulator, &monitors); ScriptDocument doc;
        doc.events = {key(ScriptEventType::KeyDown, 100), key(ScriptEventType::KeyUp, 200)};
        ScriptEvent disabled = key(ScriptEventType::KeyDown, 210, 'Z'); disabled.enabled = false; doc.events.append(disabled);
        PlaybackSettings settings; settings.speedFactor = speed; settings.repeatCount = 2;
        settings.roundIntervalMs = 40; settings.restoreCursor = false;
        player.setDocument(doc); player.setPlaybackSettings(settings);
        QSignalSpy done(&player, &ScriptPlayer::finished), progress(&player, &ScriptPlayer::progressChanged);
        QElapsedTimer timer; timer.start(); player.start();
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 2500);
        QVERIFY2(timer.elapsed() >= 400 / speed + 35, "Playback ignored timestamps/speed/round interval");
        QVERIFY(timer.elapsed() < 400 / speed + 600); QCOMPARE(progress.count(), 4);
        QVERIFY(!simulator.hasPressedKeys());
    }
    void pauseAndRepeatedStop() {
        QVERIFY(focusReceiver());
        WindowsInputSimulator simulator; MonitorManager monitors; monitors.initialize();
        ScriptPlayer player(&simulator, &monitors); TaskManager tasks; tasks.setScriptPlayer(&player);
        ScriptDocument doc; doc.events = {key(ScriptEventType::KeyDown, 250), key(ScriptEventType::KeyUp, 350)};
        PlaybackSettings settings; settings.restoreCursor = false;
        QSignalSpy progress(&player, &ScriptPlayer::progressChanged);
        QVERIFY(tasks.requestStartPlayback(doc, settings));
        tasks.requestPause(); QCOMPARE(tasks.currentState(), TaskState::Paused);
        QTest::qWait(400); QCOMPARE(progress.count(), 0);
        tasks.requestResume(); QCOMPARE(tasks.currentState(), TaskState::Playing);
        QTRY_VERIFY_WITH_TIMEOUT(!tasks.isAnyTaskRunning(), 1500);
        QCOMPARE(progress.count(), 2);
        for (int round = 0; round < 5; ++round) {
            settings.startDelayMs = 60000;
            QVERIFY(tasks.requestStartPlayback(doc, settings));
            tasks.emergencyStop();
            QVERIFY(tasks.requestStartPlayback(doc, settings));
            QTest::qWait(20); // 已排队的旧 finished 不得把新任务设为 Idle。
            QCOMPARE(tasks.currentState(), TaskState::Playing);
            tasks.emergencyStop();
            QVERIFY(!player.isRunning()); QVERIFY(!simulator.hasPressedKeys());
        }
    }
    void capturePersistsBeforeRegistration() {
        HotkeyEdit edit;
        HotkeyInfo initial; initial.key = Qt::Key_F6; edit.setHotkey(initial);
        int stored = initial.key, registered = initial.key;
        connect(&edit, &HotkeyEdit::hotkeyChanged, &edit, [&](const HotkeyInfo& hk) { stored = hk.key; });
        HotkeyEdit::setCaptureStateCallback([&](bool active) { if (!active) registered = stored; });
        QTest::mouseClick(&edit, Qt::LeftButton);
        QVERIFY(HotkeyEdit::isAnyCapturing());
        QTest::keyClick(&edit, Qt::Key_F5);
        QCOMPARE(stored, int(Qt::Key_F5)); QCOMPARE(registered, stored);
        QVERIFY(!HotkeyEdit::isAnyCapturing());
        HotkeyEdit::setCaptureStateCallback({});
        KeyCaptureEdit target;
        QTest::mouseClick(&target, Qt::LeftButton);
        QTest::keyClick(&target, Qt::Key_Control);
        QCOMPARE(target.keyInfo().qtKey, int(Qt::Key_Control));
        QVERIFY(!HotkeyEdit::isAnyCapturing());
    }
    void emergencyAndInvalidStarts() {
        QVERIFY(focusReceiver()); WindowsInputSimulator simulator; MonitorManager monitors; monitors.initialize();
        MouseClickEngine mouse(&simulator, &monitors); KeyboardClickEngine keyboard(&simulator);
        ScriptPlayer player(&simulator, &monitors); TaskManager tasks;
        tasks.setMouseClickEngine(&mouse); tasks.setKeyboardClickEngine(&keyboard); tasks.setScriptPlayer(&player);
        QVERIFY(!tasks.requestStartKeyboardClick()); QCOMPARE(tasks.currentState(), TaskState::Idle);
        ScriptDocument empty; QVERIFY(!tasks.requestStartPlayback(empty, {})); QCOMPARE(tasks.currentState(), TaskState::Idle);
        MouseClickEngine::Config mc; mc.startDelayMs = 60000; mouse.setConfig(mc);
        QVERIFY(tasks.requestStartMouseClick()); QElapsedTimer t; t.start(); tasks.emergencyStop();
        QVERIFY(t.elapsed() < 500); QVERIFY(!mouse.isRunning()); QCOMPARE(tasks.currentState(), TaskState::Idle);
        mc.startDelayMs = 0; mc.clickMode = ClickMode::Hold; mc.pressDurationMs = 60000; mouse.setConfig(mc);
        QVERIFY(tasks.requestStartMouseClick()); QTest::qWait(30); t.restart(); tasks.emergencyStop();
        QVERIFY(t.elapsed() < 500); QVERIFY(!simulator.hasPressedKeys());
        KeyboardClickEngine::Config kc; kc.winVk = VK_F24; kc.inputMode = KeyInputMode::Hold; kc.pressDurationMs = 60000; keyboard.setConfig(kc);
        QVERIFY(tasks.requestStartKeyboardClick()); QTest::qWait(30); t.restart(); tasks.emergencyStop();
        QVERIFY(t.elapsed() < 500); QVERIFY(!simulator.hasPressedKeys());
        ScriptDocument doc; doc.events = {key(ScriptEventType::KeyDown, 0), key(ScriptEventType::KeyUp, 60000)};
        PlaybackSettings settings; settings.restoreCursor = false;
        QVERIFY(tasks.requestStartPlayback(doc, settings)); QTest::qWait(30); t.restart(); tasks.emergencyStop();
        QVERIFY(t.elapsed() < 500); QVERIFY(!player.isRunning()); QVERIFY(!simulator.hasPressedKeys());
        QTest::qWait(50); QCOMPARE(tasks.currentState(), TaskState::Idle);
    }
    void keyboardPageModesAndHotkeys() {
        AppController controller; QVERIFY(controller.initialize());
        MainWindow window(&controller); window.resize(1000, 760); window.show();
        auto* page = window.findChild<KeyboardClickPage*>(); QVERIFY(page);
        window.findChild<QTabWidget*>()->setCurrentWidget(page);
        auto* modes = page->findChild<QComboBox*>("keyboardMode");
        auto* hold = page->findChild<QSpinBox*>("keyboardHoldDuration");
        QVERIFY(modes); QVERIFY(hold); QCOMPARE(modes->count(), 2);
        QCOMPARE(modes->itemText(0), QStringLiteral("普通按键"));
        QCOMPARE(modes->itemText(1), QStringLiteral("长按"));
        QSettings raw(QSettings::defaultFormat(), QSettings::UserScope, "KeyMouseMaster", "KeyMouseMaster");
        for (int legacy : {-1, 1, 2, 3, 4, 99}) {
            raw.setValue("keyboard/inputMode", legacy); raw.sync();
            page->loadSettings(); QCOMPARE(modes->currentData().toInt(), 0); QVERIFY(!hold->isEnabled());
        }
        controller.settingsManager()->setKeyboardInputMode(5); page->loadSettings();
        QCOMPARE(modes->currentIndex(), 1); QVERIFY(hold->isEnabled());
        page->saveSettings(); SettingsManager restored; QCOMPARE(restored.keyboardInputMode(), 5);
        KeyInfo keyInfo; keyInfo.qtKey = Qt::Key_F24; keyInfo.winVk = VK_F24; keyInfo.displayName = "F24";
        controller.settingsManager()->setKeyboardKeyInfo(keyInfo);
        controller.settingsManager()->setKeyboardInfinite(true);
        controller.settingsManager()->setKeyboardPressDuration(2000);
        controller.settingsManager()->setKeyboardInterval(100);
        HotkeyInfo toggle; toggle.key = Qt::Key_F11; toggle.ctrl = true; toggle.shift = true;
        controller.settingsManager()->setKeyboardStartHotkey(toggle); page->loadSettings();
        controller.registerAllHotkeys(); QVERIFY(controller.hotkeyErrors().isEmpty());
        qApp->installNativeEventFilter(controller.hotkeyManager());
        QPushButton *startButton = nullptr, *stopButton = nullptr;
        for (auto* b : page->findChildren<QPushButton*>()) {
            if (b->text().contains(QStringLiteral("启动"))) startButton = b;
            if (b->text().contains(QStringLiteral("停止"))) stopButton = b;
        }
        QVERIFY(startButton); QVERIFY(stopButton);
        for (int mode : {0, 5}) {
            modes->setCurrentIndex(modes->findData(mode));
            QVERIFY(focusReceiver()); startButton->click();
            QTRY_COMPARE_WITH_TIMEOUT(controller.taskManager()->currentState(), TaskState::KeyboardClicking, 1000);
            QTest::qWait(100); stopButton->click();
            QTRY_COMPARE_WITH_TIMEOUT(controller.taskManager()->currentState(), TaskState::Idle, 1000);
            QVERIFY(!controller.inputSimulator()->hasPressedKeys());
            QVERIFY(focusReceiver());
            QVERIFY(controller.inputSimulator()->sendCombo(VK_F11, true, true, false, false, 10));
            QTRY_COMPARE_WITH_TIMEOUT(controller.taskManager()->currentState(), TaskState::KeyboardClicking, 1500);
            QCOMPARE(int(controller.keyboardClickEngine()->config().inputMode), mode);
            QVERIFY(!hold->isEnabled());
            QTest::qWait(100);
            QVERIFY(controller.inputSimulator()->sendCombo(VK_F11, true, true, false, false, 10));
            QTRY_COMPARE_WITH_TIMEOUT(controller.taskManager()->currentState(), TaskState::Idle, 1500);
            QVERIFY(!controller.inputSimulator()->hasPressedKeys());
            QVERIFY(!(GetAsyncKeyState(VK_F24) & 0x8000));
            QCOMPARE(hold->isEnabled(), mode == 5);
        }
        qApp->removeNativeEventFilter(controller.hotkeyManager());
        window.hide();
    }
    void pageLayoutSizes() {
        AppController controller; QVERIFY(controller.initialize()); controller.unregisterAllHotkeys();
        MainWindow window(&controller); window.show();
        auto* tabs = window.findChild<QTabWidget*>();
        auto* page = window.findChild<ScriptPage*>();
        auto* configuration = page->findChild<QScrollArea*>("scriptConfiguration");
        auto* table = page->findChild<QTableView*>("scriptEvents");
        auto* controls = page->findChild<QGroupBox*>("scriptRunControls");
        QVERIFY(configuration); QVERIFY(table); QVERIFY(controls);
        int normalTableHeight = 0;
        for (QSize size : {QSize(700, 500), QSize(900, 650), QSize(1000, 800), QSize(1280, 940)}) {
            window.resize(size); tabs->setCurrentWidget(page); QTest::qWait(150);
            QCOMPARE(window.size(), size);
            QCOMPARE(configuration->horizontalScrollBar()->maximum(), 0);
            QVERIFY(page->rect().contains(controls->geometry()));
            QVERIFY(table->height() >= page->fontMetrics().lineSpacing() * 4);
            if (size.width() >= 900) QCOMPARE(configuration->verticalScrollBar()->maximum(), 0);
            if (size.width() == 1000) normalTableHeight = table->height();
            if (size.width() == 1280) QVERIFY(table->height() > normalTableHeight + 100);
            window.grab().save(QCoreApplication::applicationDirPath() + QString("/script-%1.png").arg(size.width()));
            tabs->setCurrentWidget(window.findChild<KeyboardClickPage*>()); QTest::qWait(50);
            QCOMPARE(window.size(), size);
            window.grab().save(QCoreApplication::applicationDirPath() + QString("/keyboard-%1.png").arg(size.width()));
        }
        window.hide();
    }
    void scriptPageWorkflow() {
        AppController controller; QVERIFY(controller.initialize()); controller.unregisterAllHotkeys();
        MainWindow window(&controller); window.resize(1000, 800); window.show();
        auto* page = window.findChild<ScriptPage*>();
        window.findChild<QTabWidget*>()->setCurrentWidget(page);
        auto button = [page](const QString& text) -> QPushButton* {
            for (auto* b : page->findChildren<QPushButton*>()) if (b->text().contains(text)) return b;
            return nullptr;
        };
        QVERIFY(button(QStringLiteral("开始录制"))); QVERIFY(focusReceiver());
        button(QStringLiteral("开始录制"))->click();
        QCOMPARE(controller.taskManager()->currentState(), TaskState::Recording);
        controller.hookManager()->setIgnoreSimulatedInput(false);
        QVERIFY(controller.inputSimulator()->keyPress(VK_F24, 1)); QTest::qWait(100);
        button(QStringLiteral("停止录制"))->click();
        QCOMPARE(controller.taskManager()->currentState(), TaskState::Idle);
        QVERIFY(page->currentDocument().eventCount() >= 2);
        QTemporaryDir directory; const QString path = directory.filePath("ui-workflow.kms");
        bool selected = false;
        bool validSuggestedName = false;
        auto selectPath = [&]() {
            if (auto* dialog = qobject_cast<QFileDialog*>(qApp->activeModalWidget())) {
                selected = true;
                validSuggestedName = !QFileInfo(dialog->selectedFiles().value(0)).fileName().contains(':');
                auto* fileName = dialog->findChild<QLineEdit*>("fileNameEdit");
                if (fileName) fileName->setText(QDir::toNativeSeparators(path));
                else dialog->selectFile(path);
                QMetaObject::invokeMethod(dialog, "accept", Qt::QueuedConnection);
            }
        };
        QTimer guard;
        guard.setSingleShot(true);
        connect(&guard, &QTimer::timeout, []() {
            if (auto* dialog = qobject_cast<QDialog*>(qApp->activeModalWidget())) dialog->reject();
        });
        guard.start(3000);
        QTimer::singleShot(100, selectPath);
        QVERIFY(QMetaObject::invokeMethod(page, "onSaveAsScript")); guard.stop(); QVERIFY(selected); QVERIFY(validSuggestedName); QVERIFY(QFile::exists(path));
        selected = false; guard.start(3000); QTimer::singleShot(100, selectPath);
        QVERIFY(QMetaObject::invokeMethod(page, "onImportScript")); guard.stop(); QVERIFY(selected);
        QVERIFY(page->currentDocument().eventCount() >= 2);
        QVERIFY(focusReceiver()); button(QStringLiteral("开始回放"))->click();
        QTRY_COMPARE_WITH_TIMEOUT(controller.taskManager()->currentState(), TaskState::Idle, 2000);
        auto doc = page->currentDocument();
        doc.events = {key(ScriptEventType::KeyDown, 0), key(ScriptEventType::KeyUp, 5000)};
        page->setDocument(doc); QVERIFY(focusReceiver());
        button(QStringLiteral("开始回放"))->click(); QTest::qWait(100);
        QCOMPARE(controller.taskManager()->currentState(), TaskState::Playing);
        button(QStringLiteral("停止回放"))->click();
        QTRY_COMPARE_WITH_TIMEOUT(controller.taskManager()->currentState(), TaskState::Idle, 1000);
        QVERIFY(!controller.inputSimulator()->hasPressedKeys());
        window.hide();
    }
    void menuActionsAndFiles() {
        AppController controller; QVERIFY(controller.initialize()); controller.unregisterAllHotkeys();
        controller.settingsManager()->setMouseStartDelay(60000);
        controller.settingsManager()->setKeyboardStartDelay(60000);
        KeyInfo info; info.qtKey = Qt::Key_F24; info.winVk = VK_F24; info.displayName = "F24";
        controller.settingsManager()->setKeyboardKeyInfo(info);
        MainWindow window(&controller); window.show();
        auto* page = window.findChild<ScriptPage*>();
        auto action = [&](const char* name) { return window.findChild<QAction*>(name); };
        const char* names[] = {"runMouse", "runKeyboard", "runRecording", "runPlayback"};
        const TaskState states[] = {TaskState::MouseClicking, TaskState::KeyboardClicking, TaskState::Recording, TaskState::Playing};
        for (int i = 0; i < 4; ++i) {
            ScriptDocument doc; doc.name = "menu test"; doc.filePath = "menu-test.kms";
            doc.events = {key(ScriptEventType::KeyDown, 60000), key(ScriptEventType::KeyUp, 60001)};
            page->setDocument(doc);
            QVERIFY(action(names[i])); action(names[i])->trigger();
            QCOMPARE(controller.taskManager()->currentState(), states[i]);
            for (auto name : names) QVERIFY(!action(name)->isEnabled());
            QVERIFY(!action("importScript")->isEnabled());
            action(names[i])->trigger(); QCOMPARE(controller.taskManager()->currentState(), states[i]);
            controller.taskManager()->requestStop();
            QTRY_COMPARE(controller.taskManager()->currentState(), TaskState::Idle);
            for (auto name : names) QVERIFY(action(name)->isEnabled());
            QVERIFY(action("importScript")->isEnabled());
        }
        ScriptDocument doc; doc.name = "menu save";
        doc.events = {key(ScriptEventType::KeyDown, 0), key(ScriptEventType::KeyUp, 10)};
        page->setDocument(doc);
        QTemporaryDir directory; const QString path = directory.filePath("menu.kms");
        bool selected = false;
        auto selectPath = [&] {
            if (auto* dialog = qobject_cast<QFileDialog*>(qApp->activeModalWidget())) {
                selected = true;
                dialog->findChild<QLineEdit*>("fileNameEdit")->setText(QDir::toNativeSeparators(path));
                QMetaObject::invokeMethod(dialog, "accept", Qt::QueuedConnection);
            }
        };
        QTimer guard; guard.setSingleShot(true);
        connect(&guard, &QTimer::timeout, [] { if (auto* dialog = qobject_cast<QDialog*>(qApp->activeModalWidget())) dialog->reject(); });
        guard.start(3000); QTimer::singleShot(100, selectPath);
        action("saveScriptAs")->trigger(); guard.stop(); QVERIFY(selected); QVERIFY(QFile::exists(path));
        action("saveScript")->trigger(); ScriptDocument loaded; QVERIFY(loaded.loadFromFile(path)); QCOMPARE(loaded.eventCount(), 2);
        selected = false; guard.start(3000); QTimer::singleShot(100, selectPath);
        action("importScript")->trigger(); guard.stop(); QVERIFY(selected); QCOMPARE(page->currentDocument().filePath, path);
        action("exitApplication")->trigger(); QVERIFY(!window.isVisible());
        QVERIFY(!window.findChild<QSystemTrayIcon*>()->isVisible());
        controller.settingsManager()->setMouseStartDelay(0); controller.settingsManager()->setKeyboardStartDelay(0);
    }
    void restoreDefaultsAction() {
        AppController controller; QVERIFY(controller.initialize()); controller.unregisterAllHotkeys();
        auto* settings = controller.settingsManager();
        settings->setMouseClickInterval(1234); settings->setKeyboardInterval(2345);
        MainWindow window(&controller); window.show(); QTest::qWait(100);
        auto* action = window.findChild<QAction*>("restoreDefaults"); QVERIFY(action);
        QCOMPARE(action->shortcut(), QKeySequence("Ctrl+Shift+R"));
        QCOMPARE(action->shortcutContext(), Qt::WindowShortcut);
        auto* page = window.findChild<ScriptPage*>();
        ScriptDocument doc; doc.name = "preserve unsaved";
        doc.events = {key(ScriptEventType::KeyDown, 0), key(ScriptEventType::KeyUp, 10)};
        page->setDocument(doc);
        MouseClickEngine::Config config; config.startDelayMs = 60000;
        controller.mouseClickEngine()->setConfig(config);
        QVERIFY(controller.taskManager()->requestStartMouseClick());
        QTimer::singleShot(50, [] {
            if (auto* box = qobject_cast<QMessageBox*>(qApp->activeModalWidget())) box->button(QMessageBox::No)->click();
        });
        action->trigger();
        QCOMPARE(settings->mouseClickInterval(), 1234);
        QCOMPARE(controller.taskManager()->currentState(), TaskState::MouseClicking);
        QVERIFY(action->isEnabled());
        QTimer::singleShot(50, [] {
            if (auto* box = qobject_cast<QMessageBox*>(qApp->activeModalWidget())) box->button(QMessageBox::Yes)->click();
        });
        action->trigger();
        QCOMPARE(controller.taskManager()->currentState(), TaskState::Idle);
        QCOMPARE(settings->mouseClickInterval(), 100);
        QCOMPARE(settings->keyboardInterval(), 100);
        QVERIFY(!controller.inputSimulator()->hasPressedKeys());
        QCOMPARE(page->currentDocument().name, doc.name);
        QCOMPARE(page->currentDocument().eventCount(), 2);
        QVERIFY(window.findChild<QAction*>("runMouse")->isEnabled());
        QVERIFY(window.findChild<QAction*>("showStatusBar")->isChecked());
        QVERIFY(!window.findChild<QAction*>("alwaysOnTop")->isChecked());
        window.hide();
    }
    void helpAndViewActions() {
        AppController controller; QVERIFY(controller.initialize()); controller.unregisterAllHotkeys();
        MainWindow window(&controller); window.show(); QTest::qWait(100);
        auto action = [&](const char* name) { return window.findChild<QAction*>(name); };
        const QRect geometry = window.geometry();
        action("alwaysOnTop")->setChecked(true); QTest::qWait(50);
        QVERIFY(window.isVisible()); QVERIFY(window.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
        QCOMPARE(window.geometry(), geometry); QVERIFY(controller.settingsManager()->alwaysOnTop());
        action("alwaysOnTop")->setChecked(false);
        action("showStatusBar")->setChecked(false); QVERIFY(window.statusBar()->isHidden());
        QVERIFY(!controller.settingsManager()->statusBarVisible()); action("showStatusBar")->setChecked(true);
        if (QSystemTrayIcon::isSystemTrayAvailable()) {
            action("minimizeToTray")->trigger(); QVERIFY(!window.isVisible());
            window.findChild<QSystemTrayIcon*>()->contextMenu()->actions().first()->trigger(); QVERIFY(window.isVisible());
        }
        UrlReceiver urls; QDesktopServices::setUrlHandler("https", &urls, "receive");
        QDesktopServices::setUrlHandler("file", &urls, "receive");
        action("github")->trigger(); QCOMPARE(urls.last, QUrl(QString::fromLatin1(KmmProjectUrl)));
        urls.last.clear(); action("feedback")->trigger(); QCOMPARE(urls.last, QUrl(QString::fromLatin1(KmmProjectUrl)));
        const QString help = QCoreApplication::applicationDirPath() + "/docs/user-guide.html";
        QVERIFY(QFile::exists(help)); action("userGuide")->trigger(); QCOMPARE(urls.last, QUrl::fromLocalFile(help));
        QDesktopServices::unsetUrlHandler("https"); QDesktopServices::unsetUrlHandler("file");
        // Only the deployed test copy is moved; source and shipping copies stay intact.
        QVERIFY(QFile::rename(help, help + ".test-backup"));
        bool missingMessage = false;
        QTimer::singleShot(100, [&] { if (auto* box = qobject_cast<QMessageBox*>(qApp->activeModalWidget())) { missingMessage = box->text() == QStringLiteral("未找到本地使用说明文件。"); box->accept(); } });
        action("userGuide")->trigger();
        const bool restored = QFile::rename(help + ".test-backup", help); QVERIFY(restored); QVERIFY(missingMessage);
        const QString clipboard = QApplication::clipboard()->text();
        bool copied = false;
        QTimer::singleShot(100, [&] {
            if (auto* about = qobject_cast<AboutDialog*>(qApp->activeModalWidget())) {
                auto* text = about->findChild<QTextBrowser*>("aboutInformation");
                about->findChild<QPushButton*>("copyInformation")->click();
                copied = text && text->textInteractionFlags().testFlag(Qt::TextSelectableByMouse)
                    && QApplication::clipboard()->text() == about->information();
                about->accept();
            }
        });
        action("aboutKmm")->trigger(); QVERIFY(copied); QApplication::clipboard()->setText(clipboard);
        bool qtOpened = false;
        QTimer::singleShot(100, [&] { if (auto* box = qobject_cast<QMessageBox*>(qApp->activeModalWidget())) { qtOpened = true; box->accept(); } });
        action("aboutQt")->trigger(); QVERIFY(qtOpened); window.hide();
    }
    void defaultWindowAndRecovery() {
        QSettings raw(QSettings::defaultFormat(), QSettings::UserScope, "KeyMouseMaster", "KeyMouseMaster"); raw.remove("window"); raw.sync();
        {
            AppController controller; QVERIFY(controller.initialize()); controller.unregisterAllHotkeys();
            MainWindow window(&controller); window.show();
            auto* page = window.findChild<ScriptPage*>(); window.findChild<QTabWidget*>()->setCurrentWidget(page);
            QTest::qWait(150);
            auto* area = page->findChild<QScrollArea*>("scriptConfiguration");
            qInfo() << "Default window" << window.size() << "DPR" << window.devicePixelRatioF();
            QCOMPARE(area->verticalScrollBar()->maximum(), 0);
            QCOMPARE(area->horizontalScrollBar()->maximum(), 0);
            QVERIFY(page->findChild<QTableView*>("scriptEvents")->height() >= page->fontMetrics().lineSpacing() * 4);
            window.grab().save(QCoreApplication::applicationDirPath() + "/default-window.png");
            window.resize(700, 500); window.hide();
        }
        {
            AppController controller; QVERIFY(controller.initialize()); controller.unregisterAllHotkeys();
            MainWindow window(&controller); window.show(); QTest::qWait(50); QCOMPARE(window.size(), QSize(700, 500)); window.hide();
        }
        raw.setValue("window/size", QSize(1, 1)); raw.setValue("window/position", QPoint(99999, 99999)); raw.sync();
        {
            AppController controller; QVERIFY(controller.initialize()); controller.unregisterAllHotkeys();
            MainWindow window(&controller); window.show(); QTest::qWait(50);
            QVERIFY(window.width() >= 700); QVERIFY(window.height() >= 500);
            QVERIFY(window.screen()->availableGeometry().contains(window.geometry().center())); window.hide();
        }
        raw.setValue("window/maximized", true); raw.sync();
        {
            AppController controller; QVERIFY(controller.initialize()); controller.unregisterAllHotkeys();
            MainWindow window(&controller); window.show(); QTest::qWait(50); QVERIFY(window.isMaximized());
            window.showNormal(); window.resize(900, 650); window.hide();
        }
    }
    void settingsAndUiChain() {
        AppController controller; QVERIFY(controller.initialize()); controller.unregisterAllHotkeys();
        MainWindow window(&controller); window.show(); QTest::qWait(100);
        QVERIFY(window.isVisible());
        auto tabs = window.findChild<QTabWidget*>(); QVERIFY(tabs); QCOMPARE(tabs->count(), 3);
        for (int i = 0; i < 3; ++i) { tabs->setCurrentIndex(i); QTest::qWait(30); QCOMPARE(tabs->currentIndex(), i); }
        controller.unregisterAllHotkeys();
        KeyInfo info; info.qtKey = Qt::Key_F24; info.winVk = VK_F24; info.hasCtrl = true; info.hasAlt = true;
        info.displayName = "Ctrl+Alt+F24"; controller.settingsManager()->setKeyboardKeyInfo(info);
        controller.settingsManager()->sync(); SettingsManager second;
        QVERIFY(second.keyboardKeyInfo().hasCtrl); QVERIFY(second.keyboardKeyInfo().hasAlt);
        controller.applyKeyboardSettings(); QVERIFY(controller.keyboardClickEngine()->config().hasAlt);
        auto page = window.findChild<ScriptPage*>(); QVERIFY(page);
        RecordingSettings settings; settings.ignoreOwnWindow = false; settings.ignoreSimulated = false;
        QVERIFY(controller.taskManager()->requestStartRecording(settings));
        auto stops = page->findChildren<QPushButton*>(); bool enabledStop = false;
        for (auto* button : stops) if (button->text().contains(QStringLiteral("停止录制"))) enabledStop = button->isEnabled();
        QVERIFY(enabledStop);
        QVERIFY(focusReceiver()); QVERIFY(controller.inputSimulator()->keyPress(VK_F24, 1)); QTest::qWait(50);
        controller.taskManager()->requestStop(); QCOMPARE(controller.taskManager()->currentState(), TaskState::Idle);
        QVERIFY(page->currentDocument().eventCount() >= 2);
        ScriptDocument saved = page->currentDocument(); QTemporaryDir dir;
        QVERIFY(saved.saveToFile(dir.filePath("recorded.kms"))); ScriptDocument loaded; QVERIFY(loaded.loadFromFile(saved.filePath));
        page->setDocument(loaded);
        QVERIFY(QMetaObject::invokeMethod(&window, "onScriptPlaybackStart"));
        QTRY_VERIFY_WITH_TIMEOUT(!controller.taskManager()->isAnyTaskRunning(), 2000);
        window.show(); window.grab().save(QCoreApplication::applicationDirPath() + "/ui-smoke.png");
        controller.emergencyStop();
        auto* tray = window.findChild<QSystemTrayIcon*>(); QVERIFY(tray); QVERIFY(tray->isVisible());
        QVERIFY(tray->contextMenu());
        QAction *showAction = nullptr, *stopAction = nullptr, *resetAction = nullptr;
        for (auto* action : tray->contextMenu()->actions()) {
            if (action->text() == QStringLiteral("显示主窗口")) showAction = action;
            if (action->text() == QStringLiteral("停止当前任务")) stopAction = action;
            if (action->text() == QStringLiteral("输入状态复位")) resetAction = action;
        }
        QVERIFY(showAction); QVERIFY(stopAction); QVERIFY(resetAction);
        QVERIFY(!stopAction->isEnabled());
        window.hide(); showAction->trigger(); QVERIFY(window.isVisible());
        MouseClickEngine::Config delayed; delayed.startDelayMs = 60000;
        controller.mouseClickEngine()->setConfig(delayed);
        QVERIFY(controller.taskManager()->requestStartMouseClick());
        QVERIFY(stopAction->isEnabled()); stopAction->trigger();
        QTRY_COMPARE_WITH_TIMEOUT(controller.taskManager()->currentState(), TaskState::Idle, 1000);
        resetAction->trigger(); QVERIFY(!controller.inputSimulator()->hasPressedKeys());
        window.hide();
    }
};

int main(int argc, char** argv) {
    WindowsDpiManager::initializeDpiSupport();
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    QTemporaryDir settingsDir;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
    Logger::instance()->init(QCoreApplication::applicationDirPath() + "/logs");
    KmmTests tests;
    const int result = QTest::qExec(&tests, argc, argv);
    Logger::instance()->shutdown();
    return result;
}
#include "tst_kmm.moc"
