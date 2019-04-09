#pragma once

#include "flexview_p.h"

class FlexRow;
class FlexSectionItem;

class FlexSection : public QObject
{
    Q_OBJECT

    enum class DirtyFlag
    {
        None = 0,
        Data = 0x1,
        Geometry = 0x2,
        Indices = 0x4 | Data,
        All = Indices | Geometry | Data
    };
    Q_DECLARE_FLAGS(DirtyFlags, DirtyFlag)

public:
    FlexViewPrivate * const view;
    const QString value;

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
    bool setSpacing(qreal horizontal, qreal vertical);
    bool setIdealHeight(qreal min, qreal ideal, qreal max);

    void insert(int i, int count);
    void remove(int i, int count);
    void change(int i, int count);
    void clear();

    void setCurrentIndex(int index);

    bool layout();
    void layoutDelegates(const QRectF &visibleArea, const QRectF &cacheArea);
    void releaseDelegates(int first = 0, int last = -1);
    void releaseSectionDelegate();

    qreal estimatedHeight() const;
    qreal contentHeight() const { return m_contentHeight; }

    int indexAt(const QPointF &pos) const;
    int rowAt(qreal y) const;
    int rowIndexAt(int row, qreal x, bool nearest = false) const;
    QRectF geometryOf(int i) const;

    int rowForIndex(int index) const;
    int rowCount() const { return layoutRows.size(); }

    FlexSectionItem *ensureItem();
    static FlexSectionItem *qmlAttachedProperties(QObject *obj);

private:
    FlexSectionItem *m_sectionItem = nullptr;
    QVector<FlexRow> layoutRows;
    qreal viewportWidth = 0;
    qreal minHeight = 0;
    qreal idealHeight = 0;
    qreal maxHeight = 0;
    qreal hSpacing = 0;
    qreal vSpacing = 0;
    qreal m_contentHeight = 0;
    qreal m_lastSectionHeight = 0;
    int m_lastSectionCount = 0;
    int currentIndex = -1;
    DirtyFlags dirty = DirtyFlag::All;

    qreal badness(const FlexRow &row) const;
    void layoutRow(const FlexRow &row, qreal y, bool create = true);
};
QML_DECLARE_TYPEINFO(FlexSection, QML_HAS_ATTACHED_PROPERTIES)

class FlexSectionItem : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QQuickItem* contentItem READ contentItem WRITE setContentItem NOTIFY contentItemChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool isCurrentSection READ isCurrentSection NOTIFY isCurrentSectionChanged)
    Q_PROPERTY(QQuickItem* currentItem READ currentItem NOTIFY currentItemChanged)

public:
    FlexSectionItem(FlexSection *section);
    virtual ~FlexSectionItem();

    QString name() const { return m_section->value; }
    int count() const { return m_section->count; }

    QQuickItem *contentItem();
    void setContentItem(QQuickItem *contentItem);

    bool isCurrentSection() const;
    QQuickItem *currentItem() const;

    // Non-QML API
    QQuickItem *item() const { return m_item; }
    void setItem(QQuickItem *item);
    void destroy();

signals:
    void countChanged();
    void contentItemChanged();
    void isCurrentSectionChanged();
    void currentItemChanged();

private:
    const FlexSection *m_section;
    QQuickItem *m_item = nullptr;
    QQuickItem *m_contentItem = nullptr;
};
