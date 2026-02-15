#include "include/ui/roughstrategyviewermodel.h"

RoughStrategyViewerModel::RoughStrategyViewerModel(TableStrategyModel* tableStrategyModel, QObject *parent):QAbstractItemModel(parent){
    this->tableStrategyModel = tableStrategyModel;
    // This connection is the key to fixing the crash. When the tableStrategyModel is
    // about to be destroyed, it will emit a signal, and this lambda will set our
    // pointer to nullptr, preventing any use-after-free errors.
    connect(tableStrategyModel, &QObject::destroyed, this, [this](){ this->tableStrategyModel = nullptr; });
}

RoughStrategyViewerModel::~RoughStrategyViewerModel()
{
}

QModelIndex RoughStrategyViewerModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    return createIndex(row, column, nullptr);
}

QVariant RoughStrategyViewerModel::headerData(int section, Qt::Orientation orientation, int role){
    return QString::fromStdString("");
}

QModelIndex RoughStrategyViewerModel::parent(const QModelIndex &child) const{
    return QModelIndex();
}

void RoughStrategyViewerModel::onchanged(){
    // Add a guard to prevent emitting signals if the underlying model is gone.
    if (!tableStrategyModel) return;
    emit headerDataChanged(Qt::Horizontal, 0 , columnCount());
}

int RoughStrategyViewerModel::columnCount(const QModelIndex &parent) const
{
    // Add a guard to prevent accessing the deleted model.
    if (!tableStrategyModel) return 0;
    return static_cast<int>(this->tableStrategyModel->total_strategy.size());
}

int RoughStrategyViewerModel::rowCount(const QModelIndex &parent) const
{
    return 1;
}

QVariant RoughStrategyViewerModel::data(const QModelIndex &index, int role) const
{
    // Add a guard to prevent accessing the deleted model.
    if (!tableStrategyModel) return QVariant();

    std::size_t col = index.column();
    if(col < this->tableStrategyModel->total_strategy.size()){
        pair<GameActions,pair<float,float>> one_strategy = this->tableStrategyModel->total_strategy[col];
        return QString::number(one_strategy.second.second);
    }else{
        return "Rough Strategy";
    }
}
