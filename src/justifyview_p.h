#pragma once

#include "justifyview.h"
#include <QPointer>
#include <QtQml/private/qqmlobjectmodel_p.h>
#include <QtQml/private/qqmlchangeset_p.h>
#include <QtQml/private/qqmlguard_p.h>

class FlexSection;

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

    QList<FlexSection*> sections;
    QString sectionRole;

    JustifyViewPrivate(JustifyView *q);
    virtual ~JustifyViewPrivate();

    void layout();
    bool applyPendingChanges();
    void validateSections();
    void clear();

    QQuickItem *createItem(int index);

    QString sectionValue(int index) const;

public slots:
    void modelUpdated(const QQmlChangeSet &changes, bool reset);
    void initItem(int index, QObject *object);
    void createdItem(int index, QObject *object);
    void destroyingItem(QObject *object);
};
