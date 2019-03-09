#pragma once

#include <QtQuick/private/qquickflickable_p.h>

class JustifyViewPrivate;
class QQmlComponent;

class JustifyView : public QQuickFlickable
{
    Q_OBJECT

    Q_PROPERTY(QVariant model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QQmlComponent* delegate READ delegate WRITE setDelegate NOTIFY delegateChanged)

public:
    JustifyView(QQuickItem *parent = nullptr);
    virtual ~JustifyView();

    QVariant model() const;
    void setModel(const QVariant &model);

    QQmlComponent *delegate() const;
    void setDelegate(QQmlComponent *delegate);

signals:
    void modelChanged();
    void delegateChanged();

protected:
    virtual void componentComplete() override;
    virtual void updatePolish() override;

private:
    friend class JustifyViewPrivate;

    JustifyViewPrivate *d = nullptr;
};
