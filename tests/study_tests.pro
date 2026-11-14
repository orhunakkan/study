QT += core gui quick quickcontrols2 testlib dbus
CONFIG += c++20 testcase
TARGET = study_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/backend.h ../src/bridge.h ../src/filetree.h ../src/gitrepo.h ../src/tabs.h ../src/theme.h ../src/translator.h ../src/filepicker.h
SOURCES += study_tests.cpp ../src/backend.cpp ../src/bridge.cpp ../src/filetree.cpp ../src/gitrepo.cpp ../src/tabs.cpp ../src/theme.cpp ../src/translator.cpp
