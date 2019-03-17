#include "plugin.h"
#include "flexview.h"
#include "flexsection.h"
#include <QtQml>

void QuickViewsPlugin::registerTypes(const char *uri)
{
    // @uri Crimson.Views
    qmlRegisterType<FlexView>(uri, 1, 0, "FlexView");
    qmlRegisterUncreatableType<FlexSection>(uri, 1, 0, "FlexSection", "attached type");
}
