TEMPLATE = lib
TARGET = quickviews
TARGETPATH = Crimson/Views
IMPORT_VERSION = 1.0

CONFIG += qt
QT += qml quick

SOURCES += \
    src/plugin.cpp

HEADERS += \
    src/plugin.h

load(qml_plugin)
