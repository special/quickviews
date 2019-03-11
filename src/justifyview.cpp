#include "justifyview_p.h"
#include "flexsection.h"
#include <QtQml>
#include <QLoggingCategory>
#include <QQmlComponent>
#include <QtQml/private/qqmldelegatemodel_p.h>

#include <QRandomGenerator>

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

QString JustifyView::sectionRole() const
{
    return d->sectionRole;
}

void JustifyView::setSectionRole(const QString &role)
{
    if (d->sectionRole == role)
        return;
    d->sectionRole = role;
    d->clear();
    polish();
    emit sectionRoleChanged();
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
    modelSizeRole = -1;
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

    applyPendingChanges();
    validateSections();

    QRectF visibleArea(q->contentX(), q->contentY(), q->width(), q->height());
    qCDebug(lcLayout) << "layout area" << visibleArea;

    qreal x = 0, y = 0;
    for (int s = 0; ; s++) {
        if (s >= sections.size()) {
            if (!refill())
                break;
        }

        FlexSection *section = sections[s];
        section->layout();

        if (!visibleArea.intersects(QRectF(x, y, visibleArea.width()-x, section->height))) {
            qCDebug(lcLayout) << "section" << s << "y" << y << "h" << section->height << "not visible";
            if (y > visibleArea.bottom())
                break;
            y += section->height;
            continue;
        }

        section->layoutDelegates(x, y);
    }
}

bool JustifyViewPrivate::applyPendingChanges()
{
    if (pendingChanges.isEmpty())
        return false;

    for (const auto &remove : pendingChanges.removes()) {
        int first = remove.start();
        int count = remove.count;

        for (FlexSection *section : sections) {
            if (!count) {
                section->viewStart -= remove.count;
                continue;
            }

            int sectionFirst = section->mapToSection(first);
            if (sectionFirst < 0)
                continue;
            int sectionCount = std::min(count, section->count - sectionFirst);

            section->remove(sectionFirst, sectionCount);
            section->viewStart -= first - remove.start();

            first += sectionCount;
            count -= sectionCount;
        }
    }

    for (const auto &insert : pendingChanges.inserts()) {
        int index = insert.start();
        int count = insert.count;

        for (int s = 0; s < sections.size(); s++) {
            FlexSection *section = sections[s];
            if (section->viewStart > index + count) {
                section->viewStart += count;
                continue;
            } else if (index < section->viewStart) {
                continue;
            }

            bool createdSection = false;
            int from = 0;
            for (int i = 0; i < count; i++) {
                QString value = sectionValue(index + i);
                if (value == section->value)
                    continue;

                if (!createdSection && index != section->viewStart + section->count) {
                    // Split original section before inserting
                    FlexSection *suffix = new FlexSection(this, section->value);
                    // Suffix has viewStart _before_ the insert, so it offsets correctly after
                    suffix->viewStart = index;
                    int splitFirst = index - section->viewStart;
                    int splitCount = section->count - splitFirst;
                    suffix->insert(0, splitCount);
                    sections.insert(s + 1, suffix);
                    section->remove(splitFirst, splitCount);
                }
                createdSection = true;

                // Always appending at this point
                if (i > from)
                    section->insert(section->count, i - from);
                from = i;

                s++;
                section = new FlexSection(this, value);
                section->viewStart = index + from;
                sections.insert(s, section);
            }
            qCDebug(lcView) << "inserting to section" << section << "at" << section->viewStart << "from" << index - section->viewStart << "count" << count - from;

            if (createdSection)
                section->insert(0, count - from);
            else
                section->insert(index - section->viewStart, count - from);
        }
    }

    for (const auto &change : pendingChanges.changes()) {
        int first = change.start();
        int count = change.count;

        for (int s = 0; s < sections.size(); s++) {
            FlexSection *section = sections[s];
            int sectionFirst = section->mapToSection(first);
            if (sectionFirst < 0)
                continue;
            int sectionCount = std::min(count, section->count - sectionFirst);

            for (int i = 0; i < sectionCount; i++) {
                QString value = sectionValue(section->mapToView(sectionFirst + i));
                if (value == section->value)
                    continue;

                section->remove(sectionFirst + i, sectionCount - i);

                FlexSection *newSection = new FlexSection(this, value);
                newSection->viewStart = section->mapToView(sectionFirst + i);
                newSection->insert(0, sectionCount - i);
                sections.insert(s+1, newSection);

                sectionCount = i;
                break;
            }

            if (sectionCount > 0) {
                section->change(sectionFirst, sectionCount);
                first += sectionCount;
                count -= sectionCount;
            }

            if (!count)
                break;
        }
    }

    for (int s = 0; s < sections.size(); s++) {
        FlexSection *section = sections[s];
        if (section->count == 0) {
            delete section;
            sections.removeAt(s);
            s--;
            continue;
        }

        if (s > 0 && section->value == sections[s-1]->value) {
            FlexSection *prev = sections[s-1];
            prev->insert(prev->count, section->count);
            delete section;
            sections.removeAt(s);
            s--;
            continue;
        }
    }

    pendingChanges.clear();
    return true;
}

// XXX disable except for debugging
void JustifyViewPrivate::validateSections()
{
    int modelCount = model->count();
    FlexSection *prevSection = nullptr;
    for (int s = 0; s < sections.size(); s++) {
        FlexSection *section = sections[s];
        if ((prevSection && prevSection->viewStart + prevSection->count != section->viewStart) || (!prevSection && section->viewStart > 0)) {
            qCWarning(lcLayout) << "section" << s << "viewStart" << section->viewStart << "expected" << (prevSection ? prevSection->viewStart + prevSection->count : 0);
        }
        if (section->count < 1) {
            qCWarning(lcLayout) << "section" << s << "is empty";
        }
        if (section->viewStart + section->count > modelCount) {
            qCWarning(lcLayout) << "section" << s << "goes past model count" << modelCount << "with" << section->viewStart << section->count;
        }
        if (prevSection && prevSection->value == section->value) {
            qCWarning(lcLayout) << "section" << s << "should merge with previous section";
        }
        prevSection = section;
    }
}

bool JustifyViewPrivate::refill()
{
    FlexSection *section = sections.isEmpty() ? nullptr : sections.last();
    int lastIndex = section ? section->mapToView(section->count - 1) : -1;

    bool sectionAdded = false;
    for (int i = lastIndex+1; i < model->count(); i++) {
        QString value = sectionValue(i);
        if (!section || value != section->value) {
            if (sectionAdded)
                break;
            section = new FlexSection(this, value);
            section->viewStart = i;
            sections.append(section);
            sectionAdded = true;
        }
        section->insert(section->count, 1);
    }

    return sectionAdded;
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
    delegateValidated = true;
    return item;
}

QString JustifyViewPrivate::sectionValue(int index) const
{
    if (sectionRole.isEmpty())
        return QString();
    else
        return model->stringValue(index, sectionRole);
}

qreal JustifyViewPrivate::indexFlexRatio(int index)
{
    // XXX
    static QMap<int,double> fake;
    if (!fake.contains(index))
        fake.insert(index, QRandomGenerator::global()->generateDouble());
    return fake.value(index);

    QQmlDelegateModel *delegateModel = qobject_cast<QQmlDelegateModel*>(model);
    if (!model || sizeRole.isEmpty())
        return 1;

    // XXX delegate model only provides ::stringValue()...
    QAbstractItemModel *aim = qobject_cast<QAbstractItemModel*>(delegateModel->model().value<QObject*>());
    if (!aim) {
        qCWarning(lcView) << "Only AbstractItemModel is supported for sizeRole";
        return 1;
    }

    if (modelSizeRole < 0) {
        auto roleNames = aim->roleNames();
        for (auto it = roleNames.constBegin(); it != roleNames.constEnd(); it++) {
            if (it.value() == sizeRole) {
                modelSizeRole = it.key();
                break;
            }
        }
        if (modelSizeRole < 0) {
            qCWarning(lcView) << "No role '" << sizeRole << "' found in model for sizeRole";
            return 1;
        }
    }

    QVariant value = aim->data(delegateModel->modelIndex(index).value<QModelIndex>(), modelSizeRole);
    if (value.canConvert<QSizeF>()) {
        QSizeF sz = value.value<QSizeF>();
        if (!sz.isEmpty())
            return sz.width() / sz.height();
        else
            return 1;
    } else if (value.canConvert<qreal>()) {
        return value.value<qreal>();
    } else {
        qCWarning(lcView) << "Invalid value" << value << "for sizeRole on index" << index;
        return 1;
    }
}
