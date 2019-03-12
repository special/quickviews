#include "flexsection.h"
#include <QLoggingCategory>

// FlexSection contains a range of rows to be laid out as a discrete section. It manages
// layout geometry and delegates within its range.

Q_LOGGING_CATEGORY(lcFlex, "crimson.justifyview.flex")

FlexSection::FlexSection(JustifyViewPrivate *view, const QString &value)
    : QObject(view)
    , view(view)
    , value(value)
{
}

FlexSection::~FlexSection()
{
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
            view->model->release(it.value());
        } else {
            adjusted.insert(k, it.value());
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

    QList<RowCandidate> candidates;
    QList<RowCandidate> possible{RowCandidate(0)};
    QList<RowCandidate> breakCandidates;

    for (int i = 0; i < count; i++) {
        qreal ratio = view->indexFlexRatio(mapToView(i));

        bool canBreak = false;
        breakCandidates.clear();
        Q_ASSERT(!possible.isEmpty());
        for (int p = 0; p < possible.size(); p++) {
            RowCandidate &candidate = possible[p];
            candidate.ratio += ratio;
            candidate.height = viewportWidth / candidate.ratio;

            if (candidate.height > maxHeight && i+1 < count) {
                continue;
            } else if (candidate.height < minHeight) {
                // XXX edge case of very small ratio with 1 item
                qCDebug(lcFlex) << "no more candidates starting from" << candidate.start;
                possible.removeAt(p);
                p--;
            } else {
                RowCandidate row = candidate;
                row.end = i;

                if (row.height < idealHeight) {
                    row.badness = 1 - (row.height - minHeight) / (idealHeight - minHeight);
                } else if (row.height > idealHeight) {
                    row.badness = 1 - (maxHeight - row.height) / (maxHeight - idealHeight);
                }
                row.cost += row.badness;

                if (i+1 == count && row.height > maxHeight) {
                    // Set last partial row to idealHeight, but keep original badness
                    row.height = idealHeight;
                }

                candidates.append(row);
                breakCandidates.append(row); // XXX Just a trailing portion of candidates, optimize
                canBreak = true;

                qCDebug(lcFlex) << "added candidate row" << row.start << "to" << row.end << "(inclusive) at height" << row.height << "and ratio" << row.ratio << "with badness" << row.badness << "final cost" << row.cost;
            }
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

            possible.insert(i+1, next);
            qCDebug(lcFlex) << "considering rows from" << i+1 << "with" << breakCandidates.size() << "paths" << "selected" << next.upStart << next.upEnd << "for cost" << next.cost;
        }
    }

    layoutRows.clear();
    qCDebug(lcFlex) << "final rows:";
    for (const auto &row : breakCandidates) {
        qCDebug(lcFlex) << "\t" << row.start << "to" << row.end << "cost" << row.cost << "parent" << row.upStart << "to" << row.upEnd;
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
    qCDebug(lcFlex) << "selected layout:";
    for (const auto &row : layoutRows) {
        qCDebug(lcFlex) << "\t" << row.start << "to" << row.end << "cost" << row.cost << "height" << row.height;
        this->height += row.height;
    }

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
        view->model->release(delegates.first());
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
