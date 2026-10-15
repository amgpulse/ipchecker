QT       += core gui network widgets

TARGET = ipchecker
TEMPLATE = app

CONFIG += c++17

# Single source of truth for the app version: CI reads it from this file.
VERSION = 1.2.0
DEFINES += APP_VERSION=\\\"$$VERSION\\\"
DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += main.cpp \
           mainwindow.cpp

HEADERS += mainwindow.h

RESOURCES += ipchecker.qrc

win32: RC_FILE = ipchecker.rc
macx: QMAKE_TARGET_BUNDLE_PREFIX = com.amgpulse
