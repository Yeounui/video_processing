#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QtQml/qqmlregistration.h>
#include <cstddef>
#include <vector>

class AlgorithmModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString category READ category WRITE setCategory NOTIFY categoryChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        CategoryRole,
        ParamsRole
    };
    Q_ENUM(Roles)

    explicit AlgorithmModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString category() const;
    void setCategory(const QString &cat);

signals:
    void categoryChanged();

private:
    void rebuild();

    QString category_;
    std::vector<std::size_t> filtered_;
};
