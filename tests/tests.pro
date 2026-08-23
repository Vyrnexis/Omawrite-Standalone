QT += core gui quick testlib
CONFIG += testcase c++17 release
CONFIG -= debug
TEMPLATE = app
TARGET = tst_omawrite
DESTDIR = ../bin
gcc|clang: QMAKE_CXXFLAGS_RELEASE += -O3
msvc: QMAKE_CXXFLAGS_RELEASE += /O2
unix: QMAKE_LFLAGS_RELEASE += -s

INCLUDEPATH += ../src
SOURCES += \
    tst_omawrite.cpp \
    ../src/backend.cpp \
    ../src/markdownhighlighter.cpp \
    ../src/codehighlighter.cpp
HEADERS += \
    ../src/backend.h \
    ../src/markdownhighlighter.h \
    ../src/codehighlighter.h

QT += widgets printsupport quickcontrols2 quickdialogs2
