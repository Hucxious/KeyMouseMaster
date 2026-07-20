#ifndef WINCOMPAT_H
#define WINCOMPAT_H

// ============================================================================
// Windows API 类型桩，用于非Windows平台编译
// 在Windows平台由 <windows.h> 提供真实定义
// ============================================================================

#ifndef Q_OS_WIN

// 基础类型
typedef unsigned long      DWORD;
typedef unsigned long      ULONG_PTR;
typedef long               LONG;
typedef short              SHORT;
typedef int                BOOL;
typedef unsigned short     WORD;
typedef unsigned int       UINT;

// Windows消息常量 (鼠标钩子)
#define WM_MOUSEMOVE     0x0200
#define WM_LBUTTONDOWN   0x0201
#define WM_LBUTTONUP     0x0202
#define WM_RBUTTONDOWN   0x0204
#define WM_RBUTTONUP     0x0205
#define WM_MBUTTONDOWN   0x0207
#define WM_MBUTTONUP     0x0208
#define WM_MOUSEWHEEL    0x020A
#define WM_MOUSEHWHEEL   0x020E
#define WM_XBUTTONDOWN   0x020B
#define WM_XBUTTONUP     0x020C
#define WM_HOTKEY        0x0312

// 鼠标额外数据
#define XBUTTON1         0x0001
#define XBUTTON2         0x0002
#define WHEEL_DELTA      120

// 键盘消息
#define WM_KEYDOWN       0x0100
#define WM_KEYUP         0x0101
#define WM_SYSKEYDOWN    0x0104
#define WM_SYSKEYUP      0x0105

// 键盘标志
#define KF_EXTENDED      0x0100
#define LLKHF_EXTENDED   (KF_EXTENDED >> 8)

// SendInput 标志
#define MOUSEEVENTF_MOVE         0x0001
#define MOUSEEVENTF_LEFTDOWN     0x0002
#define MOUSEEVENTF_LEFTUP       0x0004
#define MOUSEEVENTF_RIGHTDOWN    0x0008
#define MOUSEEVENTF_RIGHTUP      0x0010
#define MOUSEEVENTF_MIDDLEDOWN   0x0020
#define MOUSEEVENTF_MIDDLEUP     0x0040
#define MOUSEEVENTF_XDOWN        0x0080
#define MOUSEEVENTF_XUP          0x0100
#define MOUSEEVENTF_WHEEL        0x0800
#define MOUSEEVENTF_HWHEEL       0x1000
#define MOUSEEVENTF_ABSOLUTE     0x8000
#define MOUSEEVENTF_VIRTUALDESK  0x4000

#define KEYEVENTF_EXTENDEDKEY    0x0001
#define KEYEVENTF_KEYUP          0x0002
#define KEYEVENTF_SCANCODE       0x0008

// 虚拟键码
#define VK_CONTROL    0x11
#define VK_LCONTROL   0xA2
#define VK_RCONTROL   0xA3
#define VK_SHIFT      0x10
#define VK_LSHIFT     0xA0
#define VK_RSHIFT     0xA1
#define VK_MENU       0x12
#define VK_LMENU      0xA4
#define VK_RMENU      0xA5
#define VK_LWIN       0x5B
#define VK_RWIN       0x5C
#define VK_CAPITAL    0x14
#define VK_NUMLOCK    0x90
#define VK_SCROLL     0x91
#define VK_SNAPSHOT   0x2C

// 输入结构 (简化桩)
typedef struct tagMOUSEINPUT {
    LONG      dx;
    LONG      dy;
    DWORD     mouseData;
    DWORD     dwFlags;
    DWORD     time;
    ULONG_PTR dwExtraInfo;
} MOUSEINPUT;

typedef struct tagKEYBDINPUT {
    WORD      wVk;
    WORD      wScan;
    DWORD     dwFlags;
    DWORD     time;
    ULONG_PTR dwExtraInfo;
} KEYBDINPUT;

typedef struct tagINPUT {
    DWORD type;
    union {
        MOUSEINPUT   mi;
        KEYBDINPUT   ki;
    };
} INPUT;
#define INPUT_MOUSE    0
#define INPUT_KEYBOARD 1

// 显示器
#define MONITORINFOF_PRIMARY  0x00000001

// 修饰键
#define MOD_ALT      0x0001
#define MOD_CONTROL  0x0002
#define MOD_SHIFT    0x0004
#define MOD_WIN      0x0008

// 热键错误
#define ERROR_HOTKEY_ALREADY_REGISTERED  1409
#define ERROR_ACCESS_DENIED              5

// 工具宏
#define HIWORD(l)  ((WORD)(((DWORD)(l) >> 16) & 0xFFFF))
#define LOWORD(l)  ((WORD)((DWORD)(l) & 0xFFFF))

#include <cstdint>
typedef uint32_t* HHOOK;     // 钩子句柄桩
typedef void*     HWND;      // 窗口句柄桩
typedef void*     HMONITOR;  // 显示器句柄桩
typedef void*     HINSTANCE; // 实例句柄桩

// 函数桩
inline void* GetModuleHandleW(const wchar_t*) { return nullptr; }
inline int GetSystemMetrics(int) { return 0; }
#define SM_XVIRTUALSCREEN  76
#define SM_YVIRTUALSCREEN  77
#define SM_CXVIRTUALSCREEN 78
#define SM_CYVIRTUALSCREEN 79

#else
// Windows 平台使用真实头文件
#include <windows.h>
#include <winuser.h>
#endif

#endif // WINCOMPAT_H
