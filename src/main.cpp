#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>
#include <QIcon>
#include <QAbstractNativeEventFilter>

#include "core/AppController.h"
#include "ui/MainWindow.h"
#include "platform/windows/WindowsDpiManager.h"
#include "platform/windows/WindowsHotkeyManager.h"
#include "utils/Logger.h"

int main(int argc, char* argv[])
{
    // ========================================================================
    // 高DPI初始化 (必须在 QApplication 创建前)
    // ========================================================================
    // Qt 5 高DPI属性设置
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
#endif

    QApplication app(argc, argv);
    app.setApplicationName("KeyMouseMaster");
    app.setApplicationDisplayName("键鼠大师");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Hucxious");
    app.setWindowIcon(QIcon(":/icons/app.png"));  // 窗口和任务栏图标
    app.setQuitOnLastWindowClosed(false);  // 支持系统托盘

    // Windows 高DPI感知设置
    WindowsDpiManager::initializeDpiSupport();

    // ========================================================================
    // 日志初始化
    // ========================================================================
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                     + "/logs";
    Logger::instance()->init(logDir);
    LOG_INFO("========================================");
    LOG_INFO("键鼠大师 KeyMouseMaster v1.0 启动");
    LOG_INFO(QString("Qt版本: %1").arg(qVersion()));
    LOG_INFO(QString("日志目录: %1").arg(logDir));

    // 记录DPI信息
    WindowsDpiManager::logDpiInfo();

    // ========================================================================
    // 应用控制器初始化
    // ========================================================================
    AppController controller;

    QObject::connect(&controller, &AppController::initialized, [&]() {
        LOG_INFO("应用控制器初始化成功");
    });

    QObject::connect(&controller, &AppController::initializationError, [&](const QString& msg) {
        LOG_CRITICAL("初始化失败: " + msg);
        QMessageBox::critical(nullptr, "启动失败",
            QString("键鼠大师初始化失败:\n%1\n\n请检查日志文件了解详情。").arg(msg));
    });

    if (!controller.initialize()) {
        LOG_CRITICAL("应用控制器初始化失败，程序退出");
        return 1;
    }

    // ========================================================================
    // 主窗口
    // ========================================================================
    MainWindow mainWindow(&controller);
    mainWindow.show();

    LOG_INFO("主窗口已显示");

    // ========================================================================
    // 安装原生事件过滤器 (用于全局热键)
    // ========================================================================
    app.installNativeEventFilter(static_cast<QAbstractNativeEventFilter*>(controller.hotkeyManager()));

    // ========================================================================
    // 事件循环
    // ========================================================================
    int result = app.exec();

    // ========================================================================
    // 清理
    // ========================================================================
    LOG_INFO("键鼠大师正在退出...");
    controller.emergencyStop();
    app.removeNativeEventFilter(static_cast<QAbstractNativeEventFilter*>(controller.hotkeyManager()));
    Logger::instance()->shutdown();

    return result;
}
