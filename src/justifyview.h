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

public:
    JustifyView(QQuickItem *parent = nullptr);
    virtual ~JustifyView();

    QVariant model() const;
    void setModel(const QVariant &model);

    QQmlComponent *delegate() const;
    void setDelegate(QQmlComponent *delegate);

    QString sectionRole() const;
    void setSectionRole(const QString &role);

signals:
    void modelChanged();
    void delegateChanged();
    void sectionRoleChanged();

protected:
    virtual void componentComplete() override;
    virtual void updatePolish() override;
    virtual void geometryChanged(const QRectF &newRect, const QRectF &oldRect) override;

private:
    friend class JustifyViewPrivate;

    JustifyViewPrivate *d = nullptr;
};
