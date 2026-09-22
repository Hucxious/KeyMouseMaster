QT += core gui widgets testlib
CONFIG += c++17 testcase console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = kmm_tests
KMM_ROOT = $$clean_path($$PWD/..)
INCLUDEPATH += $$KMM_ROOT/src $$KMM_ROOT/src/core $$KMM_ROOT/src/models $$KMM_ROOT/src/utils $$KMM_ROOT/src/platform/windows
KMM_DIRS = core models utils settings ui platform/windows
for(dir, KMM_DIRS) {
    SOURCES += $$files($$KMM_ROOT/src/$$dir/*.cpp)
    HEADERS += $$files($$KMM_ROOT/src/$$dir/*.h)
}
SOURCES += $$PWD/tst_kmm.cpp
RESOURCES += $$KMM_ROOT/resources/resources.qrc
win32: LIBS += -luser32 -lshell32 -lshcore
win32: DEFINES += NOMINMAX WIN32_LEAN_AND_MEAN
win32-msvc*: QMAKE_CXXFLAGS += /utf-8
OBJECTS_DIR = $$OUT_PWD/obj
MOC_DIR = $$OUT_PWD/moc
RCC_DIR = $$OUT_PWD/rcc
DESTDIR = $$OUT_PWD

# Offline help is an explicit dependency, so documentation-only edits are deployed too.
help_file.target = $$DESTDIR/docs/user-guide.html
help_file.depends = $$KMM_ROOT/docs/user-guide.html
help_file.commands = if not exist $$shell_quote($$shell_path($$DESTDIR/docs)) mkdir $$shell_quote($$shell_path($$DESTDIR/docs))
help_file.commands += $$escape_expand(\n\t) $$QMAKE_COPY $$shell_quote($$shell_path($$KMM_ROOT/docs/user-guide.html)) $$shell_quote($$shell_path($$help_file.target))
QMAKE_EXTRA_TARGETS += help_file
PRE_TARGETDEPS += $$help_file.target
