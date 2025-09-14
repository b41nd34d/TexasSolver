#ifndef TABLESTRATEGYMODEL_H
#define TABLESTRATEGYMODEL_H

#include <QAbstractItemModel>
#include "include/runtime/qsolverjob.h"
#include "include/ui/treeitem.h"
#include "include/nodes/GameActions.h"
#include "include/ui/detailwindowsetting.h"
#include "include/Card.h"
#include <vector>
#include <utility>

class TableStrategyModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit TableStrategyModel(QSolverJob *qSolverJob, DetailWindowSetting* setting, QObject *parent = nullptr);

    QVariant data(const QModelIndex &index, int role) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    void setGameTreeNode(const weak_ptr<GameTreeNode>& node);
    void updateStrategyData();
    void setTrunCard(const Card& card);
    void setRiverCard(const Card& card);
    Card getTrunCard() const;
    Card getRiverCard() const;
    weak_ptr<GameTreeNode> getCurrentNode() const { return currentNode; }
    vector<pair<GameActions,pair<float,float>>> total_strategy;
    vector<pair<GameActions, float>> get_strategy(int i, int j) const;
    vector<float> get_strategies_evs(int i, int j) const;
    QSolverJob* get_qsolverjob() const { return qSolverJob; }
    const DetailWindowSetting* get_detail_window_setting() const { return detailWindowSetting; }

    // Members needed by delegates
    vector<vector<vector<pair<int, int>>>> ui_strategy_table;
    vector<vector<vector<pair<int, int>>>> ui_p1_range;
    vector<vector<vector<pair<int, int>>>> ui_p2_range;
    vector<vector<vector<float>>> current_strategy;
    vector<vector<vector<float>>> current_evs;
    vector<vector<float>> p1_range;
    vector<vector<float>> p2_range;
    vector<Card> cardint2card;
    int current_player = 0;
    vector<float> get_ev_grid(int i, int j) const;

private:
    QSolverJob *qSolverJob;
    weak_ptr<GameTreeNode> currentNode;
    DetailWindowSetting* detailWindowSetting; // This pointer is owned by StrategyExplorer
    Card turnCard;
    Card riverCard;
    void build_ui_tables();
    map<string, pair<int, int>> string2ij;
    QStringList ranklist;
};

#endif // TABLESTRATEGYMODEL_H