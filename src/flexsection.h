#pragma once

#include "justifyview_p.h"

class RowCandidate;

class FlexSection : public QObject
{
    Q_OBJECT

public:
    JustifyViewPrivate * const view;
    const QString value;

    // XXX part of section delegate
    double height = 0;

    int viewStart = -1;
    int count = 0;

    FlexSection(JustifyViewPrivate *view, const QString &value);
    virtual ~FlexSection();

    int mapToView(int i) const
    {
        Q_ASSERT(viewStart >= 0);
        Q_ASSERT(i < count);
        return viewStart+i;
    }

    int mapToSection(int i) const
    {
        Q_ASSERT(viewStart >= 0);
        if (i < viewStart || i >= viewStart + count)
            return -1;
        return i - viewStart;
    }

    // XXX These are not touching viewStart

    void insert(int i, int count);
    void remove(int i, int count);
    void change(int i, int count);

    bool layout();
    void layoutDelegates(double y, const QRectF &visibleArea);

private:
    QList<RowCandidate> layoutRows;
    bool dirty = true;
};
