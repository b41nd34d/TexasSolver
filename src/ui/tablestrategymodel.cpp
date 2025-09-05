#include "include/ui/tablestrategymodel.h"
#include "include/nodes/ActionNode.h"
#include "include/solver/Solver.h"
#include "include/runtime/PokerSolver.h"

TableStrategyModel::TableStrategyModel(QSolverJob *qSolverJob, DetailWindowSetting* setting, QObject *parent)
    : QAbstractItemModel(parent), qSolverJob(qSolverJob), detailWindowSetting(setting)
{
    ranklist = QString("A,K,Q,J,T,9,8,7,6,5,4,3,2").split(",");
    if (qSolverJob && qSolverJob->get_solver() && qSolverJob->get_solver()->get_deck()) {
        cardint2card = qSolverJob->get_solver()->get_deck()->getCards();
    }
    build_ui_tables();
}

QVariant TableStrategyModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role == Qt::DisplayRole) {
        // This model primarily holds data, display is handled by the delegate
        return QVariant();
    }

    return QVariant();
}

QModelIndex TableStrategyModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid())
        return QModelIndex();
    return createIndex(row, column);
}

QModelIndex TableStrategyModel::parent(const QModelIndex &index) const
{
    return QModelIndex();
}

int TableStrategyModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return 13;
}

int TableStrategyModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return 13;
}

void TableStrategyModel::setGameTreeNode(TreeItem *item) {
    this->treeItem = item;
}

void TableStrategyModel::updateStrategyData() {
    beginResetModel();
    total_strategy.clear();
    if (!treeItem || !treeItem->m_treedata.lock()) {
        current_strategy.clear();
        current_evs.clear();
        endResetModel();
        return;
    }

    shared_ptr<GameTreeNode> node = treeItem->m_treedata.lock();
    if (node->getType() != GameTreeNode::ACTION) {
        current_strategy.clear();
        current_evs.clear();
        endResetModel();
        return;
    }

    shared_ptr<ActionNode> actionNode = static_pointer_cast<ActionNode>(node);
    current_player = actionNode->getPlayer();

    shared_ptr<Solver> solver = qSolverJob->get_solver()->get_solver();
    if (!solver) {
        current_strategy.clear();
        current_evs.clear();
        endResetModel();
        return;
    }

    vector<Card> chance_cards;
    if (!turnCard.empty()) {
        chance_cards.push_back(turnCard);
    }
    if (!riverCard.empty()) {
        chance_cards.push_back(riverCard);
    }

    current_strategy = solver->get_strategy(actionNode, chance_cards);
    current_evs = solver->get_evs(actionNode, chance_cards);

    ui_strategy_table = (current_player == 0) ? ui_p1_range : ui_p2_range;

    // Calculate the aggregated "total_strategy" for the rough view
    vector<vector<vector<float>>>& ev_data = current_evs;
    vector<vector<vector<float>>>& strat_data = current_strategy;

    // To calculate the full rough strategy, we need both EV and strategy percentages.
    if (strat_data.empty() || ev_data.empty()) {
        endResetModel();
        return;
    }

    vector<GameActions>& actions = actionNode->getActions();
    int player = actionNode->getPlayer();
    PokerSolver* ps = qSolverJob->get_solver();
    if (!ps) {
        endResetModel();
        return;
    }
    const vector<PrivateCards>& range = (player == 0) ? ps->player1Range : ps->player2Range;

    vector<pair<float, float>> summed_values(actions.size(), {0.0f, 0.0f}); // {EV, Strat}
    float total_weight = 0.0f;

    for (const auto& pc : range) {
        if (pc.card1 >= 52 || pc.card2 >= 52 || pc.card1 < 0 || pc.card2 < 0) continue;

        if (strat_data[pc.card1].empty() || strat_data[pc.card1][pc.card2].empty() || ev_data[pc.card1].empty() || ev_data[pc.card1][pc.card2].empty()) continue;
        const auto& hand_strat = strat_data[pc.card1][pc.card2];
        const auto& hand_ev = ev_data[pc.card1][pc.card2];
        if (hand_strat.size() != actions.size() || hand_ev.size() != actions.size()) continue;

        total_weight += pc.weight;
        for (size_t i = 0; i < actions.size(); ++i) {
            summed_values[i].first += hand_ev[i] * pc.weight;      // EV
            summed_values[i].second += hand_strat[i] * pc.weight; // Strategy
        }
    }

    if (total_weight > 0) {
        for (size_t i = 0; i < actions.size(); ++i) {
            summed_values[i].first /= total_weight;  // Average EV
            summed_values[i].second /= total_weight; // Average probability
            total_strategy.push_back({actions[i], summed_values[i]});
        }
    }
    endResetModel();
}

void TableStrategyModel::build_ui_tables() {
    ui_strategy_table.assign(13, vector<vector<pair<int, int>>>(13));
    ui_p1_range.assign(13, vector<vector<pair<int, int>>>(13));
    ui_p2_range.assign(13, vector<vector<pair<int, int>>>(13));
    p1_range.assign(52, vector<float>(52, 0.0f));
    p2_range.assign(52, vector<float>(52, 0.0f));

    for(int i=0; i<13; i++) {
        for(int j=0; j<13; j++) {
            string hand_str;
            if (i < j) { // Suited
                hand_str = ranklist[i].toStdString() + ranklist[j].toStdString() + "s";
            } else if (i > j) { // Offsuit
                hand_str = ranklist[j].toStdString() + ranklist[i].toStdString() + "o";
            } else { // Pair
                hand_str = ranklist[i].toStdString() + ranklist[j].toStdString();
            }
            string2ij[hand_str] = {i, j};
        }
    }

    PokerSolver* ps = qSolverJob->get_solver();
    if (!ps) return;

    const auto& p1_range_vec = ps->player1Range;
    const auto& p2_range_vec = ps->player2Range;

    for(const auto& pc : p1_range_vec) {
        p1_range[pc.card1][pc.card2] = pc.weight;
        p1_range[pc.card2][pc.card1] = pc.weight;
        string hand_str = PrivateCards(pc.card1, pc.card2, 0).toString();
        if (string2ij.count(hand_str)) {
            auto& pos = string2ij[hand_str];
            ui_p1_range[pos.first][pos.second].push_back({pc.card1, pc.card2});
        }
    }

    for(const auto& pc : p2_range_vec) {
        p2_range[pc.card1][pc.card2] = pc.weight;
        p2_range[pc.card2][pc.card1] = pc.weight;
        string hand_str = PrivateCards(pc.card1, pc.card2, 0).toString();
        if (string2ij.count(hand_str)) {
            auto& pos = string2ij[hand_str];
            ui_p2_range[pos.first][pos.second].push_back({pc.card1, pc.card2});
        }
    }
}

vector<float> TableStrategyModel::get_ev_grid(int i, int j) const {
    vector<float> evs;
    if (i < 0 || i >= 13 || j < 0 || j >= 13) return evs;

    const auto& combos = ui_strategy_table[i][j];
    for (const auto& combo : combos) {
        if (current_evs.empty() || static_cast<size_t>(combo.first) >= current_evs.size() || static_cast<size_t>(combo.second) >= current_evs[combo.first].size() || current_evs[combo.first][combo.second].empty()) {
            evs.push_back(0.0f); // Or some indicator for no data
        } else {
            float total_ev = 0.0f;
            for (float ev : current_evs[combo.first][combo.second]) {
                total_ev += ev;
            }
            evs.push_back(total_ev);
        }
    }
    return evs;
}

void TableStrategyModel::setTrunCard(const Card &card) { this->turnCard = card; }
void TableStrategyModel::setRiverCard(const Card &card) { this->riverCard = card; }
Card TableStrategyModel::getTrunCard() const { return this->turnCard; }
Card TableStrategyModel::getRiverCard() const { return this->riverCard; }

QSolverJob *TableStrategyModel::get_qsolverjob() const {
    return qSolverJob;
}

vector<pair<GameActions, float>> TableStrategyModel::get_strategy(int i, int j) const {
    vector<pair<GameActions, float>> avg_strategy;
    if (i < 0 || i >= 13 || j < 0 || j >= 13 || !treeItem || !treeItem->m_treedata.lock()) {
        return avg_strategy;
    }

    shared_ptr<GameTreeNode> node = treeItem->m_treedata.lock();
    if (node->getType() != GameTreeNode::ACTION) {
        return avg_strategy;
    }
    shared_ptr<ActionNode> actionNode = static_pointer_cast<ActionNode>(node);
    vector<GameActions>& actions = actionNode->getActions();

    const auto& combos = ui_strategy_table[i][j];
    if (combos.empty() || current_strategy.empty()) {
        return avg_strategy;
    }

    vector<float> summed_strats(actions.size(), 0.0f);
    int combo_count = 0;

    for (const auto& combo : combos) {
        if (static_cast<size_t>(combo.first) < current_strategy.size() &&
            static_cast<size_t>(combo.second) < current_strategy[combo.first].size() &&
            !current_strategy[combo.first][combo.second].empty()) {

            const auto& hand_strat = current_strategy[combo.first][combo.second];
            if (hand_strat.size() == actions.size()) {
                for (size_t k = 0; k < actions.size(); ++k) {
                    summed_strats[k] += hand_strat[k];
                }
                combo_count++;
            }
        }
    }

    if (combo_count > 0) {
        for (size_t k = 0; k < actions.size(); ++k) {
            avg_strategy.push_back({actions[k], summed_strats[k] / combo_count});
        }
    }

    return avg_strategy;
}

vector<float> TableStrategyModel::get_strategies_evs(int i, int j) const {
    vector<float> avg_evs;
    if (i < 0 || i >= 13 || j < 0 || j >= 13 || !treeItem || !treeItem->m_treedata.lock()) {
        return avg_evs;
    }

    shared_ptr<GameTreeNode> node = treeItem->m_treedata.lock();
    if (node->getType() != GameTreeNode::ACTION) {
        return avg_evs;
    }
    shared_ptr<ActionNode> actionNode = static_pointer_cast<ActionNode>(node);
    vector<GameActions>& actions = actionNode->getActions();

    const auto& combos = ui_strategy_table[i][j];
    if (combos.empty() || current_evs.empty()) {
        return avg_evs;
    }

    vector<float> summed_evs(actions.size(), 0.0f);
    int combo_count = 0;

    for (const auto& combo : combos) {
        if (static_cast<size_t>(combo.first) < current_evs.size() &&
            static_cast<size_t>(combo.second) < current_evs[combo.first].size() &&
            !current_evs[combo.first][combo.second].empty()) {

            const auto& hand_ev = current_evs[combo.first][combo.second];
            if (hand_ev.size() == actions.size()) {
                for (size_t k = 0; k < actions.size(); ++k) {
                    summed_evs[k] += hand_ev[k];
                }
                combo_count++;
            }
        }
    }

    if (combo_count > 0) {
        for (size_t k = 0; k < actions.size(); ++k) {
            avg_evs.push_back(summed_evs[k] / combo_count);
        }
    }

    return avg_evs;
}