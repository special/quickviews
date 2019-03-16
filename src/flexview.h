#pragma once

#include <QtQuick/private/qquickflickable_p.h>

class FlexViewPrivate;
class QQmlComponent;

class FlexView : public QQuickFlickable
{
    Q_OBJECT

    Q_PROPERTY(QVariant model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QQmlComponent* delegate READ delegate WRITE setDelegate NOTIFY delegateChanged)
    Q_PROPERTY(QQmlComponent* section READ section WRITE setSection NOTIFY sectionChanged)
    Q_PROPERTY(QString sectionRole READ sectionRole WRITE setSectionRole NOTIFY sectionRoleChanged)
    Q_PROPERTY(QString sizeRole READ sizeRole WRITE setSizeRole NOTIFY sizeRoleChanged)
    Q_PROPERTY(qreal idealHeight READ idealHeight WRITE setIdealHeight NOTIFY idealHeightChanged)
    Q_PROPERTY(qreal minHeight READ minHeight WRITE setMinHeight NOTIFY minHeightChanged)
    Q_PROPERTY(qreal maxHeight READ maxHeight WRITE setMaxHeight NOTIFY maxHeightChanged)

public:
    FlexView(QQuickItem *parent = nullptr);
    virtual ~FlexView();

    QVariant model() const;
    void setModel(const QVariant &model);

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

signals:
    void modelChanged();
    void delegateChanged();
    void sectionChanged();
    void sectionRoleChanged();
    void sizeRoleChanged();
    void idealHeightChanged();
    void minHeightChanged();
    void maxHeightChanged();

protected:
    virtual void componentComplete() override;
    virtual void updatePolish() override;
    virtual void geometryChanged(const QRectF &newRect, const QRectF &oldRect) override;

private:
    friend class FlexViewPrivate;

    FlexViewPrivate *d = nullptr;
};
