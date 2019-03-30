#include "flexsection.h"
#include <QtQuick/private/qquickitem_p.h>

// FlexSection contains a range of rows to be laid out as a discrete section. It manages
// layout geometry and delegates within its range.

Q_LOGGING_CATEGORY(lcSection, "crimson.flexview.section")
Q_LOGGING_CATEGORY(lcFlexLayout, "crimson.flexview.layout.flex", QtWarningMsg)

struct FlexRow
{
    int start;
    int end; // inclusive
    // Chosen previous row is between prevStart and start-1
    int prevStart;
    qreal ratio;
    qreal height;
    qreal cost;

    FlexRow() = default;
    FlexRow(int start)
        : start(start), end(-1), prevStart(-1), ratio(0), height(0), cost(0)
    {
    }
};

FlexSection::FlexSection(FlexViewPrivate *view, const QString &value)
    : QObject(view)
    , view(view)
    , value(value)
{
}

FlexSection::~FlexSection()
{
    clear();
}

void FlexSection::clear()
{
    releaseSectionDelegate();
    m_contentHeight = 0;
    m_lastSectionHeight = 0;
    m_lastSectionCount = 0;
    count = 0;
    currentIndex =- 1;
    layoutRows.clear();
    dirty = 0;
}

void FlexSection::insert(int i, int c)
{
    Q_ASSERT(i >= 0 && i <= count);
    count += c;
    dirty |= DirtyFlag::Indices;

    QMap<int, QQuickItem*> adjusted;
    for (auto it = delegates.lowerBound(i); it != delegates.end(); ) {
        adjusted.insert(it.key()+c, it.value());
        it = delegates.erase(it);
    }
    delegates.unite(adjusted);
}

void FlexSection::remove(int i, int c)
{
    Q_ASSERT(i >= 0 && i < count);
    Q_ASSERT(c >= 0);
    Q_ASSERT(i+c <= count);
    count -= c;
    dirty |= DirtyFlag::Indices;

    QMap<int, QQuickItem*> adjusted;
    for (auto it = delegates.lowerBound(i); it != delegates.end(); ) {
        int k = it.key()-c;
        if (k - i < 0) {
            (*it)->setVisible(false);
            view->model->release(*it);
        } else {
            adjusted.insert(k, *it);
        }
        it = delegates.erase(it);
    }
    delegates.unite(adjusted);
}

void FlexSection::change(int i, int c)
{
    Q_ASSERT(i >= 0 && i < count);
    Q_ASSERT(c >= 0);
    Q_ASSERT(i+c <= count);
    dirty |= DirtyFlag::Data;
}

bool FlexSection::setViewportWidth(qreal width)
{
    if (viewportWidth == width)
        return false;
    viewportWidth = width;
    dirty |= DirtyFlag::Geometry;
    return true;
}

bool FlexSection::setIdealHeight(qreal min, qreal ideal, qreal max)
{
    if (min == minHeight && ideal == idealHeight && max == maxHeight)
        return false;
    minHeight = min;
    idealHeight = ideal;
    maxHeight = max;
    dirty |= DirtyFlag::Geometry;
    return true;
}

void FlexSection::setCurrentIndex(int index)
{
    index = std::max(index, -1);
    Q_ASSERT(index < count);
    if (index == currentIndex || index >= count)
        return;

    bool hadCurrentIndex = currentIndex >= 0;
    currentIndex = index;
    if (currentIndex >= 0)
        ensureItem();

    if (m_sectionItem) {
        if (hadCurrentIndex != (currentIndex >= 0))
            m_sectionItem->isCurrentSectionChanged();
        m_sectionItem->currentItemChanged();
    }
}

// Return the actual section item height, if it exists.
// Otherwise, guess based on the last known heights
// If it has never existed, make a guess based on idealHeight
// In all cases, use at least 10 pixels.
//
// The exact values here are not as important; when the section
// item doesn't exist, they're primarily just for contentHeight
// scrolling estimates. It is important to have at least 1px for
// the visble/cache area to intersect.
qreal FlexSection::estimatedHeight() const
{
    qreal estimate = 0;
    if (m_sectionItem && m_sectionItem->item()) {
        estimate = m_sectionItem->item()->height();
    } else if (m_lastSectionHeight > 0 && m_lastSectionCount > 0) {
        int delta = count - m_lastSectionCount;
        estimate = m_lastSectionHeight + ((m_lastSectionHeight / m_lastSectionCount) * delta);
    } else if (idealHeight > 0 && count > 0 && viewportWidth > 0) {
        // Estimate items per row based on idealHeight, viewportWidth, and
        // an assumption that everything is 4:3
        int itemsPerRow = std::round(viewportWidth / (idealHeight * (4 / 3)));
        estimate = std::ceil(double(count) / itemsPerRow) * idealHeight;
    }

    return std::max(10., estimate);
}

bool FlexSection::layout()
{
    if (!dirty)
        return false;

    if (minHeight > idealHeight || maxHeight < idealHeight) {
        qCWarning(lcSection) << "Impossible layout constraints with min/ideal/max" << minHeight << idealHeight << maxHeight;
        idealHeight = -1;
    }

    layoutRows.clear();
    m_contentHeight = 0;
    if (viewportWidth < 1 || minHeight < 1 || idealHeight < 1 || maxHeight < 1) {
        dirty.setFlag(DirtyFlag::Geometry, false);
        return true;
    } else if (count < 1) {
        dirty.setFlag(DirtyFlag::Indices, false);
        return true;
    }

    QElapsedTimer tm;
    tm.restart();

    QVector<FlexRow> rows;
    QVector<FlexRow> openRows{FlexRow(0)};
    int nStartPositions = 0;

    qCDebug(lcFlexLayout) << "layout for section viewStart" << viewStart << "count" << count << "dirty" << dirty;

    for (int i = 0; i < count; i++) {
        qreal ratio = view->indexFlexRatio(mapToView(i));
        if (!ratio)
            ratio = 1;

        int rowsAdded = 0;
        Q_ASSERT(!openRows.isEmpty());
        for (int p = 0; p < openRows.size(); p++) {
            FlexRow &candidate = openRows[p];
            candidate.ratio += ratio;
            candidate.height = viewportWidth / candidate.ratio;

            if (candidate.height > maxHeight && i+1 < count)
                continue;
            if (candidate.height < minHeight && candidate.end >= 0) {
                // Below minimum height, and at least one candidate has been recorded with this start index
                qCDebug(lcFlexLayout) << ".... no more rows start at" << candidate.start << "because adding"
                    << i << "reduces height to" << candidate.height << "with minimum" << minHeight;
                openRows.removeAt(p);
                p--;
                continue;
            }

            candidate.end = i;
            FlexRow row = candidate;
            row.cost += badness(row);

            if (row.height > maxHeight) {
                // Set last partial row to idealHeight, but keep original badness
                row.height = idealHeight;
            } else if (row.height < minHeight) {
                // XXX if i > row.start, the candidate ending at i-1 was just as viable.
                // There was no way to know that at the time, and adding it now is complex.
                qCDebug(lcFlexLayout) << ".... row from" << row.start << "to" << i << "height" << row.height
                    << "is below minimum" << minHeight << "but no other rows can start from" << row.start;
                openRows.removeAt(p);
                p--;
            }

            rows.append(row);
            rowsAdded++;
            qCDebug(lcFlexLayout) << ".. row:" << row.start << "to" << row.end << "height" << row.height
                << "ratio" << row.ratio << "badness" << badness(row) << "cost" << row.cost;
        }

        if (rowsAdded && i+1 < count) {
            // Row(s) ended at index i, so add a row starting at i+1 to openRows
            FlexRow next(i+1);
            for (int j = rows.size() - rowsAdded; j < rows.size(); j++) {
                if (next.prevStart < 0 || rows[j].cost < next.cost) {
                    next.cost = rows[j].cost;
                    next.prevStart = rows[j].start;
                }
            }

            nStartPositions++;
            openRows.append(next);
            qCDebug(lcFlexLayout) << ".... starting row at" << next.start << "reachable by" << rowsAdded
                << "with cost" << next.cost << "for previous row" << next.prevStart << "to" << next.start-1;
        }
    }

    for (int i = rows.size()-1; i >= 0; i--) {
        const FlexRow &row = rows[i];
        if (row.end == count-1) {
            if (layoutRows.isEmpty())
                layoutRows.append(row);
            else if (row.cost < layoutRows[0].cost)
                layoutRows[0] = row;
            m_contentHeight = row.height;
        } else if (row.start == layoutRows[0].prevStart && row.end == layoutRows[0].start-1) {
            // XXX This isn't ideal; prepend is slow on vector
            layoutRows.prepend(row);
            m_contentHeight += row.height;
        }
    }
    Q_ASSERT(!layoutRows.isEmpty());
    Q_ASSERT(layoutRows[0].start == 0);

    if (lcFlexLayout().isDebugEnabled()) {
        qCDebug(lcFlexLayout) << ".. selected rows:";
        for (const auto &row : layoutRows) {
            qCDebug(lcFlexLayout) << "...." << row.start << "to" << row.end << "cost" << row.cost << "height" << row.height;
        }
    }

    qCDebug(lcLayout) << "section:" << layoutRows.size() << "rows for" << count << "items starting" << viewStart << "in" << m_contentHeight << "px; considered" << rows.size() << "rows from" << nStartPositions << "positions in" << tm.elapsed() << "ms";

    if (dirty & DirtyFlag::Indices && m_sectionItem)
        emit m_sectionItem->countChanged();

    dirty = 0;
    return true;
}

qreal FlexSection::badness(const FlexRow &row) const
{
    if (row.height < idealHeight) {
        return 1 - (row.height - minHeight) / (idealHeight - minHeight);
    } else if (row.height > idealHeight) {
        return 1 - (maxHeight - row.height) / (maxHeight - idealHeight);
    } else {
        return 0;
    }
}

void FlexSection::layoutDelegates(const QRectF &visibleArea, const QRectF &cacheArea)
{
    Q_ASSERT(!dirty);

    if (!ensureItem()) {
        qCWarning(lcDelegate) << "failed to create section delegate";
        return;
    }
    QQuickItem *contentItem = m_sectionItem->contentItem();
    if (!contentItem) {
        qCWarning(lcDelegate) << "failed to create section contentItem";
        return;
    }
    contentItem->setSize(QSizeF(viewportWidth, m_contentHeight));
    m_lastSectionHeight = m_sectionItem->item()->height();
    m_lastSectionCount = count;

    qreal y = 0;
    int currentIndex = mapToSection(view->currentIndex);

    auto row = layoutRows.constBegin();
    for (; row != layoutRows.constEnd(); row++) {
        if (y + row->height >= cacheArea.top())
            break;
        if (currentIndex >= row->start && currentIndex <= row->end)
            layoutRow(*row, y, false);
        y += row->height;
    }

    if (row == layoutRows.constEnd()) {
        releaseDelegates();
        return;
    }
    if (row->start > 0)
        releaseDelegates(0, row->start - 1);

    for (; row != layoutRows.constEnd(); row++) {
        if (y > cacheArea.bottom())
            break;

        layoutRow(*row, y);
        y += row->height;
        Q_ASSERT(y+row->height <= m_contentHeight);
    }

    // Release the remaining delegates before continuing to layout the current row
    // if applicable. The current item is not affected by releaseDelegates.
    if (row != layoutRows.constEnd())
        releaseDelegates(row->start, -1);

    for (; currentIndex >= 0 && row != layoutRows.constEnd() && row->end <= currentIndex; row++) {
        if (currentIndex >= row->start) {
            layoutRow(*row, y, false);
            break;
        }
        y += row->height;
    }
}

void FlexSection::layoutRow(const FlexRow &row, qreal y, bool create)
{
    qreal x = 0;

    for (int i = row.start; i <= row.end; i++) {
        int viewIndex = mapToView(i);
        // XXX It's probably worth caching these.
        double ratio = view->indexFlexRatio(viewIndex);
        if (!ratio)
            ratio = 1;
        qreal width = ratio * row.height;

        QQuickItem *item = delegates.value(i);
        if (!item) {
            if (create) {
                item = view->createItem(viewIndex);
                item->setVisible(true);
                delegates.insert(i, item);
            } else if (viewIndex == view->currentIndex) {
                item = view->currentItem;
            }

            if (!item) {
                x += width;
                continue;
            }

            // XXX should not be reparenting, but note that currentItem
            // may not have the correct parent yet
            item->setParentItem(m_sectionItem->contentItem());
        }

        item->setPosition(QPointF(x, y));
        item->setSize(QSizeF(width, row.height));
        x += width;
    }
}

int FlexSection::rowAt(qreal target) const
{
    qreal y = 0;
    for (int i = 0; i < layoutRows.size(); i++) {
        y += layoutRows[i].height;
        if (y > target)
            return i;
    }
    return -1;
}

int FlexSection::rowIndexAt(int rowIndex, qreal target, bool nearest) const
{
    if (rowIndex < 0 || rowIndex >= layoutRows.size())
        return -1;
    qreal x = 0;
    qreal dist = -1;
    const FlexRow &row = layoutRows[rowIndex];
    for (int i = row.start; i <= row.end; i++) {
        int viewIndex = mapToView(i);
        double ratio = view->indexFlexRatio(viewIndex);
        if (!ratio)
            ratio = 1;
        qreal width = ratio * row.height;

        if (target >= x && target < x + width) {
            return i;
        } else if (target < x) {
            if (nearest)
                return (dist < 0 || x - target < dist) ? i : i - 1;
            else
                break;
        } else {
            dist = target - (x + width);
            x += width;
        }
    }
    return nearest ? row.end : -1;
}

int FlexSection::indexAt(const QPointF &pos) const
{
    return rowIndexAt(rowAt(pos.y()), pos.x());
}

int FlexSection::rowForIndex(int index) const
{
    auto it = std::lower_bound(layoutRows.begin(), layoutRows.end(), index, [](const FlexRow &row, int i) { return row.end < i; });
    if (it == layoutRows.end())
        return -1;
    Q_ASSERT(index >= it->start && index <= it->end);
    return std::distance(layoutRows.begin(), it);
}

QRectF FlexSection::geometryOf(int index) const
{
    int rowIndex = rowForIndex(index);
    if (rowIndex < 0)
        return QRectF();
    const FlexRow &row = layoutRows[rowIndex];

    QRectF geom;
    for (int i = 0; i < rowIndex; i++)
        geom.setY(geom.y() + layoutRows[i].height);
    geom.setHeight(row.height);

    for (int i = row.start; i <= index; i++) {
        // XXX This needs to be cached and not copied everywhere
        int viewIndex = mapToView(i);
        double ratio = view->indexFlexRatio(viewIndex);
        if (!ratio)
            ratio = 1;
        qreal width = ratio * row.height;
        if (i == index) {
            geom.setWidth(width);
            break;
        }
        geom.setX(geom.x() + width);
    }

    return geom;
}

void FlexSection::releaseSectionDelegate()
{
    releaseDelegates();
    if (m_sectionItem) {
        qCDebug(lcDelegate) << "releasing section delegate" << m_sectionItem;
        m_sectionItem->destroy();
        m_sectionItem = nullptr;
    }
}

void FlexSection::releaseDelegates(int first, int last)
{
    int released = 0;
    auto it = delegates.begin();
    if (first > 0)
        it = delegates.lowerBound(first);
    while (it != delegates.end()) {
        if (last >= 0 && it.key() > last)
            break;
        (*it)->setVisible(false);
        view->model->release(*it);
        it = delegates.erase(it);
        released++;
    }

    if (released)
        qCDebug(lcDelegate) << "released" << released << "delegates between" << first << "and" << last;
}

FlexSectionItem *FlexSection::ensureItem()
{
    if (m_sectionItem)
        return m_sectionItem;
    if (!view->sectionDelegate) {
        qCWarning(lcDelegate) << "Section delegate is not set";
        return nullptr;
    }

    Q_ASSERT(qmlContext(view->q));
    QQmlContext *context = new QQmlContext(qmlContext(view->q), this);

    QVariantMap properties{{"name", value}};
    context->setProperty("section", properties);

    QObject *object = view->sectionDelegate->beginCreate(context);
    QQuickItem *item = qobject_cast<QQuickItem*>(object);
    if (!item) {
        qCWarning(lcDelegate) << "Section delegate must be an Item";
        view->sectionDelegate->completeCreate();
        object->deleteLater();
        return nullptr;
    }
    QQml_setParent_noEvent(item, this);
    item->setProperty("_flexsection", QVariant::fromValue(this));
    item->setParentItem(view->q->contentItem());
    view->sectionDelegate->completeCreate();

    QQuickItemPrivate::get(item)->addItemChangeListener(view, QQuickItemPrivate::Geometry);

    m_sectionItem = new FlexSectionItem(this, item);
    return m_sectionItem;
}

FlexSectionItem *FlexSection::qmlAttachedProperties(QObject *obj)
{
    FlexSection *section = obj->property("_flexsection").value<FlexSection*>();
    if (!section)
        return nullptr;
    return section->m_sectionItem;
}

FlexSectionItem::FlexSectionItem(FlexSection *section, QQuickItem *item)
    : QObject(item)
    , m_section(section)
    , m_item(item)
{
}

FlexSectionItem::~FlexSectionItem()
{
}

QQuickItem *FlexSectionItem::contentItem()
{
    if (!m_contentItem) {
        m_contentItem = new QQuickItem(m_item);
    }

    return m_contentItem;
}

void FlexSectionItem::setContentItem(QQuickItem *item)
{
    if (item == m_contentItem)
        return;

    if (m_contentItem) {
        for (auto child : m_contentItem->childItems())
            child->setParentItem(item);
    }

    m_contentItem = item;
    emit contentItemChanged();
}

bool FlexSectionItem::isCurrentSection() const
{
    return m_section->view->currentSection == m_section;
}

QQuickItem *FlexSectionItem::currentItem() const
{
    if (isCurrentSection())
        return m_section->view->currentItem;
    return nullptr;
}

void FlexSectionItem::destroy()
{
    // m_item owns this object and m_contentItem, and it must destroy first
    m_item->setVisible(false);
    m_item->deleteLater();
}
