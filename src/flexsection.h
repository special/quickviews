#pragma once

#include "flexview_p.h"

class FlexRow;

class FlexSection : public QObject
{
    Q_OBJECT

public:
    FlexViewPrivate * const view;
    const QString value;

    // XXX part of section delegate
    double height = 0;

    int viewStart = -1;
    int count = 0;

    FlexSection(FlexViewPrivate *view, const QString &value);
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

    bool setViewportWidth(qreal width);
    bool setIdealHeight(qreal min, qreal ideal, qreal max);

    void insert(int i, int count);
    void remove(int i, int count);
    void change(int i, int count);
    void clear();

    bool layout();
    void layoutDelegates(double y, const QRectF &visibleArea);
    void releaseDelegates();

private:
    QVector<FlexRow> layoutRows;
    QMap<int, QQuickItem*> delegates;
    qreal viewportWidth = 0;
    qreal minHeight = 0;
    qreal idealHeight = 0;
    qreal maxHeight = 0;
    bool dirty = true;

    qreal badness(const FlexRow &row) const;
};
