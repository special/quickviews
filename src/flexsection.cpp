#include "flexsection.h"
#include <QLoggingCategory>

// FlexSection contains a range of rows to be laid out as a discrete section. It manages
// layout geometry and delegates within its range.

Q_LOGGING_CATEGORY(lcFlex, "crimson.justifyview.flex")
Q_LOGGING_CATEGORY(lcFlexLayout, "crimson.justifyview.flex.layout")

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

struct RowCandidate
{
    int start;
    int end; // inclusive
    qreal ratio;
    qreal height;
    qreal badness;

    int upStart = -1;
    int upEnd = -1;
    qreal cost = 0;

    RowCandidate(int start)
        : start(start), end(-1), ratio(0), height(0), badness(0)
    {
    }
};

bool FlexSection::layout()
{
    const int viewportWidth = view->q->width();
    const qreal idealHeight = 300;
    const qreal minHeight = idealHeight * 0.85;
    const qreal maxHeight = idealHeight * 1.15;

    if (!dirty)
        return false;

    QElapsedTimer tm;
    tm.restart();

    QList<RowCandidate> candidates;
    QList<RowCandidate> possible{RowCandidate(0)};
    QList<RowCandidate> breakCandidates;
    int nBreaks = 0;

    for (int i = 0; i < count; i++) {
        qreal ratio = view->indexFlexRatio(mapToView(i));

        bool canBreak = false;
        breakCandidates.clear();
        Q_ASSERT(!possible.isEmpty());
        for (int p = 0; p < possible.size(); p++) {
            RowCandidate &candidate = possible[p];
            candidate.ratio += ratio;
            candidate.height = viewportWidth / candidate.ratio;

            if (candidate.height > maxHeight && i+1 < count)
                continue;
            if (candidate.height < minHeight && candidate.end >= 0) {
                // Below minimum height, and at least one candidate has been recorded with this start index
                qCDebug(lcFlexLayout) << "no other possible rows start at" << candidate.start << "because adding row"
                    << i << "reduces height to" << candidate.height << "(min" << minHeight << ")";
                possible.removeAt(p);
                p--;
                continue;
            }

            candidate.end = i;
            RowCandidate row = candidate;

            if (row.height < idealHeight) {
                row.badness = 1 - (row.height - minHeight) / (idealHeight - minHeight);
            } else if (row.height > idealHeight) {
                row.badness = 1 - (maxHeight - row.height) / (maxHeight - idealHeight);
            }
            row.cost += row.badness;

            if (row.height > maxHeight) {
                // Set last partial row to idealHeight, but keep original badness
                row.height = idealHeight;
            } else if (row.height < minHeight) {
                // XXX if i > row.start, the candidate ending at i-1 was just as viable.
                // There was no way to know that at the time, and adding it now is complex
                // because it _might_ create a new possible break.
                qCDebug(lcFlexLayout) << "only possible candidate starting at" << row.start << "ends at" << i
                    << "with height" << row.height << "below minimum" << minHeight;
                possible.removeAt(p);
                p--;
            }

            candidates.append(row);
            breakCandidates.append(row); // XXX Just a trailing portion of candidates, optimize
            canBreak = true;

            qCDebug(lcFlexLayout) << "added candidate row" << row.start << "to" << row.end << "(inclusive) at height" << row.height << "and ratio" << row.ratio << "with badness" << row.badness << "final cost" << row.cost;
        }

        if (canBreak && i+1 < count) {
            RowCandidate next(i+1);
            for (const auto &node : breakCandidates) {
                if (!next.cost || node.cost < next.cost) {
                    next.cost = node.cost;
                    next.upStart = node.start;
                    next.upEnd = node.end;
                }
            }

            nBreaks++;
            possible.insert(i+1, next);
            qCDebug(lcFlexLayout) << "considering rows from" << i+1 << "with" << breakCandidates.size() << "paths" << "selected" << next.upStart << next.upEnd << "for cost" << next.cost;
        }
    }

    layoutRows.clear();
    qCDebug(lcFlexLayout) << "final rows:";
    for (const auto &row : breakCandidates) {
        qCDebug(lcFlexLayout) << "\t" << row.start << "to" << row.end << "cost" << row.cost << "parent" << row.upStart << "to" << row.upEnd;
        if (layoutRows.isEmpty())
            layoutRows.append(row);
        else if (row.cost < layoutRows.first().cost)
            layoutRows.first() = row;
    }

    for (auto it = candidates.rbegin(); it != candidates.rend(); it++) {
        int start = layoutRows.first().upStart;
        int end = layoutRows.first().upEnd;
        Q_ASSERT(end > it->start);
        if (it->start == start && it->end == end)
            layoutRows.prepend(*it);
    }

    this->height = 0;
    qCDebug(lcFlexLayout) << "selected layout:";
    for (const auto &row : layoutRows) {
        qCDebug(lcFlexLayout) << "\t" << row.start << "to" << row.end << "cost" << row.cost << "height" << row.height;
        this->height += row.height;
    }

    qCDebug(lcFlex) << "layout has" << layoutRows.size() << "rows for" << count << "items; considered" << candidates.size() << "rows from" << nBreaks << "positions in" << tm.elapsed() << "ms";
    dirty = false;
    return true;
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
