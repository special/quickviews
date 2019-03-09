#include "justifyview_p.h"
#include <QtQml>
#include <QLoggingCategory>
#include <QQmlComponent>
#include <QtQml/private/qqmldelegatemodel_p.h>

Q_LOGGING_CATEGORY(lcView, "crimson.justifyview")
Q_LOGGING_CATEGORY(lcLayout, "crimson.justifyview.layout")
Q_LOGGING_CATEGORY(lcDelegate, "crimson.justifyview.delegate")

JustifyView::JustifyView(QQuickItem *parent)
    : QQuickFlickable(parent)
    , d(new JustifyViewPrivate(this))
{
}

JustifyView::~JustifyView()
{
}

void JustifyView::componentComplete()
{
    if (d->model && d->ownModel)
        static_cast<QQmlDelegateModel*>(d->model.data())->componentComplete();
    QQuickFlickable::componentComplete();
}

QVariant JustifyView::model() const
{
    return d->modelVariant;
}

void JustifyView::setModel(const QVariant &m)
{
    // Primarily based on QQuickItemView::setModel
    QVariant model = m;
    if (model.userType() == qMetaTypeId<QJSValue>())
        model = model.value<QJSValue>().toVariant();
    if (d->modelVariant == model)
        return;

    if (d->model) {
        disconnect(d->model, &QQmlInstanceModel::modelUpdated, d, &JustifyViewPrivate::modelUpdated);
        disconnect(d->model, &QQmlInstanceModel::initItem, d, &JustifyViewPrivate::initItem);
        disconnect(d->model, &QQmlInstanceModel::createdItem, d, &JustifyViewPrivate::createdItem);
        disconnect(d->model, &QQmlInstanceModel::destroyingItem, d, &JustifyViewPrivate::destroyingItem);
    }

    QQmlInstanceModel *oldModel = d->model;

    d->clear();
    d->model = nullptr;
    d->modelVariant = model;

    QObject *object = qvariant_cast<QObject*>(model);
    QQmlInstanceModel *vim = nullptr;
    if (object && (vim = qobject_cast<QQmlInstanceModel *>(object))) {
        if (d->ownModel) {
            delete oldModel;
            d->ownModel = false;
        }
        d->model = vim;
    } else {
        if (!d->ownModel) {
            d->model = new QQmlDelegateModel(qmlContext(this), this);
            d->ownModel = true;
            if (isComponentComplete())
                static_cast<QQmlDelegateModel *>(d->model.data())->componentComplete();
        } else {
            d->model = oldModel;
        }
        if (QQmlDelegateModel *dataModel = qobject_cast<QQmlDelegateModel*>(d->model))
            dataModel->setModel(model);
    }

    if (d->model) {
        connect(d->model, &QQmlInstanceModel::modelUpdated, d, &JustifyViewPrivate::modelUpdated);
        connect(d->model, &QQmlInstanceModel::initItem, d, &JustifyViewPrivate::initItem);
        connect(d->model, &QQmlInstanceModel::createdItem, d, &JustifyViewPrivate::createdItem);
        connect(d->model, &QQmlInstanceModel::destroyingItem, d, &JustifyViewPrivate::destroyingItem);
    }

    polish();
    qCDebug(lcView) << "setModel" << d->model << "count" << (d->model ? d->model->count() : 0);
    emit modelChanged();
}

QQmlComponent *JustifyView::delegate() const
{
    if (d->model) {
        QQmlDelegateModel *dataModel = qobject_cast<QQmlDelegateModel*>(d->model);
        return dataModel ? dataModel->delegate() : nullptr;
    }
    return nullptr;
}

void JustifyView::setDelegate(QQmlComponent *delegate)
{
    if (delegate == this->delegate())
        return;

    if (!d->ownModel) {
        d->model = new QQmlDelegateModel(qmlContext(this));
        d->ownModel = true;
        if (isComponentComplete())
            static_cast<QQmlDelegateModel*>(d->model.data())->componentComplete();
    }

    QQmlDelegateModel *dataModel = qobject_cast<QQmlDelegateModel*>(d->model);
    if (dataModel) {
        dataModel->setDelegate(delegate);
        d->clear();
    }

    qCDebug(lcDelegate) << "setDelegate" << delegate;
    emit delegateChanged();
}

void JustifyView::updatePolish()
{
    QQuickFlickable::updatePolish();
    qCDebug(lcView) << "updatePolish" << isComponentComplete();
    d->layout();
}

JustifyViewPrivate::JustifyViewPrivate(JustifyView *q)
    : QObject(q)
    , q(q)
{
}

JustifyViewPrivate::~JustifyViewPrivate()
{
}

void JustifyViewPrivate::clear()
{
    pendingChanges.clear();
    delegateValidated = false;
}

void JustifyViewPrivate::modelUpdated(const QQmlChangeSet &changes, bool reset)
{
    qCDebug(lcLayout) << "model updated" << changes << reset;
    if (reset) {
        pendingChanges.clear();
        q->cancelFlick();
        // ...
        q->polish();
        return;
    }

    pendingChanges.apply(changes);
    q->polish();
}

void JustifyViewPrivate::initItem(int index, QObject *object)
{
    QQuickItem *item = qmlobject_cast<QQuickItem*>(object);
    if (item) {
        item->setParentItem(q->contentItem());
        qCDebug(lcDelegate) << "init index" << index << item;
    }
}

void JustifyViewPrivate::createdItem(int index, QObject *object)
{
    QQuickItem *item = qmlobject_cast<QQuickItem*>(object);
    qCDebug(lcDelegate) << "created index" << index << item;
}

void JustifyViewPrivate::destroyingItem(QObject *object)
{
    QQuickItem *item = qmlobject_cast<QQuickItem*>(object);
    if (item) {
        qCDebug(lcDelegate) << "destroying" << item;
        item->setParentItem(nullptr);
    }
}

void JustifyViewPrivate::layout()
{
    if (!q->isComponentComplete())
        return;

    // XXX init model
    if (!pendingChanges.isEmpty()) {
        qCDebug(lcLayout) << "layout: apply changes" << pendingChanges;
        pendingChanges.clear();
    }

    QRectF visibleArea(q->contentX(), q->contentY(), q->width(), q->height());

    qCDebug(lcLayout) << "layout" << model->count() << "items in visible area" << visibleArea;

    qreal x = 0, y = 0;
    for (int i = 0; i < model->count(); i++) {
        // XXX visibleArea

        // XXX what if it already existed?
        QQuickItem *item = createItem(i);
        if (!item)
            break;

        item->setPosition(QPointF(x, y));
        y += item->height();
    }
}

QQuickItem *JustifyViewPrivate::createItem(int index)
{
    QObject *object = model->object(index, QQmlIncubator::AsynchronousIfNested);
    QQuickItem *item = qmlobject_cast<QQuickItem*>(object);
    if (!item) {
        if (!delegateValidated) {
            delegateValidated = true;
            qmlWarning(q) << "Delegate must be an Item";
        }

        if (object)
            model->release(object);
        return nullptr;
    }

    qCDebug(lcDelegate) << "created index" << index << item;
    return item;
}
