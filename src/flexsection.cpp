#include "flexsection.h"
#include <QLoggingCategory>

// FlexSection contains a range of rows to be laid out as a discrete section. It manages
// layout geometry and delegates within its range.

Q_LOGGING_CATEGORY(lcFlex, "crimson.justifyview.flex")
Q_LOGGING_CATEGORY(lcFlexLayout, "crimson.justifyview.flex.layout")

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

FlexSection::FlexSection(JustifyViewPrivate *view, const QString &value)
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
    height = 0;
    count = 0;
    layoutRows.clear();
    for (auto item : delegates) {
        item->setVisible(false);
        view->model->release(item);
    }
    delegates.clear();
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

bool FlexSection::layout()
{
    if (!dirty)
        return false;

    layoutRows.clear();
    height = 0;
    if (viewportWidth < 1 || minHeight < 1 || idealHeight < 1 || maxHeight < 1) {
        dirty = false;
        return true;
    }

    QElapsedTimer tm;
    tm.restart();

    QVector<FlexRow> rows;
    QVector<FlexRow> openRows{FlexRow(0)};
    int nStartPositions = 0;

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
                qCDebug(lcFlexLayout) << "no more rows start at" << candidate.start << "because adding"
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
                qCDebug(lcFlexLayout) << "row from" << row.start << "to" << i << "height" << row.height
                    << "is below minimum" << minHeight << "but no other rows can start from" << row.start;
                openRows.removeAt(p);
                p--;
            }

            rows.append(row);
            rowsAdded++;
            qCDebug(lcFlexLayout) << "row:" << row.start << "to" << row.end << "height" << row.height
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
            qCDebug(lcFlexLayout) << "starting row at" << next.start << "reachable by" << rowsAdded
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
        } else if (row.start == layoutRows[0].prevStart) {
            Q_ASSERT(row.end == layoutRows[0].start-1);
            // This isn't ideal; prepend is slow on vector
            layoutRows.prepend(row);
        }
    }

    qCDebug(lcFlexLayout) << "selected rows:";
    for (const auto &row : layoutRows) {
        qCDebug(lcFlexLayout) << "\t" << row.start << "to" << row.end << "cost" << row.cost << "height" << row.height;
        this->height += row.height;
    }

    qCDebug(lcFlex) << "layout has" << layoutRows.size() << "rows for" << count << "items; considered" << rows.size() << "rows from" << nStartPositions << "positions in" << tm.elapsed() << "ms";
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

void FlexSection::layoutDelegates(double y, const QRectF &visibleArea)
{
    Q_ASSERT(!dirty);

    auto row = layoutRows.constBegin();
    for (; row != layoutRows.constEnd(); row++) {
        if (visibleArea.intersects(QRectF(visibleArea.x(), y, visibleArea.width(), row->height)))
            break;
        y += row->height;
    }
    if (row == layoutRows.constEnd())
        return;

    int first = row->start;
    while (!delegates.isEmpty() && delegates.firstKey() < first) {
        auto first = delegates.first();
        first->setVisible(false);
        view->model->release(first);
        delegates.erase(delegates.begin());
    }

    double x = 0;
    for (int i = row->start; i < count; i++) {
        if (i > row->end) {
            y += row->height;
            x = 0;
            row++;
            Q_ASSERT(row != layoutRows.constEnd());

            if (y > visibleArea.bottom())
                break;
        }
        Q_ASSERT(i >= row->start && i <= row->end);

        QQuickItem *item = delegates.value(i);
        if (!item) {
            item = view->createItem(mapToView(i));
            if (!item)
                return;
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

    int last = row->end;
    for (auto it = delegates.lowerBound(last+1); it != delegates.end(); ) {
        (*it)->setVisible(false);
        view->model->release(*it);
        it = delegates.erase(it);
    }
}
