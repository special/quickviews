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
}

void FlexSection::remove(int i, int c)
{
    Q_ASSERT(i >= 0 && i < count);
    Q_ASSERT(c >= 0);
    Q_ASSERT(i+c <= count);
    count -= c;
}

void FlexSection::change(int i, int c)
{
    Q_ASSERT(i >= 0 && i < count);
    Q_ASSERT(c >= 0);
    Q_ASSERT(i+c <= count);
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
    const qreal idealHeight = 150;
    const qreal minHeight = idealHeight * 0.85;
    const qreal maxHeight = idealHeight * 1.15;

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

            // XXX as of here, we've selected the best path to i+1 (but not that i+1 will be used)
            // all other paths to i+1 are now dead
            possible.insert(i+1, next);
            qCDebug(lcFlex) << "considering rows from" << i+1 << "with" << breakCandidates.size() << "paths" << "selected" << next.upStart << next.upEnd << "for cost" << next.cost;
        }

        // XXX anything in break for last index or the final bits are our terminating nodes..
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

    return true;
}

void FlexSection::layoutDelegates(double left, double y)
{
    auto row = layoutRows.constBegin();
    double x = left;
    for (int i = 0; i < count; i++) {
        // XXX visible area, etc..
        QQuickItem *item = view->createItem(mapToView(i));
        if (!item)
            return;
        double ratio = view->indexFlexRatio(mapToView(i));

        if (i > row->end) {
            y += row->height;
            x = left;
            row++;
            Q_ASSERT(row != layoutRows.constEnd());
        }
        Q_ASSERT(i >= row->start && i <= row->end);

        qreal width = ratio * row->height;
        item->setPosition(QPointF(x, y));
        item->setSize(QSizeF(width, row->height));
        x += width;
    }
}
