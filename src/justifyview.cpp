#include "justifyview_p.h"
#include <QQmlComponent>

JustifyView::JustifyView(QQuickItem *parent)
    : QQuickFlickable(parent)
    , d(new JustifyViewPrivate(this))
{
}

JustifyView::~JustifyView()
{
}

QVariant JustifyView::model() const
{
    return d->modelVariant;
}

void JustifyView::setModel(const QVariant &model)
{
    if (d->modelVariant == model)
        return;

    d->modelVariant = model;
    emit modelChanged();
}

QQmlComponent *JustifyView::delegate() const
{
    return d->delegate.data();
}

void JustifyView::setDelegate(QQmlComponent *delegate)
{
    if (d->delegate == delegate)
        return;

    d->delegate = delegate;
    emit delegateChanged();
}

void JustifyView::updatePolish()
{
    QQuickItem::updatePolish();
}

JustifyViewPrivate::JustifyViewPrivate(JustifyView *q)
    : QObject(q)
    , q(q)
{
}

JustifyViewPrivate::~JustifyViewPrivate()
{
}
