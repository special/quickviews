#pragma once

#include <QObject>
#include <QQuickItem>
#include <QMap>
#include <QQmlIncubator>
#include <QSharedPointer>
#include <QLoggingCategory>
#include <memory>

class QAbstractItemModel;

typedef std::shared_ptr<QQuickItem> DelegateRef;

class DelegateManager : public QObject
{
    Q_OBJECT

    friend class DelegateContextObject;

public:
    DelegateManager(QObject *parent = nullptr);
    virtual ~DelegateManager();

    void setModel(QAbstractItemModel *model);

    DelegateRef item(int index) const;
    DelegateRef createItem(int index, QQmlComponent *component, QQuickItem *parent, QQmlIncubator::IncubationMode mode);
    void release(int index) { release(index, index); }
    void release(int first, int last);
    void clear();

    void adjustIndex(int from, int delta);

private:
    QMap<int, std::weak_ptr<QQuickItem>> m_items;
    QAbstractItemModel *m_model = nullptr;
    QHash<int, int> m_rolePropertyMap;
    QSharedPointer<QMetaObject> m_dataMetaObject = nullptr;
    int m_recentlyReleased = 0;

    bool createMetaObject();
    void release(QQuickItem *item);
    void cleanup();

    DelegateContextObject *contextObject(QQuickItem *item);
};

Q_DECLARE_LOGGING_CATEGORY(lcDelegate)
