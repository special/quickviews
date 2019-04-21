#include "delegatemanager.h"
#include <QQmlContext>
#include <QQmlComponent>
#include <QAbstractItemModel>
#include <QtQml/private/qqmlglobal_p.h>
#include <QtCore/private/qmetaobjectbuilder_p.h>

Q_LOGGING_CATEGORY(lcDelegate, "crimson.flexview.delegate")

class DelegateContextObject : public QObject
{
public:
    DelegateContextObject(DelegateManager *mgr, QSharedPointer<QMetaObject> mo, int index)
        : m_metaObject(mo)
        , m_mgr(mgr)
        , m_index(index)
    {
    };

    void setIndex(int index)
    {
        if (index == m_index)
            return;

        m_index = index;
        QMetaObject::activate(this, m_metaObject.get(), 0, nullptr);
    }

    virtual const QMetaObject *metaObject() const override
    {
        return m_metaObject.data();
    }

    virtual int qt_metacall(QMetaObject::Call c, int id, void **argv) override
    {
        id = QObject::qt_metacall(c, id, argv);
        if (id < 0)
            return id;

        if (c == QMetaObject::ReadProperty) {
            int count = m_metaObject->propertyCount() - m_metaObject->propertyOffset();
            int offsetId = m_metaObject->propertyOffset() + id;
            int role = m_mgr->m_rolePropertyMap.value(id, -1);

            if (role >= 0) {
                if (lcDelegate().isDebugEnabled()) {
                    QMetaProperty prop = m_metaObject->property(offsetId);
                    qCDebug(lcDelegate) << "context object for" << m_index << "read" << prop.name() << "for role" << role;
                }

                QAbstractItemModel *model = m_mgr->m_model;
                *reinterpret_cast<QVariant*>(argv[0]) = model->data(model->index(m_index, 0), role);
            } else if (id == 0) {
                *reinterpret_cast<int*>(argv[0]) = m_index;
            }

            id -= count;
        }

        return id;
    }

private:
    QSharedPointer<QMetaObject> m_metaObject;
    DelegateManager *m_mgr;
    int m_index;
};

DelegateManager::DelegateManager(QObject *parent)
    : QObject(parent)
{
}

DelegateManager::~DelegateManager()
{
    clear();
}

void DelegateManager::setModel(QAbstractItemModel *model)
{
    clear();
    m_model = model;
}

bool DelegateManager::createMetaObject()
{
    if (!m_model || m_dataMetaObject)
        return m_dataMetaObject;
    auto roles = m_model->roleNames();
    Q_ASSERT(m_rolePropertyMap.isEmpty());

    QMetaObjectBuilder b;
    {
        auto signal = b.addSignal("indexChanged()");
        Q_ASSERT(signal.index() == 0);
        auto prop = b.addProperty("index", "int", signal.index());
        prop.setWritable(false);
    }

    for (auto it = roles.constBegin(); it != roles.constEnd(); it++) {
        auto signal = b.addSignal(it.value() + "Changed()");
        auto prop = b.addProperty(it.value(), "QVariant", signal.index());
        prop.setWritable(false);
        m_rolePropertyMap.insert(prop.index(), it.key());
    }

    m_dataMetaObject.reset(b.toMetaObject(), &::free);
    return true;
}

DelegateRef DelegateManager::item(int index) const
{
    return m_items.value(index).lock();
}

DelegateRef DelegateManager::createItem(int index, QQmlComponent *component, QQuickItem *parent, QQmlIncubator::IncubationMode mode)
{
    auto it = m_items.lowerBound(index);
    if (it != m_items.end() && it.key() == index) {
        if (auto ref = it->lock())
            return ref;
    }

    // XXX Incubation

    // XXX If a delegate moves between sections, this context isn't "correct", for whatever that means
    if (!createMetaObject()) {
        qCWarning(lcDelegate) << "Cannot create meta object for model";
        return nullptr;
    }

    QQmlContext *context = new QQmlContext(qmlContext(parent));
    DelegateContextObject *ctxObject = new DelegateContextObject(this, m_dataMetaObject, index);
    context->setContextObject(ctxObject);
    context->setContextProperty("model", ctxObject);

    QObject *object = component->beginCreate(context);
    QQuickItem *item = qobject_cast<QQuickItem*>(object);
    if (!item) {
        qCWarning(lcDelegate) << "Delegate" << component << "must be a valid Item";
        if (object) {
            component->completeCreate();
            object->deleteLater();
        }
        context->deleteLater();
        return nullptr;
    }
    context->setParent(item);

    QQml_setParent_noEvent(item, parent);
    item->setParentItem(parent);

    qCDebug(lcDelegate) << "created delegate" << item << "for index" << index;
    component->completeCreate();

    auto ref = std::shared_ptr<QQuickItem>(item, [this](auto item) { release(item); });
    m_items.insert(it, index, ref);
    return ref;
}

void DelegateManager::release(QQuickItem *item)
{
    // Don't bother removing the item from m_items; the weak ref has the same
    // result, and it's cheaper to not be modifying all the time (especially in
    // this path). It's also a bit annoying to track back to an index from here.
    // It will be cleaned up eventually.

    // XXX delay the actual deletion slightly to prevent any delegate bouncing
    item->setVisible(false);
    item->deleteLater();
}

void DelegateManager::adjustIndex(int from, int delta)
{
    auto it = m_items.constEnd();
    if (m_items.isEmpty() || (it-1).key() < from)
        return;

    // QMap claims that inserting the largest item first is more efficient
    QMap<int, std::weak_ptr<QQuickItem>> adjusted;
    do {
        it--;
        int key = it.key();
        if (key < from) {
            adjusted.insert(adjusted.constBegin(), key, it.value());
            continue;
        } else if (delta < 0 && from - key < delta)
            continue;
        key += delta;

        auto item = it.value().lock();
        if (!item)
            continue;
        auto ctx = qmlContext(item.get());
        Q_ASSERT(ctx);
        if (ctx) {
            auto ctxObject = qobject_cast<DelegateContextObject*>(ctx->contextObject());
            Q_ASSERT(ctxObject);
            if (ctxObject)
                ctxObject->setIndex(key);
        }

        adjusted.insert(adjusted.constBegin(), key, item);
    } while (it != m_items.constBegin());

    m_items = adjusted;
}

void DelegateManager::clear()
{
    qCDebug(lcDelegate) << "clearing delegate manager and releasing" << m_items.size() << "delegates";
    m_items.clear();
    m_rolePropertyMap.clear();
    m_dataMetaObject.reset();
}
