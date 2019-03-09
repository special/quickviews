#pragma once

#include "justifyview.h"
#include <QPointer>
#include <QtQml/private/qqmlobjectmodel_p.h>
#include <QtQml/private/qqmlchangeset_p.h>
#include <QtQml/private/qqmlguard_p.h>

class JustifyViewPrivate : public QObject
{
    Q_OBJECT

public:
    JustifyView * const q;

    QVariant modelVariant;
    QPointer<QQmlInstanceModel> model;
    bool ownModel = false;
    bool delegateValidated = false;

    QQmlChangeSet pendingChanges;

    JustifyViewPrivate(JustifyView *q);
    virtual ~JustifyViewPrivate();

    void layout();
    void clear();

    QQuickItem *createItem(int index);

public slots:
    void modelUpdated(const QQmlChangeSet &changes, bool reset);
    void initItem(int index, QObject *object);
    void createdItem(int index, QObject *object);
    void destroyingItem(QObject *object);
};
