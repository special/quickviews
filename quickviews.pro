TEMPLATE = lib
TARGET = quickviews
TARGETPATH = Crimson/Views
IMPORT_VERSION = 1.0

CONFIG += qt
QT += qml quick qml-private quick-private qmlmodels-private

SOURCES += \
    src/plugin.cpp \
    src/flexview.cpp \
    src/flexsection.cpp \
    src/delegatemanager.cpp

HEADERS += \
    src/plugin.h \
    src/flexview.h \
    src/flexview_p.h \
    src/flexsection.h \
    src/delegatemanager.h

load(qml_plugin)
