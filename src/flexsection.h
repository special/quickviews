#pragma once

#include "flexview_p.h"

class FlexRow;
class FlexSectionItem;

class FlexSection : public QObject
{
    Q_OBJECT

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
    bool setIdealHeight(qreal min, qreal ideal, qreal max);

    void insert(int i, int count);
    void remove(int i, int count);
    void change(int i, int count);
    void clear();

    bool layout();
    void layoutDelegates(const QRectF &visibleArea, const QRectF &cacheArea);
    void releaseDelegates(int first = 0, int last = -1);
    void releaseSectionDelegate();

    qreal estimatedHeight() const;
    qreal contentHeight() const { return m_contentHeight; }

    FlexSectionItem *ensureItem();
    static FlexSectionItem *qmlAttachedProperties(QObject *obj);

private:
    FlexSectionItem *m_sectionItem = nullptr;
    QVector<FlexRow> layoutRows;
    QMap<int, QQuickItem*> delegates;
    qreal viewportWidth = 0;
    qreal minHeight = 0;
    qreal idealHeight = 0;
    qreal maxHeight = 0;
    qreal m_contentHeight = 0;
    qreal m_estimatedHeight = 0;
    bool dirty = true;

    qreal badness(const FlexRow &row) const;

};
QML_DECLARE_TYPEINFO(FlexSection, QML_HAS_ATTACHED_PROPERTIES)

class FlexSectionItem : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QQuickItem* contentItem READ contentItem WRITE setContentItem NOTIFY contentItemChanged)

public:
    FlexSectionItem(FlexSection *section, QQuickItem *item);
    virtual ~FlexSectionItem();

    QString name() const { return m_section->value; }

    QQuickItem *item() const { return m_item; }

    QQuickItem *contentItem();
    void setContentItem(QQuickItem *contentItem);

    void destroy();

signals:
    void contentItemChanged();

private:
    FlexSection *m_section;
    QQuickItem *m_item;
    QQuickItem *m_contentItem = nullptr;
};
