#pragma once

#include <QtQuick/private/qquickflickable_p.h>

class JustifyViewPrivate;
class QQmlComponent;

class JustifyView : public QQuickFlickable
{
    Q_OBJECT

    Q_PROPERTY(QVariant model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QQmlComponent* delegate READ delegate WRITE setDelegate NOTIFY delegateChanged)
    Q_PROPERTY(QString sectionRole READ sectionRole WRITE setSectionRole NOTIFY sectionRoleChanged)
    Q_PROPERTY(qreal idealHeight READ idealHeight WRITE setIdealHeight NOTIFY idealHeightChanged)
    Q_PROPERTY(qreal minHeight READ minHeight WRITE setMinHeight NOTIFY minHeightChanged)
    Q_PROPERTY(qreal maxHeight READ maxHeight WRITE setMaxHeight NOTIFY maxHeightChanged)

public:
    JustifyView(QQuickItem *parent = nullptr);
    virtual ~JustifyView();

    QVariant model() const;
    void setModel(const QVariant &model);

    QQmlComponent *delegate() const;
    void setDelegate(QQmlComponent *delegate);

    QString sectionRole() const;
    void setSectionRole(const QString &role);

    // XXX sizeRole

    qreal idealHeight() const;
    void setIdealHeight(qreal idealHeight);
    qreal minHeight() const;
    void setMinHeight(qreal minHeight);
    qreal maxHeight() const;
    void setMaxHeight(qreal maxHeight);

signals:
    void modelChanged();
    void delegateChanged();
    void sectionRoleChanged();
    void idealHeightChanged();
    void minHeightChanged();
    void maxHeightChanged();

protected:
    virtual void componentComplete() override;
    virtual void updatePolish() override;
    virtual void geometryChanged(const QRectF &newRect, const QRectF &oldRect) override;

private:
    friend class JustifyViewPrivate;

    JustifyViewPrivate *d = nullptr;
};
