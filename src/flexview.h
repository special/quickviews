#pragma once

#include <QtQuick/private/qquickflickable_p.h>
#include <QAbstractItemModel>

class FlexViewPrivate;
class QQmlComponent;
class QAbstractItemModel;

class FlexView : public QQuickFlickable
{
    Q_OBJECT

    Q_PROPERTY(QAbstractItemModel* model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QQmlComponent* delegate READ delegate WRITE setDelegate NOTIFY delegateChanged)
    Q_PROPERTY(QQmlComponent* section READ section WRITE setSection NOTIFY sectionChanged)
    Q_PROPERTY(QString sectionRole READ sectionRole WRITE setSectionRole NOTIFY sectionRoleChanged)
    Q_PROPERTY(QString sizeRole READ sizeRole WRITE setSizeRole NOTIFY sizeRoleChanged)
    Q_PROPERTY(qreal idealHeight READ idealHeight WRITE setIdealHeight NOTIFY idealHeightChanged)
    Q_PROPERTY(qreal minHeight READ minHeight WRITE setMinHeight NOTIFY minHeightChanged)
    Q_PROPERTY(qreal maxHeight READ maxHeight WRITE setMaxHeight NOTIFY maxHeightChanged)
    Q_PROPERTY(qreal cacheBuffer READ cacheBuffer WRITE setCacheBuffer NOTIFY cacheBufferChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QQuickItem* currentItem READ currentItem NOTIFY currentItemChanged)
    Q_PROPERTY(QQuickItem* currentSection READ currentSection NOTIFY currentSectionChanged)
    Q_PROPERTY(qreal verticalSpacing READ verticalSpacing WRITE setVerticalSpacing NOTIFY verticalSpacingChanged)
    Q_PROPERTY(qreal horizontalSpacing READ horizontalSpacing WRITE setHorizontalSpacing NOTIFY horizontalSpacingChanged)
    Q_PROPERTY(qreal sectionSpacing READ sectionSpacing WRITE setSectionSpacing NOTIFY sectionSpacingChanged)

public:
    FlexView(QQuickItem *parent = nullptr);
    virtual ~FlexView();

    QAbstractItemModel *model() const;
    void setModel(QAbstractItemModel *model);

    QQmlComponent *delegate() const;
    void setDelegate(QQmlComponent *delegate);

    QQmlComponent *section() const;
    void setSection(QQmlComponent *sectionDelegate);

    QString sectionRole() const;
    void setSectionRole(const QString &role);

    QString sizeRole() const;
    void setSizeRole(const QString &role);

    qreal idealHeight() const;
    void setIdealHeight(qreal idealHeight);
    qreal minHeight() const;
    void setMinHeight(qreal minHeight);
    qreal maxHeight() const;
    void setMaxHeight(qreal maxHeight);

    qreal cacheBuffer() const;
    void setCacheBuffer(qreal cacheBuffer);

    int currentIndex() const;
    void setCurrentIndex(int index);
    Q_INVOKABLE bool moveCurrentRow(int delta);
    QQuickItem *currentItem() const;
    QQuickItem *currentSection() const;

    qreal verticalSpacing() const;
    void setVerticalSpacing(qreal spacing);
    qreal horizontalSpacing() const;
    void setHorizontalSpacing(qreal spacing);
    qreal sectionSpacing() const;
    void setSectionSpacing(qreal spacing);

signals:
    void modelChanged();
    void delegateChanged();
    void sectionChanged();
    void sectionRoleChanged();
    void sizeRoleChanged();
    void idealHeightChanged();
    void minHeightChanged();
    void maxHeightChanged();
    void cacheBufferChanged();
    void currentIndexChanged();
    void currentItemChanged();
    void currentSectionChanged();
    void verticalSpacingChanged();
    void horizontalSpacingChanged();
    void sectionSpacingChanged();

protected:
    virtual void componentComplete() override;
    virtual void updatePolish() override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    virtual void geometryChange(const QRectF &newRect, const QRectF &oldRect) override;
#else
    virtual void geometryChanged(const QRectF &newRect, const QRectF &oldRect) override;
#endif

    virtual void viewportMoved(Qt::Orientations orient) override;

private:
    friend class FlexViewPrivate;

    FlexViewPrivate *d = nullptr;
};
