#pragma once

#include <QObject>
#include <QQuickItem>
#include <QMap>
#include <QQmlIncubator>
#include <QSharedPointer>
#include <QLoggingCategory>

class QAbstractItemModel;

class DelegateManager : public QObject
{
    Q_OBJECT

    friend class DelegateContextObject;

public:
    DelegateManager(QObject *parent = nullptr);
    virtual ~DelegateManager();

    void setModel(QAbstractItemModel *model);

    QQuickItem *item(int index) const;
    QQuickItem *createItem(int index, QQmlComponent *component, QQuickItem *parent, QQmlIncubator::IncubationMode mode);
    void release(int index) { release(index, index); }
    void release(int first, int last);
    void clear();

    void adjustIndex(int from, int delta);

    void hold(int index) { m_hold = index; }

private:
    QMap<int, QQuickItem*> m_items;
    QAbstractItemModel *m_model = nullptr;
    QHash<int, int> m_rolePropertyMap;
    QSharedPointer<QMetaObject> m_dataMetaObject = nullptr;
    int m_hold = -1;

    bool createMetaObject();
};

Q_DECLARE_LOGGING_CATEGORY(lcDelegate)
