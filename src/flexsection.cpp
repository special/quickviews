#include "flexsection.h"
#include <QtQuick/private/qquickitem_p.h>

// FlexSection contains a range of rows to be laid out as a discrete section. It manages
// layout geometry and delegates within its range.

Q_LOGGING_CATEGORY(lcSection, "crimson.flexview.section")
Q_LOGGING_CATEGORY(lcFlexLayout, "crimson.flexview.layout.flex", QtWarningMsg)

#if 0
#define DEBUG_LAYOUT() qCDebug(lcFlexLayout)
#define DEBUGGING_LAYOUT
#else
#define DEBUG_LAYOUT() if (false) qCDebug(lcFlexLayout)
#endif

struct ModelData
{
    DelegateRef delegate;
    qreal size;

    ModelData()
        : size(0)
    {
    }

    ModelData(const ModelData &) = delete;
    ModelData &operator=(const ModelData &) = delete;
    ModelData(ModelData &&) = default;
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
    m_currentItem = nullptr;
    releaseSectionDelegate();
    m_contentHeight = 0;
    m_lastSectionHeight = 0;
    m_lastSectionCount = 0;
    count = 0;
    currentIndex =- 1;
    layoutRows.clear();
    m_data.clear();
    dirty = {};
}

void FlexSection::insert(int i, int c)
{
    Q_ASSERT(i >= 0 && i <= count);
    adjustIndex(i, c);
    count += c;
    dirty |= DirtyFlag::Indices;
}

void FlexSection::remove(int i, int c)
{
    Q_ASSERT(i >= 0 && i < count);
    Q_ASSERT(c >= 0);
    Q_ASSERT(i+c <= count);
    adjustIndex(i, -c);
    count -= c;
    dirty |= DirtyFlag::Indices;
}

void FlexSection::change(int i, int c)
{
    Q_ASSERT(i >= 0 && i < count);
    Q_ASSERT(c >= 0);
    Q_ASSERT(i+c <= count);

    for (int j = i; j < i+c; j++) {
        ModelData &data = indexData(j);
        qreal size = view->indexFlexRatio(mapToView(j));
        if (size != data.size) {
            data.size = size;
            dirty |= DirtyFlag::Data;
        }
    }
}

void FlexSection::adjustIndex(int from, int delta)
{
    auto it = m_data.lower_bound(from);
    if (!delta || it == m_data.end())
        return;

    if (currentIndex >= from) {
        if (delta < 0 && from - currentIndex < delta)
            setCurrentIndex(-1);
        else
            currentIndex += delta;
    }

    if (delta > 0) {
        auto first = it;
        it = m_data.end();

        for (;;) {
            auto prev = it;
            it--;
            int nkey = it->first + delta;
            m_data.insert(prev, std::move(*it));
            if (it == first) {
                m_data.erase(it);
                break;
            }
            it = m_data.erase(it);
        }
    } else {
        while (it != m_data.end()) {
            int nkey = it->first + delta;
            if (nkey >= from)
                m_data.insert(it, std::move(*it));
            it = m_data.erase(it);
        }
    }
}

bool FlexSection::setViewportWidth(qreal width)
{
    if (viewportWidth == width)
        return false;
    viewportWidth = width;
    dirty |= DirtyFlag::Geometry;
    return true;
}

bool FlexSection::setSpacing(qreal horizontal, qreal vertical)
{
    if (hSpacing == horizontal && vSpacing == vertical)
        return false;
    vSpacing = vertical;
    hSpacing = horizontal;
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

    int oldIndex = currentIndex;
    currentIndex = index;
    if (currentIndex >= 0) {
        ensureItem();
        m_currentItem = delegate(index, true);
    } else {
        m_currentItem = nullptr;
    }

    qCDebug(lcSection) << "section current index changed" << oldIndex << "->" << currentIndex;

    if (m_sectionItem) {
        if ((oldIndex >= 0) != (currentIndex >= 0))
            m_sectionItem->isCurrentSectionChanged();
        m_sectionItem->currentItemChanged();
    }
}

QQuickItem *FlexSection::currentItem()
{
    return m_currentItem.get();
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

    std::vector<FlexRow> rows;
    std::vector<FlexRow> openRows{FlexRow(0)};
    int nAdditions = 0;

    DEBUG_LAYOUT() << "layout for section viewStart" << viewStart << "count" << count << "dirty" << dirty;

    // XXX oof, this is redoing layout for every data change because the size role may have changed..
    // a cache could save a lot of pain

    for (int i = 0; i < count; i++) {
        auto &data = indexData(i);
        if (!data.size) {
            data.size = view->indexFlexRatio(mapToView(i));
            if (!data.size)
                data.size = 1;
        }

        FlexRow addingRow(0);
        Q_ASSERT(!openRows.empty());
        for (auto it = openRows.begin(); it != openRows.end(); ) {
            FlexRow &candidate = *it;
            candidate.ratio += data.size;
            candidate.height = (viewportWidth - (hSpacing * (i - candidate.start))) / candidate.ratio;
            nAdditions++;

            if (candidate.height > maxHeight && i+1 < count) {
                it++;
                continue;
            } else if (candidate.height < minHeight && candidate.end >= 0) {
                // Below minimum height, and at least one candidate has been recorded with this start index
                DEBUG_LAYOUT() << ".... no more rows start at" << candidate.start << "because adding"
                    << i << "reduces height to" << candidate.height << "with minimum" << minHeight;
                it = openRows.erase(it);
                continue;
            } else {
                qreal cost = candidate.cost + badness(candidate);
                candidate.end = i;

                if (addingRow.end < 0 || cost < addingRow.cost) {
                    addingRow = candidate;
                    addingRow.cost = cost;

                    if (addingRow.height > maxHeight) {
                        // Set last partial row to idealHeight, but keep original badness
                        addingRow.height = idealHeight;
                    } else if (addingRow.height < minHeight) {
                        // XXX if i > row.start, the candidate ending at i-1 was just as viable.
                        // There was no way to know that at the time, and adding it now is complex.
                        // XXX revisit how complex that actually is... can candidate be changed back?
                        it = openRows.erase(it);
                        continue;
                    }
                }

                it++;
                continue;
            }

            Q_UNREACHABLE();
        }

        if (addingRow.end >= 0) {
            DEBUG_LAYOUT() << ".. row:" << addingRow.start << "to" << addingRow.end << "height" << addingRow.height
                << "ratio" << addingRow.ratio << "badness" << badness(addingRow) << "cost" << addingRow.cost;
            rows.push_back(addingRow);

            if (addingRow.height < minHeight) {
                DEBUG_LAYOUT() << ".... row of height" << addingRow.height << "is below minimum" << minHeight << "but no other rows could start from" << addingRow.start;
            }

            FlexRow next(i+1);
            next.cost = addingRow.cost;
            next.prev = rows.size() - 1;
            openRows.push_back(next);
        }
    }

    for (int i = rows.size() - 1; i >= 0; i = rows[i].prev) {
        layoutRows.append(rows[i]);
        m_contentHeight += rows[i].height;
    }
    std::reverse(layoutRows.begin(), layoutRows.end());
    Q_ASSERT(!layoutRows.isEmpty());
    Q_ASSERT(layoutRows[0].start == 0);
    m_contentHeight += vSpacing * (layoutRows.size() - 1);

#ifdef DEBUGGING_LAYOUT
    if (lcFlexLayout().isDebugEnabled()) {
        qCDebug(lcFlexLayout) << ".. selected rows:";
        for (const auto &row : layoutRows) {
            qCDebug(lcFlexLayout) << "...." << row.start << "to" << row.end << "cost" << row.cost << "height" << row.height;
        }
    }
#endif

    qCDebug(lcLayout) << "section:" << layoutRows.size() << "rows for" << count << "items starting" << viewStart << "in" << m_contentHeight << "px; built" << rows.size() << "rows from" << nAdditions << "additions in" << tm.elapsed() << "ms";

    if (dirty & DirtyFlag::Indices && m_sectionItem)
        emit m_sectionItem->countChanged();

    dirty = {};
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
    auto row = layoutRows.constBegin();
    for (; row != layoutRows.constEnd(); row++) {
        if (y + row->height >= cacheArea.top())
            break;
        if (currentIndex >= row->start && currentIndex <= row->end)
            layoutRow(*row, y, false);
        y += row->height + vSpacing;
    }

    if (row == layoutRows.constEnd()) {
        releaseDelegates();
        return;
    }
    if (row->start > 0)
        releaseDelegates(0, row->start - 1);

    for (; row != layoutRows.constEnd(); row++) {
        Q_ASSERT(y < m_contentHeight);
        if (y > cacheArea.bottom())
            break;

        layoutRow(*row, y);
        y += row->height + vSpacing;
    }

    // Release the remaining delegates before continuing to layout the current row
    // if applicable. The current item is not affected by releaseDelegates.
    if (row != layoutRows.constEnd())
        releaseDelegates(row->start, -1);

    for (; currentIndex >= 0 && row != layoutRows.constEnd() && currentIndex >= row->start; row++) {
        if (currentIndex <= row->end) {
            layoutRow(*row, y, false);
            break;
        }
        y += row->height + vSpacing;
    }
}

void FlexSection::layoutRow(const FlexRow &row, qreal y, bool create)
{
    qreal x = 0;
    QQuickItem *contentItem = m_sectionItem->contentItem();

    for (int i = row.start; i <= row.end; i++) {
        if (i > row.start)
            x += hSpacing;

        int viewIndex = mapToView(i);
        auto &data = indexData(i);
        qreal width = data.size * row.height;

        // XXX inefficient everywhere
        auto item = delegate(i, create);
        if (!item) {
            x += width;
            continue;
        }

        item->setParentItem(contentItem);
        item->setPosition(QPointF(x, y));
        item->setSize(QSizeF(width, row.height));
        if (i == currentIndex)
            item->setFocus(true);
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
        y += vSpacing;
        if (y > target)
            return -1;
    }
    return -1;
}

int FlexSection::rowIndexAt(int rowIndex, qreal target, bool nearest)
{
    if (rowIndex < 0 || rowIndex >= layoutRows.size())
        return -1;
    qreal x = 0;
    qreal dist = -1;
    const FlexRow &row = layoutRows[rowIndex];
    for (int i = row.start; i <= row.end; i++) {
        if (i > row.start)
            x += hSpacing;

        int viewIndex = mapToView(i);
        auto &data = indexData(i);
        qreal width = data.size * row.height;

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

int FlexSection::indexAt(const QPointF &pos)
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

QRectF FlexSection::geometryOf(int index)
{
    int rowIndex = rowForIndex(index);
    if (rowIndex < 0)
        return QRectF();
    const FlexRow &row = layoutRows[rowIndex];

    QRectF geom;
    for (int i = 0; i < rowIndex; i++)
        geom.setY(geom.y() + layoutRows[i].height + vSpacing);
    geom.setHeight(row.height);

    for (int i = row.start; i <= index; i++) {
        if (i > row.start)
            geom.setX(geom.x() + hSpacing);

        int viewIndex = mapToView(i);
        auto &data = indexData(i);
        qreal width = data.size * row.height;
        if (i == index) {
            geom.setWidth(width);
            break;
        }
        geom.setX(geom.x() + width);
    }

    return geom;
}

ModelData &FlexSection::indexData(int index)
{
    Q_ASSERT(index >= 0);
    Q_ASSERT(index < count);

    auto it = m_data.lower_bound(index);
    if (it != m_data.end() && it->first == index)
        return it->second;

    m_data.insert(it, std::make_pair(index, ModelData()));
    it--;
    Q_ASSERT(it->first == index);
    return it->second;
}

DelegateRef FlexSection::delegate(int index, bool create)
{
    auto &data = indexData(index);
    if (!data.delegate) {
        // XXX inefficient, queries unnecessarily, but things need reworking around delegates with this anyway
        if (!create) {
            data.delegate = view->items.item(mapToView(index));
        } else {
            // XXX AsyncIfNested, etc
            data.delegate = view->items.createItem(mapToView(index), view->delegate, m_sectionItem->contentItem(), QQmlIncubator::Synchronous);
        }
    }

    return data.delegate;
}

void FlexSection::releaseSectionDelegate()
{
    Q_ASSERT(!currentItem());
    releaseDelegates();
    if (m_sectionItem) {
        qCDebug(lcDelegate) << "releasing section delegate" << m_sectionItem;
        m_sectionItem->destroy();
        m_sectionItem = nullptr;
    }
}

void FlexSection::releaseDelegates(int first, int last)
{
    Q_ASSERT(last < 0 || first <= last);

    int released = 0;
    auto it = m_data.begin();
    if (first > 0)
        it = m_data.lower_bound(first);

    for (; it != m_data.end(); it++) {
        if (last >= 0 && it->first > last)
            break;

        // It's not strictly necessary to keep the current item in ModelData, since a ref
        // is held by m_currentItem, but it's not a bad idea.
        if (it->first == currentIndex)
            continue;

        if (it->second.delegate) {
            it->second.delegate.reset();
            released++;
        }
    }

    if (released) {
        qCDebug(lcDelegate) << "released" << released << "delegates between" << first << "and" << last;
    }
}

FlexSectionItem *FlexSection::ensureItem()
{
    if (m_sectionItem)
        return m_sectionItem;
    if (!view->sectionDelegate) {
        qCWarning(lcDelegate) << "Section delegate is not set";
        return nullptr;
    }

    m_sectionItem = new FlexSectionItem(this);

    QQmlContext *parentContext = view->sectionDelegate->creationContext();
    if (!parentContext)
        parentContext = qmlContext(view->q);
    QQmlContext *context = new QQmlContext(parentContext, this);
    context->setContextProperty("_flexsection", QVariant::fromValue(this));

    QVariantMap properties{{"name", value}};
    context->setContextProperty("section", properties);

    QObject *object = view->sectionDelegate->beginCreate(context);
    QQuickItem *item = qobject_cast<QQuickItem*>(object);
    if (!item) {
        qCWarning(lcDelegate) << "Section delegate must be an Item";
        view->sectionDelegate->completeCreate();
        object->deleteLater();
        return nullptr;
    }
    QQml_setParent_noEvent(item, this);
    item->setParentItem(view->q->contentItem());
    m_sectionItem->setItem(item);

    view->sectionDelegate->completeCreate();
    QQuickItemPrivate::get(item)->addItemChangeListener(view, QQuickItemPrivate::Geometry);
    return m_sectionItem;
}

FlexSectionItem *FlexSection::qmlAttachedProperties(QObject *obj)
{
    QQmlContext *ctx = qmlContext(obj);
    FlexSection *section;
    if (ctx && (section = ctx->contextProperty("_flexsection").value<FlexSection*>()))
        return section->m_sectionItem;
    return nullptr;
}

FlexSectionItem::FlexSectionItem(FlexSection *section)
    : m_section(section)
{
}

FlexSectionItem::~FlexSectionItem()
{
}

void FlexSectionItem::setItem(QQuickItem *item)
{
    Q_ASSERT(!m_item);
    m_item = item;
    if (m_contentItem && !m_contentItem->parent()) {
        m_contentItem->setParent(item);
        m_contentItem->setParentItem(item);
    }
}

QQuickItem *FlexSectionItem::contentItem()
{
    if (!m_contentItem) {
        m_contentItem = new QQuickItem(m_item);
        QQmlEngine::setContextForObject(m_contentItem, qmlContext(m_item));
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

QQuickItem *FlexSectionItem::currentItem()
{
    return m_section->currentItem();
}

void FlexSectionItem::destroy()
{
    if (m_item) {
        // m_item owns this object and m_contentItem, and it must destroy first
        m_item->setVisible(false);
        m_item->deleteLater();
    } else {
        deleteLater();
    }
}
