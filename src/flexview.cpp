#include "flexview_p.h"
#include "flexsection.h"
#include <QtQml>
#include <QQmlComponent>

Q_LOGGING_CATEGORY(lcView, "crimson.flexview")
Q_LOGGING_CATEGORY(lcLayout, "crimson.flexview.layout")

FlexView::FlexView(QQuickItem *parent)
    : QQuickFlickable(parent)
    , d(new FlexViewPrivate(this))
{
    setFlag(QQuickItem::ItemIsFocusScope);
}

FlexView::~FlexView()
{
}

void FlexView::componentComplete()
{
    QQuickFlickable::componentComplete();
    polish();

    if (d->currentIndex >= 0) {
        int index = d->currentIndex;
        d->currentIndex = -1;
        setCurrentIndex(index);
    }
}

QAbstractItemModel *FlexView::model() const
{
    return d->model;
}

void FlexView::setModel(QAbstractItemModel *model)
{
    if (d->model == model)
        return;

    d->clear();
    if (d->model)
        disconnect(d->model, nullptr, d, nullptr);

    d->model = model;
    if (d->model) {
        connect(d->model, &QAbstractItemModel::rowsInserted, d, &FlexViewPrivate::rowsInserted);
        connect(d->model, &QAbstractItemModel::rowsRemoved, d, &FlexViewPrivate::rowsRemoved);
        connect(d->model, &QAbstractItemModel::dataChanged, d, &FlexViewPrivate::dataChanged);
        connect(d->model, &QAbstractItemModel::rowsMoved, d, &FlexViewPrivate::rowsMoved);
        connect(d->model, &QAbstractItemModel::layoutChanged, d, &FlexViewPrivate::layoutChanged);
        connect(d->model, &QAbstractItemModel::modelReset, d, &FlexViewPrivate::modelReset);
    }
    d->items.setModel(model);

    polish();
    qCDebug(lcView) << "setModel" << d->model;
    emit modelChanged();
}

QQmlComponent *FlexView::delegate() const
{
    return d->delegate;
}

void FlexView::setDelegate(QQmlComponent *delegate)
{
    if (delegate == d->delegate)
        return;

    d->delegate = delegate;
    d->clear();
    polish();

    qCDebug(lcView) << "setDelegate" << delegate;
    emit delegateChanged();
}

QQmlComponent *FlexView::section() const
{
    return d->sectionDelegate;
}

void FlexView::setSection(QQmlComponent *delegate)
{
    if (delegate == d->sectionDelegate)
        return;

    d->sectionDelegate = delegate;
    d->clear();
    polish();

    qCDebug(lcView) << "setSection" << delegate;
    emit sectionChanged();
}

QString FlexView::sectionRole() const
{
    return d->sectionRole;
}

void FlexView::setSectionRole(const QString &role)
{
    if (d->sectionRole == role)
        return;
    d->sectionRole = role;
    d->clear();
    polish();

    qCDebug(lcView) << "setSectionRole" << role;
    emit sectionRoleChanged();
}

QString FlexView::sizeRole() const
{
    return d->sizeRole;
}

void FlexView::setSizeRole(const QString &role)
{
    if (d->sizeRole == role)
        return;
    d->sizeRole = role;
    d->clear();
    polish();

    qCDebug(lcView) << "setSizeRole" << role;
    emit sizeRoleChanged();
}

qreal FlexView::idealHeight() const
{
    return d->idealHeight;
}

void FlexView::setIdealHeight(qreal height)
{
    if (d->idealHeight == height)
        return;
    d->idealHeight = height;
    polish();
    emit idealHeightChanged();
}

qreal FlexView::minHeight() const
{
    return d->minHeight;
}

void FlexView::setMinHeight(qreal height)
{
    if (d->minHeight == height)
        return;
    d->minHeight = height;
    polish();
    emit minHeightChanged();
}

qreal FlexView::maxHeight() const
{
    return d->maxHeight;
}

void FlexView::setMaxHeight(qreal height)
{
    if (d->maxHeight == height)
        return;
    d->maxHeight = height;
    polish();
    emit maxHeightChanged();
}

qreal FlexView::cacheBuffer() const
{
    return d->cacheBuffer;
}

void FlexView::setCacheBuffer(qreal cacheBuffer)
{
    cacheBuffer = std::max(cacheBuffer, 0.);
    if (cacheBuffer == d->cacheBuffer)
        return;
    d->cacheBuffer = cacheBuffer;
    emit cacheBufferChanged();
    polish();
}

qreal FlexView::verticalSpacing() const
{
    return d->vSpacing;
}

void FlexView::setVerticalSpacing(qreal spacing)
{
    if (d->vSpacing == spacing)
        return;

    d->vSpacing = spacing;
    polish();
    emit verticalSpacingChanged();
}

qreal FlexView::horizontalSpacing() const
{
    return d->hSpacing;
}

void FlexView::setHorizontalSpacing(qreal spacing)
{
    if (d->hSpacing == spacing)
        return;

    d->hSpacing = spacing;
    polish();
    emit horizontalSpacingChanged();
}

qreal FlexView::sectionSpacing() const
{
    return d->sectionSpacing;
}

void FlexView::setSectionSpacing(qreal spacing)
{
    if (d->sectionSpacing == spacing)
        return;

    d->sectionSpacing = spacing;
    polish();
    emit sectionSpacingChanged();
}

int FlexView::currentIndex() const
{
    return d->currentIndex;
}

QQuickItem *FlexView::currentItem() const
{
    // currentItem can't (reliably) be returned before component is complete,
    // because delegate/sectionDelegate/etc may not have been set yet.
    return d->currentSection ? d->currentSection->currentItem() : nullptr;
}

QQuickItem *FlexView::currentSection() const
{
    return d->currentSection ? d->currentSection->ensureItem()->item() : nullptr;
}

void FlexView::setCurrentIndex(int index)
{
    if (!isComponentComplete()) {
        d->currentIndex = index;
        return;
    }

    if (index >= 0)
        d->applyPendingChanges();

    index = std::min(index, d->count() - 1);
    index = std::max(index, -1);
    if (index == d->currentIndex)
        return;

    qCDebug(lcView) << "change currentIndex" << d->currentIndex << "to" << index;

    QPointer<FlexSection> oldSection(d->currentSection);

    d->currentIndex = index;
    d->currentSection = nullptr;
    d->moveRowTargetX = -1;

    if (index >= 0) {
        d->currentSection = d->sectionOf(index);
        if (oldSection && d->currentSection != oldSection)
            oldSection->setCurrentIndex(-1);
        if (d->currentSection)
            d->currentSection->setCurrentIndex(d->currentSection->mapToSection(index));
    } else if (oldSection)
        oldSection->setCurrentIndex(-1);

    // Can't allow layout to recurse, so if setCurrentIndex is called during layout it
    // will just schedule another one. That can lead to currentItem/currentSection being
    // temporarily null.
    if (!d->inLayout)
        d->layout();
    else
        polish();

    emit currentIndexChanged();
    emit currentItemChanged();
    if (d->currentSection != oldSection || !d->currentSection)
        emit currentSectionChanged();
}

bool FlexView::moveCurrentRow(int delta)
{
    if (delta == 0 || !isComponentComplete())
        return false;

    d->layout();

    FlexSection *section = d->currentSection;
    int sectionIndex = -1;
    int row = -1;
    qreal xTarget = d->moveRowTargetX;

    if (section) {
        sectionIndex = d->sections.indexOf(section);
        Q_ASSERT(sectionIndex >= 0);
        int i = section->mapToSection(d->currentIndex);
        Q_ASSERT(i >= 0);
        row = section->rowForIndex(i);
        Q_ASSERT(row >= 0);
        if (xTarget < 0)
            xTarget = section->geometryOf(i).center().x();
    } else if (delta > 0 && !d->sections.isEmpty()) {
        sectionIndex = 0;
        section = d->sections[0];
    } else {
        return false;
    }
    row += delta;

    for (;;) {
        Q_ASSERT(section->rowCount() > 0);
        if (row >= section->rowCount()) {
            if (sectionIndex >= d->sections.size() - 1) {
                row = section->rowCount() - 1;
                break;
            }
            row -= section->rowCount();
            sectionIndex++;
            section = d->sections[sectionIndex];
        } else if (row < 0) {
            if (sectionIndex < 1) {
                row = 0;
                break;
            }
            sectionIndex--;
            section = d->sections[sectionIndex];
            row += section->rowCount();
        } else {
            break;
        }
    }
    Q_ASSERT(row >= 0 && row < section->rowCount());

    int i = section->mapToView(section->rowIndexAt(row, xTarget, true));
    Q_ASSERT(i >= 0);
    if (i < 0 || i == d->currentIndex)
        return false;
    setCurrentIndex(i);
    d->moveRowTargetX = xTarget;
    return true;
}

void FlexView::updatePolish()
{
    QQuickFlickable::updatePolish();

    if (d->idealHeight > 0 && d->minHeight < 1)
        setMinHeight(d->idealHeight * 0.75);
    if (d->idealHeight > 0 && d->maxHeight < 1)
        setMaxHeight(d->idealHeight * 1.25);

    d->layout();
}

void FlexView::geometryChanged(const QRectF &newRect, const QRectF &oldRect)
{
    qCDebug(lcView) << "geometryChanged" << newRect << oldRect;
    QQuickFlickable::geometryChanged(newRect, oldRect);
    if (newRect.size() != oldRect.size())
        polish();
}

void FlexView::viewportMoved(Qt::Orientations orient)
{
    QQuickFlickable::viewportMoved(orient);
    polish();
}

FlexViewPrivate::FlexViewPrivate(FlexView *q)
    : QObject(q)
    , q(q)
{
}

FlexViewPrivate::~FlexViewPrivate()
{
    clear();
}

int FlexViewPrivate::count() const
{
    return model ? model->rowCount() : 0;
}

// Clear all state, but not properties
void FlexViewPrivate::clear()
{
    if (!q->isComponentComplete())
        return;

    qCDebug(lcView) << "view cleared";
    pendingChanges.clear();
    moveId = -1;
    items.clear();
    for (FlexSection *section : sections)
        section->deleteLater();
    sections.clear();
    sectionRoleIdx = -1;
    sizeRoleIdx = -1;
    // currentIndex goes to a state as if it had been set when the section didn't exist
    currentIndex = -1;
    currentSection = nullptr;
    moveRowTargetX = -1;
}

void FlexViewPrivate::rowsInserted(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent);
    pendingChanges.insert(first, last-first+1);
    q->polish();
}

void FlexViewPrivate::rowsRemoved(const QModelIndex &parent, int first, int last)
{
    Q_UNUSED(parent);
    pendingChanges.remove(first, last-first+1);
    q->polish();
}

void FlexViewPrivate::rowsMoved(const QModelIndex &parent, int start, int end, const QModelIndex &destination, int row)
{
    Q_UNUSED(parent);
    // XXX
    qFatal("rowsMoved not implemented");
}

void FlexViewPrivate::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    Q_UNUSED(roles);
    int count = bottomRight.row() - topLeft.row() + 1;
    pendingChanges.change(topLeft.row(), count);
    q->polish();

    for (int i = topLeft.row(); i <= bottomRight.row(); i++) {
        items.dataChanged(i, roles);
    }
}

void FlexViewPrivate::layoutChanged()
{
    // XXX
    modelReset();
}

void FlexViewPrivate::modelReset()
{
    qCDebug(lcView) << "model reset";
    clear();
    q->polish();
}

void FlexViewPrivate::layout()
{
    if (!q->isComponentComplete())
        return;

    if (inLayout) {
        qCWarning(lcLayout) << "FlexView cannot run layout recursively";
        return;
    }
    QScopedValueRollback guard(inLayout, true);

    applyPendingChanges();
    if (lcLayout().isDebugEnabled())
        validateSections();

    QRectF visibleArea(q->contentX(), q->contentY(), q->width(), q->height());
    QRectF cacheArea(visibleArea.adjusted(0, -cacheBuffer, 0, cacheBuffer));
    qreal viewportWidth = q->width(); // XXX contentWidth?
    qCDebug(lcLayout) << "layout area" << visibleArea << "viewportWidth" << viewportWidth << "current" << currentIndex;

    qreal x = 0, y = 0;
    int lastIndex = -1;
    for (int s = 0; ; s++) {
        if (s > 0)
            y += sectionSpacing;

        if (s >= sections.size()) {
            if ((y > cacheArea.bottom() && lastIndex >= currentIndex) || !refill())
                break;
        }

        FlexSection *section = sections[s];
        section->setViewportWidth(viewportWidth);
        section->setSpacing(hSpacing, vSpacing);
        section->setIdealHeight(minHeight, idealHeight, maxHeight);
        section->layout();

        qreal height = section->estimatedHeight();
        Q_ASSERT(height > 0);
        if (!cacheArea.intersects(QRectF(x, y, viewportWidth, height)) && section != currentSection) {
            qCDebug(lcLayout) << "section" << s << "y" << y << "estimatedHeight" << height << "not visible";
            section->releaseSectionDelegate();
            y += height;
            continue;
        }

        FlexSectionItem *sectionItem = section->ensureItem();
        if (!sectionItem)
            return;
        sectionItem->item()->setPosition(QPointF(x, y));
        sectionItem->item()->setImplicitWidth(viewportWidth);
        sectionItem->item()->setImplicitHeight(section->contentHeight());

        QRectF sectionVisibleArea = sectionItem->contentItem()->mapRectFromItem(q->contentItem(), visibleArea);
        QRectF sectionCacheArea = sectionItem->contentItem()->mapRectFromItem(q->contentItem(), cacheArea);
        section->layoutDelegates(sectionVisibleArea, sectionCacheArea);

        y += sectionItem->item()->height();
    }

    updateContentHeight(y);
}

void FlexViewPrivate::updateContentHeight(qreal layoutHeight)
{
    if (sections.isEmpty()) {
        q->setContentHeight(layoutHeight);
        return;
    }

    int lastIndex = sections.last()->mapToView(sections.last()->count-1);
    int remaining = count() - lastIndex - 1;
    qreal estimated = 0;

    if (layoutHeight > 0 && lastIndex > 0 && remaining > 0) {
        // Average the height-per-item for the remaining rows
        estimated = (layoutHeight / lastIndex) * remaining;
        qCDebug(lcLayout) << "contentHeight:" << (layoutHeight + estimated) << "with"
            << layoutHeight << "for existing" << lastIndex << "items, estimated"
            << estimated << "for remaining" << remaining << "items";
    } else if (q->contentHeight() != layoutHeight) {
        qCDebug(lcLayout) << "contentHeight:" << q->contentHeight() << "->" << layoutHeight + estimated;
    }

    q->setContentHeight(layoutHeight + estimated);
}

bool FlexViewPrivate::applyPendingChanges()
{
    if (pendingChanges.isEmpty() || !q->isComponentComplete())
        return false;

    int oldCurrentIndex = currentIndex;
    QPointer<FlexSection> oldCurrentSection(currentSection);

    for (const auto &remove : pendingChanges.removes()) {
        int first = remove.start();
        int count = remove.count;
        items.adjustIndex(first, -count);

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

        if (currentIndex >= first) {
            if (currentIndex < first + count)
                currentIndex = -1;
            else
                currentIndex -= count;
        }
    }

    for (const auto &insert : pendingChanges.inserts()) {
        int index = insert.start();
        int count = insert.count;

        items.adjustIndex(index, count);

        for (int s = 0; s < sections.size(); s++) {
            FlexSection *section = sections[s];
            if (section->viewStart >= index + count) {
                section->viewStart += count;
                continue;
            } else if (section->viewStart < index) {
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

        if (currentIndex >= index)
            currentIndex += count;
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
            section->deleteLater();
            sections.removeAt(s);
            s--;
            continue;
        }

        if (s > 0 && section->value == sections[s-1]->value) {
            FlexSection *prev = sections[s-1];
            prev->insert(prev->count, section->count);
            section->deleteLater();
            sections.removeAt(s);
            s--;
            continue;
        }
    }

    pendingChanges.clear();

    if (currentIndex < 0 && oldCurrentIndex >= 0) {
        // setCurrentIndex clears the current item as well
        currentIndex = oldCurrentIndex;
        q->setCurrentIndex(-1);
    } else {
        currentSection = (currentIndex >= 0) ? sectionOf(currentIndex) : nullptr;
        if (oldCurrentSection && currentSection != oldCurrentSection)
            oldCurrentSection->setCurrentIndex(-1);
        if (currentSection) {
            int sectionIndex = currentSection->mapToSection(currentIndex);
            Q_ASSERT(sectionIndex >= 0);
            currentSection->setCurrentIndex(sectionIndex);
        }

        if (currentIndex != oldCurrentIndex)
            emit q->currentIndexChanged();
        if (currentSection != oldCurrentSection || !currentSection)
            emit q->currentSectionChanged();
    }

    return true;
}

void FlexViewPrivate::validateSections()
{
    int modelCount = count();
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

bool FlexViewPrivate::refill()
{
    FlexSection *section = sections.isEmpty() ? nullptr : sections.last();
    int lastIndex = section ? section->mapToView(section->count - 1) : -1;

    bool sectionAdded = false;
    int modelCount = count();
    for (int i = lastIndex+1; i < modelCount; i++) {
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
        if (i == currentIndex)
            section->setCurrentIndex(section->mapToSection(i));
    }

    return sectionAdded;
}

QString FlexViewPrivate::sectionValue(int index)
{
    if (!model || sectionRole.isEmpty() || sectionRoleIdx < -1)
        return QString();

    if (sectionRoleIdx < 0) {
        sectionRoleIdx = model->roleNames().key(sectionRole.toLatin1(), -2);
        if (sectionRoleIdx < 0) {
            qCWarning(lcView) << "Model does not contain role" << sectionRole << "for sections";
            return QString();
        }
    }

    return model->data(model->index(index, 0), sectionRoleIdx).toString();
}

qreal FlexViewPrivate::indexFlexRatio(int index)
{
    if (!model || sizeRole.isEmpty() || sizeRoleIdx < -1)
        return 1;

    if (sizeRoleIdx < 0) {
        sizeRoleIdx = model->roleNames().key(sizeRole.toLatin1(), -2);
        if (sizeRoleIdx < 0) {
            qCWarning(lcView) << "Model does not contain role" << sizeRole << "for sizes";
            return 1;
        }
    }

    QVariant value = model->data(model->index(index, 0), sizeRoleIdx);
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

void FlexViewPrivate::itemGeometryChanged(QQuickItem *item, QQuickGeometryChange change, const QRectF &)
{
    if (!change.heightChange())
        return;

    qCDebug(lcLayout) << "section item geometry changed" << item;
    Q_UNUSED(item);
    q->polish();
}

FlexSection *FlexViewPrivate::sectionOf(int index) const
{
    auto it = std::lower_bound(sections.begin(), sections.end(), index, [](FlexSection *s, int i) { return s->mapToView(s->count-1) < i; });
    if (it == sections.end())
        return nullptr;
    Q_ASSERT((*it)->mapToSection(index) >= 0);
    return *it;
}

