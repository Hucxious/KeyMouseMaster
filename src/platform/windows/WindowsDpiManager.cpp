#include "WindowsDpiManager.h"
#include "Logger.h"
#include <QGuiApplication>
#include <QScreen>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellscalingapi.h>
#endif

WindowsDpiManager::WindowsDpiManager(QObject* parent)
    : QObject(parent)
{
}

void WindowsDpiManager::initializeDpiSupport()
{
#ifdef Q_OS_WIN
    // 尝试设置 Per-Monitor DPI Aware V2 (Windows 10 1703+)
    // 需要在 manifest 中同时声明，这里是程序化设置作为备用

    // 方法1: 使用 SetProcessDpiAwarenessContext (Win10 1703+)
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        typedef BOOL (WINAPI *SetProcessDpiAwarenessContextFunc)(DPI_AWARENESS_CONTEXT);
        auto pSetProcessDpiAwarenessContext =
            reinterpret_cast<SetProcessDpiAwarenessContextFunc>(
                GetProcAddress(hUser32, "SetProcessDpiAwarenessContext"));

        if (pSetProcessDpiAwarenessContext) {
            // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
            pSetProcessDpiAwarenessContext(
                reinterpret_cast<DPI_AWARENESS_CONTEXT>(-4));
            LOG_INFO("已设置 Per-Monitor DPI Aware V2");
            return;
        }
    }

    // 方法2: 使用 SetProcessDpiAwareness (Win8.1+)
    HMODULE hShcore = LoadLibraryW(L"shcore.dll");
    if (hShcore) {
        typedef HRESULT (WINAPI *SetProcessDpiAwarenessFunc)(PROCESS_DPI_AWARENESS);
        auto pSetProcessDpiAwareness =
            reinterpret_cast<SetProcessDpiAwarenessFunc>(
                GetProcAddress(hShcore, "SetProcessDpiAwareness"));

        if (pSetProcessDpiAwareness) {
            // PROCESS_PER_MONITOR_DPI_AWARE = 2
            HRESULT hr = pSetProcessDpiAwareness(static_cast<PROCESS_DPI_AWARENESS>(2));
            if (SUCCEEDED(hr)) {
                LOG_INFO("已设置 Per-Monitor DPI Aware");
                FreeLibrary(hShcore);
                return;
            }
        }
        FreeLibrary(hShcore);
    }

    // 方法3: 回退到 SetProcessDPIAware (Vista+)
    SetProcessDPIAware();
    LOG_INFO("已设置 System DPI Aware (回退模式)");
#else
    // 非Windows平台: 使用Qt的高DPI设置
    // 这些在 main.cpp 中通过 Qt 属性设置
    LOG_INFO("非Windows平台: 使用Qt高DPI属性");
#endif
}

int WindowsDpiManager::primaryDpi()
{
    QScreen* primary = QGuiApplication::primaryScreen();
    if (primary)
        return static_cast<int>(primary->logicalDotsPerInch());
    return 96;
}

qreal WindowsDpiManager::getScaleFactor(void* hwnd)
{
    Q_UNUSED(hwnd)
#ifdef Q_OS_WIN
    if (hwnd) {
        UINT dpi = GetDpiForWindow(reinterpret_cast<HWND>(hwnd));
        return dpi / 96.0;
    }
#endif
    QScreen* primary = QGuiApplication::primaryScreen();
    if (primary)
        return primary->devicePixelRatio();
    return 1.0;
}

void WindowsDpiManager::logDpiInfo()
{
    QList<QScreen*> screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        QScreen* s = screens[i];
        LOG_INFO(QString("屏幕 %1: %2 DPI=%3 缩放=%4 几何=%5x%6+(%7,%8)")
            .arg(i).arg(s->name())
            .arg(s->logicalDotsPerInch())
            .arg(s->devicePixelRatio())
            .arg(s->geometry().width()).arg(s->geometry().height())
            .arg(s->geometry().x()).arg(s->geometry().y()));
    }
}
