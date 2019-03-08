#pragma once

#include "justifyview.h"
#include <QtQml/private/qqmlguard_p.h>

class JustifyViewPrivate : public QObject
{
    Q_OBJECT

public:
    JustifyView * const q;

    QVariant modelVariant;
    QQmlGuard<QQmlComponent> delegate;

    JustifyViewPrivate(JustifyView *q);
    virtual ~JustifyViewPrivate();
};
