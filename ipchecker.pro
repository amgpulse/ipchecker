QT       += core gui network widgets

TARGET = ipchecker
TEMPLATE = app

CONFIG += c++17

# Single source of truth for the app version: CI reads it from this file.
VERSION = 1.3.0
DEFINES += APP_VERSION=\\\"$$VERSION\\\"
DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += main.cpp \
           mainwindow.cpp \
           netutils.cpp

HEADERS += mainwindow.h \
           netutils.h

RESOURCES += ipchecker.qrc

win32: RC_FILE = ipchecker.rc
macx: QMAKE_TARGET_BUNDLE_PREFIX = com.amgpulse
