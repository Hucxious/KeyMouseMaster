#ifndef WINDOWSDIPMANAGER_H
#define WINDOWSDIPMANAGER_H

#include <QObject>

// ============================================================================
// Windows 高DPI管理器
// 设置 Per-Monitor DPI Aware V2，管理DPI相关配置
// ============================================================================
class WindowsDpiManager : public QObject
{
    Q_OBJECT

public:
    explicit WindowsDpiManager(QObject* parent = nullptr);

    // 初始化高DPI支持 (在 QApplication 创建前调用)
    static void initializeDpiSupport();

    // 获取主显示器DPI
    static int primaryDpi();

    // 获取显示器缩放比例
    static qreal getScaleFactor(void* hwnd);

    // 打印当前DPI信息到日志
    static void logDpiInfo();

signals:
    void dpiChanged(void* hwnd, int newDpi);

private:
#ifdef Q_OS_WIN
    // DPI感知上下文
    static int s_dpiAwareness;
#endif
};

#endif // WINDOWSDIPMANAGER_H
