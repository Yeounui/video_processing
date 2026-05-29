#include "AlgorithmModel.h"
#include "ImageProcessorCore.h"

#include <QVariantList>
#include <QVariantMap>

AlgorithmModel::AlgorithmModel(QObject *parent)
    : QAbstractListModel(parent)
    , category_(QStringLiteral("Point"))
{
    rebuild();
}

int AlgorithmModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return static_cast<int>(filtered_.size());
}

QVariant AlgorithmModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
        return {};

    const auto &spec = ImageProcessorCore::specs().at(filtered_.at(static_cast<std::size_t>(index.row())));

    switch (role) {
    case IdRole:
        return spec.id;
    case NameRole:
    case Qt::DisplayRole:
        return spec.name;
    case CategoryRole:
        return spec.category;
    case ParamsRole: {
        QVariantList params;
        for (const auto &param : spec.params) {
            QVariantMap map;
            map.insert(QStringLiteral("key"), param.key);
            map.insert(QStringLiteral("label"), param.label);
            map.insert(QStringLiteral("valueType"), static_cast<int>(param.valueType));
            map.insert(QStringLiteral("controlType"), static_cast<int>(param.controlType));
            map.insert(QStringLiteral("min"), param.min);
            map.insert(QStringLiteral("max"), param.max);
            map.insert(QStringLiteral("step"), param.step);
            map.insert(QStringLiteral("defaultValue"), param.defaultValue);
            map.insert(QStringLiteral("enumValues"), param.enumValues);
            map.insert(QStringLiteral("isAutomatic"), param.isAutomatic);
            params.append(map);
        }
        return params;
    }
    default:
        return {};
    }
}

QHash<int, QByteArray> AlgorithmModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {NameRole, "name"},
        {CategoryRole, "category"},
        {ParamsRole, "params"}
    };
}

QString AlgorithmModel::category() const
{
    return category_;
}

void AlgorithmModel::setCategory(const QString &cat)
{
    if (category_ == cat)
        return;

    category_ = cat;
    rebuild();
}

void AlgorithmModel::rebuild()
{
    beginResetModel();
    filtered_.clear();

    const auto &specs = ImageProcessorCore::specs();
    for (std::size_t i = 0; i < specs.size(); ++i) {
        if (category_.isEmpty() || specs[i].category == category_)
            filtered_.push_back(i);
    }

    endResetModel();
    emit categoryChanged();
}
