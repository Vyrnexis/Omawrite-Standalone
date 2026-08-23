QT += core gui widgets printsupport qml quick quickcontrols2 quickdialogs2
linux: QT += dbus

CONFIG += c++17 release
CONFIG -= debug
TARGET = omawrite
TEMPLATE = app
DESTDIR = bin
gcc|clang: QMAKE_CXXFLAGS_RELEASE += -O3
msvc: QMAKE_CXXFLAGS_RELEASE += /O2
unix: QMAKE_LFLAGS_RELEASE += -s

HEADERS += \
    src/backend.h \
    src/markdownhighlighter.h \
    src/systemtheme.h \
    src/codehighlighter.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/markdownhighlighter.cpp \
    src/systemtheme.cpp \
    src/codehighlighter.cpp

RESOURCES += src/resources.qrc
