#include "plugin.h"
#include "flexview.h"
#include <QtQml>

void QuickViewsPlugin::registerTypes(const char *uri)
{
    // @uri Crimson.Views
    qmlRegisterType<FlexView>(uri, 1, 0, "FlexView");
}
