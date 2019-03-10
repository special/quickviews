TEMPLATE = lib
TARGET = quickviews
TARGETPATH = Crimson/Views
IMPORT_VERSION = 1.0

CONFIG += qt
QT += qml quick qml-private quick-private

SOURCES += \
    src/plugin.cpp \
    src/justifyview.cpp \
    src/flexsection.cpp

HEADERS += \
    src/plugin.h \
    src/justifyview.h \
    src/justifyview_p.h \
    src/flexsection.h

load(qml_plugin)
