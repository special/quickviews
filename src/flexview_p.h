#pragma once

#include "flexview.h"
#include "delegatemanager.h"
#include <QPointer>
#include <QLoggingCategory>
#include <QtQml/private/qqmlchangeset_p.h>
#include <QtQml/private/qqmlguard_p.h>
#include <QtQuick/private/qquickitemchangelistener_p.h>

class FlexSection;

class FlexViewPrivate : public QObject, public QQuickItemChangeListener
{
    Q_OBJECT

public:
    FlexView * const q;

    QAbstractItemModel *model = nullptr;
    QQmlChangeSet pendingChanges;
    int moveId = 0;

    QQmlGuard<QQmlComponent> delegate;
    DelegateManager items;

    QQmlGuard<QQmlComponent> sectionDelegate;
    QList<FlexSection*> sections;
    QString sectionRole;
    int sectionRoleIdx = -1;
    QString sizeRole;
    int sizeRoleIdx = -1;

    qreal idealHeight = 0;
    qreal minHeight = 0;
    qreal maxHeight = 0;
    qreal cacheBuffer = 0;

    int currentIndex = -1;
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

    QString sectionValue(int index);
    qreal indexFlexRatio(int index);

    virtual void itemGeometryChanged(QQuickItem *item, QQuickGeometryChange change, const QRectF &oldGeometry) override;

    FlexSection *sectionOf(int index) const;

public slots:
    void rowsInserted(const QModelIndex &parent, int first, int last);
    void rowsRemoved(const QModelIndex &parent, int first, int last);
    void rowsMoved(const QModelIndex &parent, int start, int end, const QModelIndex &destination, int row);
    void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles);
    void layoutChanged();
    void modelReset();
};

Q_DECLARE_LOGGING_CATEGORY(lcView)
Q_DECLARE_LOGGING_CATEGORY(lcLayout)
