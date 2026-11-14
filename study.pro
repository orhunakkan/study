QT += core gui qml quick quickcontrols2 dbus network webenginequick webchannel webchannelquick

CONFIG += c++20 release
TARGET = study
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/bridge.h \
    src/filepicker.h \
    src/filetree.h \
    src/gitrepo.h \
    src/portalfilepicker.h \
    src/schemehandler.h \
    src/singleinstance.h \
    src/tabs.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/bridge.cpp \
    src/filetree.cpp \
    src/gitrepo.cpp \
    src/portalfilepicker.cpp \
    src/schemehandler.cpp \
    src/singleinstance.cpp \
    src/tabs.cpp \
    src/theme.cpp \
    src/translator.cpp

# The document page (built from web/ into src/web/dist by web/build).
RESOURCES += src/resources.qrc src/web/dist/web.qrc
