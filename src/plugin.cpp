#include "plugin.h"
#include "justifyview.h"
#include <QtQml>

void QuickViewsPlugin::registerTypes(const char *uri)
{
    // @uri Crimson.Views
    qmlRegisterType<JustifyView>(uri, 1, 0, "JustifyView");
}
