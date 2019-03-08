TEMPLATE = lib
TARGET = quickviews
TARGETPATH = Crimson/Views
IMPORT_VERSION = 1.0

CONFIG += qt
QT += qml quick qml-private quick-private

SOURCES += \
    src/plugin.cpp \
    src/justifyview.cpp

HEADERS += \
    src/plugin.h \
    src/justifyview.h \
    src/justifyview_p.h

load(qml_plugin)
