#pragma once

#include "flexview.h"
#include <QPointer>
#include <QtQml/private/qqmlobjectmodel_p.h>
#include <QtQml/private/qqmlchangeset_p.h>
#include <QtQml/private/qqmlguard_p.h>

class FlexSection;

class FlexViewPrivate : public QObject
{
    Q_OBJECT

public:
    FlexView * const q;

    QVariant modelVariant;
    QPointer<QQmlInstanceModel> model;
    int modelSizeRole = -1;
    bool ownModel = false;
    bool delegateValidated = false;

    QQmlChangeSet pendingChanges;

    QQmlGuard<QQmlComponent> sectionDelegate;
    QList<FlexSection*> sections;
    QString sectionRole;
    QString sizeRole;

    qreal idealHeight = 0;
    qreal minHeight = 0;
    qreal maxHeight = 0;

    FlexViewPrivate(FlexView *q);
    virtual ~FlexViewPrivate();

    void layout();
    void updateContentHeight(qreal layoutHeight);
    bool applyPendingChanges();
    void validateSections();
    bool refill();
    void clear();

    QQuickItem *createItem(int index);

    QString sectionValue(int index) const;
    qreal indexFlexRatio(int index);

public slots:
    void modelUpdated(const QQmlChangeSet &changes, bool reset);
    void initItem(int index, QObject *object);
    void createdItem(int index, QObject *object);
    void destroyingItem(QObject *object);
};
