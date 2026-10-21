QT += core network testlib
QT -= gui

TEMPLATE = app
TARGET = tst_netutils
CONFIG += console testcase c++17
CONFIG -= app_bundle

INCLUDEPATH += ..

SOURCES += tst_netutils.cpp \
           ../netutils.cpp

HEADERS += ../netutils.h
