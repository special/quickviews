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
    m_contentHeight = 0;
    m_estimatedHeight = 0;
    count = 0;
    layoutRows.clear();
    releaseSectionDelegate();
    dirty = false;
}

void FlexSection::insert(int i, int c)
{
    Q_ASSERT(i >= 0 && i <= count);
    count += c;
    dirty = true;

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
    dirty = true;

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
    dirty = true;
}

bool FlexSection::setViewportWidth(qreal width)
{
    if (viewportWidth == width)
        return false;
    viewportWidth = width;
    dirty = true;
    return true;
}

bool FlexSection::setIdealHeight(qreal min, qreal ideal, qreal max)
{
    if (min == minHeight && ideal == idealHeight && max == maxHeight)
        return false;
    minHeight = min;
    idealHeight = ideal;
    maxHeight = max;
    dirty = true;
    return true;
}

qreal FlexSection::estimatedHeight() const
{
    // XXX This needs a decent estimation algorithm
    return std::max(10., m_contentHeight);
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
    if (viewportWidth < 1 || minHeight < 1 || idealHeight < 1 || maxHeight < 1 || count < 1) {
        dirty = false;
        return true;
    }

    QElapsedTimer tm;
    tm.restart();

    QVector<FlexRow> rows;
    QVector<FlexRow> openRows{FlexRow(0)};
    int nStartPositions = 0;

    qCDebug(lcFlexLayout) << "layout for section viewStart" << viewStart << "count" << count;

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
    dirty = false;
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

    double y = 0;

    auto row = layoutRows.constBegin();
    for (; row != layoutRows.constEnd(); row++) {
        if (y + row->height >= cacheArea.top())
            break;
        y += row->height;
    }

    if (row == layoutRows.constEnd()) {
        releaseDelegates();
        return;
    }
    if (row->start > 0)
        releaseDelegates(0, row->start - 1);

    double x = 0;
    int i;
    for (i = row->start; i < count; i++) {
        if (i > row->end) {
            y += row->height;
            x = 0;
            row++;
            Q_ASSERT(row != layoutRows.constEnd());
            Q_ASSERT(y+row->height <= m_contentHeight);

            if (y > cacheArea.bottom())
                break;
        }
        Q_ASSERT(i >= row->start && i <= row->end);

        QQuickItem *item = delegates.value(i);
        if (!item) {
            item = view->createItem(mapToView(i));
            if (!item)
                return;
            // XXX should not be reparenting
            item->setParentItem(contentItem);
            delegates.insert(i, item);
        }
        double ratio = view->indexFlexRatio(mapToView(i));
        if (!ratio)
            ratio = 1;

        qreal width = ratio * row->height;
        item->setPosition(QPointF(x, y));
        item->setSize(QSizeF(width, row->height));
        x += width;
    }

    if (i < count)
        releaseDelegates(i, -1);
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

void FlexSectionItem::destroy()
{
    // m_item owns this object and m_contentItem, and it must destroy first
    m_item->setVisible(false);
    m_item->deleteLater();
}
