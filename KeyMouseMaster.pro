# KeyMouseMaster - 键鼠大师
# Qt 5.15+ / Qt 6 compatible
# Target: Windows 10/11 64-bit

QT += core gui widgets
CONFIG += c++17
TEMPLATE = app
TARGET = KeyMouseMaster

# Windows-specific configuration
win32 {
    DEFINES += NOMINMAX WIN32_LEAN_AND_MEAN
    LIBS += -luser32 -lshell32 -lshcore
    RC_ICONS = resources/icons/app.ico
    # QMAKE_LFLAGS += /MANIFESTUAC:\"level=\'requireAdministrator\' uiAccess=\'false\'\"  # MSVC only
}

# Platform-independent defines
DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += APP_NAME=\\\"KeyMouseMaster\\\"
DEFINES += APP_VERSION=\\\"1.0.0\\\"

# Debug/Release output directories
CONFIG(debug, debug|release) {
    TARGET = KeyMouseMaster_d
    DESTDIR = $$OUT_PWD/debug
    OBJECTS_DIR = $$OUT_PWD/debug/obj
    MOC_DIR = $$OUT_PWD/debug/moc
    RCC_DIR = $$OUT_PWD/debug/rcc
} else {
    DESTDIR = $$OUT_PWD/release
    OBJECTS_DIR = $$OUT_PWD/release/obj
    MOC_DIR = $$OUT_PWD/release/moc
    RCC_DIR = $$OUT_PWD/release/rcc
}

win32-msvc*: QMAKE_CXXFLAGS += /utf-8

INCLUDEPATH += \
    $$PWD/src \
    $$PWD/src/ui \
    $$PWD/src/core \
    $$PWD/src/models \
    $$PWD/src/platform/windows \
    $$PWD/src/settings \
    $$PWD/src/utils

# Model sources
SOURCES += \
    src/models/AppTypes.cpp \
    src/models/MonitorInfo.cpp \
    src/models/ScriptEvent.cpp \
    src/models/ScriptDocument.cpp \
    src/models/ScriptEventTableModel.cpp

HEADERS += \
    src/models/AppTypes.h \
    src/models/ScriptEvent.h \
    src/models/ScriptDocument.h \
    src/models/MonitorInfo.h \
    src/models/ScriptEventTableModel.h

# Utility sources
SOURCES += \
    src/utils/Logger.cpp \
    src/utils/KeyMapper.cpp \
    src/utils/ValidationUtils.cpp \
    src/utils/TimeUtils.cpp

HEADERS += \
    src/utils/Logger.h \
    src/utils/KeyMapper.h \
    src/utils/ValidationUtils.h \
    src/utils/TimeUtils.h

# Settings
SOURCES += \
    src/settings/SettingsManager.cpp

HEADERS += \
    src/settings/SettingsManager.h

# Platform (Windows) - headers always available, sources use #ifdef guards
SOURCES += \
    src/platform/windows/WindowsInputSimulator.cpp \
    src/platform/windows/WindowsHookManager.cpp \
    src/platform/windows/WindowsHotkeyManager.cpp \
    src/platform/windows/WindowsMonitorBackend.cpp \
    src/platform/windows/WindowsDpiManager.cpp

HEADERS += \
    src/platform/windows/WinCompat.h \
    src/platform/windows/WindowsInputSimulator.h \
    src/platform/windows/WindowsHookManager.h \
    src/platform/windows/WindowsHotkeyManager.h \
    src/platform/windows/WindowsMonitorBackend.h \
    src/platform/windows/WindowsDpiManager.h

# Core
SOURCES += \
    src/core/AppController.cpp \
    src/core/TaskManager.cpp \
    src/core/MouseClickEngine.cpp \
    src/core/KeyboardClickEngine.cpp \
    src/core/ScriptRecorder.cpp \
    src/core/ScriptPlayer.cpp \
    src/core/ScriptSerializer.cpp \
    src/core/MonitorManager.cpp \
    src/core/CoordinateMapper.cpp \
    src/core/InputStateManager.cpp

HEADERS += \
    src/core/AppController.h \
    src/core/TaskManager.h \
    src/core/MouseClickEngine.h \
    src/core/KeyboardClickEngine.h \
    src/core/ScriptRecorder.h \
    src/core/ScriptPlayer.h \
    src/core/ScriptSerializer.h \
    src/core/MonitorManager.h \
    src/core/CoordinateMapper.h \
    src/core/InputStateManager.h

# UI
SOURCES += \
    src/ui/AboutDialog.cpp \
    src/ui/MainWindow.cpp \
    src/ui/MouseClickPage.cpp \
    src/ui/KeyboardClickPage.cpp \
    src/ui/ScriptPage.cpp \
    src/ui/HotkeyEdit.cpp \
    src/ui/KeyCaptureEdit.cpp \
    src/ui/MonitorPreviewWidget.cpp

HEADERS += \
    src/ui/AboutDialog.h \
    src/ui/MainWindow.h \
    src/ui/MouseClickPage.h \
    src/ui/KeyboardClickPage.h \
    src/ui/ScriptPage.h \
    src/ui/HotkeyEdit.h \
    src/ui/KeyCaptureEdit.h \
    src/ui/MonitorPreviewWidget.h

# Main entry
SOURCES += \
    src/main.cpp

RESOURCES += \
    resources/resources.qrc

# Install target
win32 {
    target.path = $$PWD/install
    INSTALLS += target
}

# Offline help is an explicit dependency, so documentation-only edits are deployed too.
help_file.target = $$DESTDIR/docs/user-guide.html
help_file.depends = $$PWD/docs/user-guide.html
help_file.commands = if not exist $$shell_quote($$shell_path($$DESTDIR/docs)) mkdir $$shell_quote($$shell_path($$DESTDIR/docs))
help_file.commands += $$escape_expand(\n\t) $$QMAKE_COPY $$shell_quote($$shell_path($$PWD/docs/user-guide.html)) $$shell_quote($$shell_path($$help_file.target))
QMAKE_EXTRA_TARGETS += help_file
PRE_TARGETDEPS += $$help_file.target

help_install.files = $$PWD/docs/user-guide.html
help_install.path = $$target.path/docs
INSTALLS += help_install
