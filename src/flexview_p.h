#pragma once

#include "flexview.h"
#include <QPointer>
#include <QLoggingCategory>
#include <QtQml/private/qqmlobjectmodel_p.h>
#include <QtQml/private/qqmlchangeset_p.h>
#include <QtQml/private/qqmlguard_p.h>
#include <QtQuick/private/qquickitemchangelistener_p.h>

class FlexSection;

class FlexViewPrivate : public QObject, public QQuickItemChangeListener
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
    qreal cacheBuffer = 0;

    int currentIndex = -1;
    QQuickItem *currentItem = nullptr;
    QPointer<FlexSection> currentSection;
    qreal moveRowTargetX = -1;

    qreal vSpacing = 0;
    qreal hSpacing = 0;
    qreal sectionSpacing = 0;

    FlexViewPrivate(FlexView *q);
    virtual ~FlexViewPrivate();

    void layout();
    void updateContentHeight(qreal layoutHeight);
    bool applyPendingChanges();
    void validateSections();
    bool refill();
    void clear();

    int count() const;

    QQuickItem *createItem(int index);

    QString sectionValue(int index) const;
    qreal indexFlexRatio(int index);

    virtual void itemGeometryChanged(QQuickItem *item, QQuickGeometryChange change, const QRectF &oldGeometry) override;

    FlexSection *sectionOf(int index) const;

public slots:
    void modelUpdated(const QQmlChangeSet &changes, bool reset);
    void initItem(int index, QObject *object);
    void createdItem(int index, QObject *object);
    void destroyingItem(QObject *object);
};

Q_DECLARE_LOGGING_CATEGORY(lcView)
Q_DECLARE_LOGGING_CATEGORY(lcLayout)
Q_DECLARE_LOGGING_CATEGORY(lcDelegate)
