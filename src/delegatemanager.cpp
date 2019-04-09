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
    DelegateContextObject(DelegateManager *mgr, QMetaObject *mo, int index)
        : m_metaObject(mo)
        , m_mgr(mgr)
        , m_index(index)
    {
    };

    virtual const QMetaObject *metaObject() const override
    {
        return m_metaObject;
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

            if (lcDelegate().isDebugEnabled()) {
                QMetaProperty prop = m_metaObject->property(offsetId);
                qCDebug(lcDelegate) << "context object for" << m_index << "read" << id << count << offsetId << prop.name() << "role" << role;
            }

            if (role >= 0) {
                QAbstractItemModel *model = m_mgr->m_model;
                QVariant value = model->data(model->index(m_index, 0), role);
                argv[0] = QMetaType::construct(QMetaType::QVariant, argv[0], reinterpret_cast<const void*>(&value));
            }

            id -= count;
        }

        return id;
    }

private:
    QMetaObject *m_metaObject;
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

QMetaObject *DelegateManager::dataMetaObject()
{
    if (!m_model || m_dataMetaObject)
        return m_dataMetaObject;
    auto roles = m_model->roleNames();
    Q_ASSERT(m_rolePropertyMap.isEmpty());

    QMetaObjectBuilder b;
    for (auto it = roles.constBegin(); it != roles.constEnd(); it++) {
        auto signal = b.addSignal(it.value() + "Changed()");
        auto prop = b.addProperty(it.value(), "QVariant", signal.index());
        prop.setWritable(false);
        m_rolePropertyMap.insert(prop.index(), it.key());
    }
    m_dataMetaObject = b.toMetaObject();

    if (lcDelegate().isDebugEnabled()) {
        QString props;
        for (int i = m_dataMetaObject->propertyOffset(); i < m_dataMetaObject->propertyCount(); i++) {
            props += m_dataMetaObject->property(i).name();
            props += " ";
        }
    }

    return m_dataMetaObject;
}

QQuickItem *DelegateManager::item(int index) const
{
    return m_items.value(index);
}

QQuickItem *DelegateManager::createItem(int index, QQmlComponent *component, QQuickItem *parent, QQmlIncubator::IncubationMode mode)
{
    auto it = m_items.lowerBound(index);
    if (it != m_items.end() && it.key() == index)
        return *it;

    // XXX Incubation

    // XXX If a delegate moves between sections, this context isn't "correct", for whatever that means
    QQmlContext *context = new QQmlContext(qmlContext(parent));
    DelegateContextObject *ctxObject = new DelegateContextObject(this, dataMetaObject(), index);
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
    m_items.insert(it, index, item);
    return item;
}

void DelegateManager::release(int first, int last)
{
    int released = 0;
    auto it = m_items.begin();
    if (first > 0)
        it = m_items.lowerBound(first);
    while (it != m_items.end()) {
        if (last >= 0 && it.key() > last)
            break;

        if (it.key() == m_hold)
            continue;

        // XXX delay release by a tick; be careful of how model remove interacts with that
        (*it)->setVisible(false);
        (*it)->deleteLater();

        it = m_items.erase(it);
        released++;
    }

    if (released)
        qCDebug(lcDelegate) << "released" << released << "delegates between" << first << "and" << last;
}

// XXX release+adjust to remove needs to clear current item _first_
void DelegateManager::adjustIndex(int from, int delta)
{
    if (m_hold >= from)
        m_hold += delta;

    auto it = m_items.constEnd();
    if (m_items.isEmpty() || (it-1).key() < from)
        return;

    // QMap claims that inserting the largest item first is more efficient
    QMap<int, QQuickItem*> adjusted;
    do {
        it--;
        auto key = it.key();
        if (key >= from)
            key += delta;
        adjusted.insert(adjusted.constBegin(), key, it.value());
    } while (it != m_items.constBegin());
    Q_ASSERT(m_items.size() == adjusted.size());

    m_items = adjusted;
}

void DelegateManager::clear()
{
    qCDebug(lcDelegate) << "clearing delegate manager and releasing" << m_items.size() << "delegates";
    for (QQuickItem *item : m_items) {
        item->setVisible(false);
        item->deleteLater();
    }
    m_items.clear();
    m_hold = -1;
    m_rolePropertyMap.clear();
    // XXX UNSAFE! This can't happen until after all of the deleteLater() calls above have finished...
    if (m_dataMetaObject) {
        free(m_dataMetaObject);
        m_dataMetaObject = nullptr;
    }
}
