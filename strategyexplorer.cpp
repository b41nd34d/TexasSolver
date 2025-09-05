#include "strategyexplorer.h"
#include "ui_strategyexplorer.h"
#include "qstandarditemmodel.h"
#include <QComboBox>
#include "include/Card.h"
#include "include/solver/PCfrSolver.h"

StrategyExplorer::StrategyExplorer(QWidget *parent,QSolverJob * qSolverJob) :
    QDialog(parent),
    ui(new Ui::StrategyExplorer)
{
    this->qSolverJob = qSolverJob;
    this->detailWindowSetting = DetailWindowSetting();
    ui->setupUi(this);
    /*
    QStandardItemModel* model = new QStandardItemModel();
    for (int row = 0; row < 4; ++row) {
         QStandardItem *item = new QStandardItem(QString("%1").arg(row) );
         model->appendRow( item );
    }
    this->ui->gameTreeView->setModel(model);
    */
    // Initial Game Tree preview panel
    this->ui->gameTreeView->setTreeData(qSolverJob);
    connect(
                this->ui->gameTreeView,
                SIGNAL(expanded(const QModelIndex&)),
                this,
                SLOT(item_expanded(const QModelIndex&))
                );
    connect(
                this->ui->gameTreeView,
                SIGNAL(clicked(const QModelIndex&)),
                this,
                SLOT(item_clicked(const QModelIndex&))
                );

    // Initize strategy(rough) table
    this->tableStrategyModel = new TableStrategyModel(this->qSolverJob, &(this->detailWindowSetting), this);
    this->ui->strategyTableView->setModel(this->tableStrategyModel);
    this->delegate_strategy = new StrategyItemDelegate(this->qSolverJob,&(this->detailWindowSetting),this);
    this->ui->strategyTableView->setItemDelegate(this->delegate_strategy);

    // Initialize turn and river card selectors based on solver mode
    initializeView();

    // Initize timer for strategy auto update
    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(update_second()));
    timer->start(1000);

    // On mouse event of strategy table
    connect(this->ui->strategyTableView,SIGNAL(itemMouseChange(int,int)),this,SLOT(onMouseMoveEvent(int,int)));

    // Initize Detail Viewer window
    this->detailViewerModel = new DetailViewerModel(this->tableStrategyModel,this);
    this->ui->detailView->setModel(this->detailViewerModel);
    this->detailItemItemDelegate = new DetailItemDelegate(&(this->detailWindowSetting),this);
    this->ui->detailView->setItemDelegate(this->detailItemItemDelegate);

    // Initize Rough Strategy Viewer
    this->roughStrategyViewerModel = new RoughStrategyViewerModel(this->tableStrategyModel,this);
    this->ui->roughStrategyView->setModel(this->roughStrategyViewerModel);
    this->roughStrategyItemDelegate = new RoughStrategyItemDelegate(&(this->detailWindowSetting),this);
    this->ui->roughStrategyView->setItemDelegate(this->roughStrategyItemDelegate);
}

StrategyExplorer::~StrategyExplorer()
{
    delete ui;
}

void StrategyExplorer::initializeView() {
    // Clear previous state
    ui->turnCardBox->clear();
    ui->riverCardBox->clear();
    this->cards.clear();

    if (qSolverJob->analysis_mode == QSolverJob::AnalysisMode::HAND_ANALYSIS) {
        // In hand analysis mode, we must get the card objects directly from the solver
        // to ensure they are fully initialized with their correct deck index.
        // Creating them from a string here would result in "detached" cards that
        // cause errors when their deck index is requested.
        shared_ptr<PCfrSolver> solver = dynamic_pointer_cast<PCfrSolver>(qSolverJob->get_solver()->get_solver());
        if (!solver) {
            qDebug() << "Error: Could not get PCfrSolver instance in hand analysis mode.";
            return;
        }

        // This uses the new public getter added to PCfrSolver.h
        const vector<Card>& solver_board_cards = solver->get_full_board_cards();

        if (solver_board_cards.size() >= 4) {
            const Card& turn_card = solver_board_cards[3];
            this->cards.push_back(turn_card); // Store a copy for local use
            ui->turnCardBox->addItem(QString::fromStdString(turn_card.toFormattedString()));
            ui->turnCardBox->setCurrentIndex(0);
            ui->turnCardBox->setEnabled(false);
            this->tableStrategyModel->setTrunCard(turn_card);
        }
        if (solver_board_cards.size() == 5) {
            const Card& river_card = solver_board_cards[4];
            this->cards.push_back(river_card); // Store a copy for local use
            ui->riverCardBox->addItem(QString::fromStdString(river_card.toFormattedString()));
            ui->riverCardBox->setCurrentIndex(0);
            ui->riverCardBox->setEnabled(false);
            this->tableStrategyModel->setRiverCard(river_card);
        }
    } else {
        // In standard mode, populate with all possible cards.
        Deck* deck = this->qSolverJob->get_solver()->get_deck();
        QString board_qstring = QString::fromStdString(this->qSolverJob->board);

        for(const auto& one_card : deck->getCards()){
            if(board_qstring.contains(QString::fromStdString(one_card.getCard()))) continue;

            this->cards.push_back(one_card);
            QString card_str_formatted = QString::fromStdString(one_card.toFormattedString());
            this->ui->turnCardBox->addItem(card_str_formatted);
            this->ui->riverCardBox->addItem(card_str_formatted);

            int new_index = this->ui->turnCardBox->count() - 1;
            if(card_str_formatted.contains(QString::fromLocal8Bit("♦️")) || card_str_formatted.contains(QString::fromLocal8Bit("♥️️"))){
                this->ui->turnCardBox->setItemData(new_index, QBrush(Qt::red), Qt::ForegroundRole);
                this->ui->riverCardBox->setItemData(new_index, QBrush(Qt::red), Qt::ForegroundRole);
            } else {
                this->ui->turnCardBox->setItemData(new_index, QBrush(Qt::black), Qt::ForegroundRole);
                this->ui->riverCardBox->setItemData(new_index, QBrush(Qt::black), Qt::ForegroundRole);
            }
        }

        GameTreeNode::GameRound round = this->qSolverJob->get_solver()->getGameTree()->getRoot()->getRound();
        if (round == GameTreeNode::GameRound::FLOP) {
            if (this->cards.size() > 0) {
                this->tableStrategyModel->setTrunCard(this->cards[0]);
                this->ui->turnCardBox->setCurrentIndex(0);
            }
            if (this->cards.size() > 1) {
                this->tableStrategyModel->setRiverCard(this->cards[1]);
                this->ui->riverCardBox->setCurrentIndex(1);
            }
        } else if (round == GameTreeNode::GameRound::TURN) {
            this->ui->turnCardBox->setEnabled(false);
            if (!this->cards.empty()) {
                this->tableStrategyModel->setRiverCard(this->cards[0]);
                this->ui->riverCardBox->setCurrentIndex(0);
            }
        } else if (round == GameTreeNode::GameRound::RIVER) {
            this->ui->turnCardBox->setEnabled(false);
            this->ui->riverCardBox->setEnabled(false);
        }
    }
}

void StrategyExplorer::item_expanded(const QModelIndex& index){
    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());
    int num_child = item->childCount();
    for (int i = 0;i < num_child;i ++){
        TreeItem* one_child = item->child(i);
        if(one_child->childCount() != 0)continue;
        this->ui->gameTreeView->tree_model->reGenerateTreeItem(one_child->m_treedata.lock()->getRound(),one_child);
    }
}

void StrategyExplorer::process_board(const TreeItem* treeitem){
    vector<string> board_str_arr = string_split(this->qSolverJob->board,',');
    vector<Card> local_cards;

    // Get the initial board cards (e.g. flop) by looking them up in the deck
    // to ensure they are valid "attached" cards with a deck index.
    Deck* deck = this->qSolverJob->get_solver()->get_deck();
    for(const string& one_board_str : board_str_arr) {
        bool found = false;
        for(const Card& deck_card : deck->getCards()){
            if(deck_card.getCard() == one_board_str){
                local_cards.push_back(deck_card);
                found = true;
                break;
            }
        }
        if (!found) {
            qDebug() << "Warning: board card not found in deck in process_board: " << QString::fromStdString(one_board_str);
        }
    }

    if(treeitem != nullptr){
        if(treeitem->m_treedata.lock()->getRound() == GameTreeNode::GameRound::TURN && !this->tableStrategyModel->getTrunCard().empty()){
            local_cards.push_back(this->tableStrategyModel->getTrunCard());
        }
        else if(treeitem->m_treedata.lock()->getRound() == GameTreeNode::GameRound::RIVER){
            if(!this->tableStrategyModel->getTrunCard().empty())
                local_cards.push_back(this->tableStrategyModel->getTrunCard());
            if(!this->tableStrategyModel->getRiverCard().empty())
                local_cards.push_back(this->tableStrategyModel->getRiverCard());
        }
    }
    this->ui->boardLabel->setText(QString("<b>%1: </b>").arg(tr("board")) + Card::boardCards2html(local_cards));
}

void StrategyExplorer::process_treeclick(const TreeItem* treeitem){
    shared_ptr<GameTreeNode> treenode = treeitem->m_treedata.lock();
    if(treenode->getType() == GameTreeNode::GameTreeNodeType::ACTION){
        shared_ptr<ActionNode> actionnode = static_pointer_cast<ActionNode>(treenode);
        QString action_str = QString("<b>%1 %2</b>").arg(actionnode->getPlayer() == 0?tr("IP"):tr("OOP"),tr(" decision node"));
        this->ui->nodeDisplayLabel->setText(action_str);
    }
    else if(treenode->getType() == GameTreeNode::GameTreeNodeType::CHANCE){
        QString chance_str = tr("<b>Chance node</b>");
        this->ui->nodeDisplayLabel->setText(chance_str);
    }
    else if(treenode->getType() == GameTreeNode::GameTreeNodeType::TERMINAL){
        QString terminal_str = tr("<b>Terminal node</b>");
        this->ui->nodeDisplayLabel->setText(terminal_str);
    }
    else if(treenode->getType() == GameTreeNode::GameTreeNodeType::SHOWDOWN){
        QString showdown_str = tr("<b>Showdown node</b>");
        this->ui->nodeDisplayLabel->setText(showdown_str);
    }
}

void StrategyExplorer::item_clicked(const QModelIndex& index){
    try{
        TreeItem * treeNode = static_cast<TreeItem*>(index.internalPointer());
        this->process_treeclick(treeNode);
        this->process_board(treeNode);
        this->tableStrategyModel->setGameTreeNode(treeNode);
        this->tableStrategyModel->updateStrategyData();
        this->ui->strategyTableView->viewport()->update();
        this->roughStrategyViewerModel->onchanged();
        this->ui->roughStrategyView->triger_resize();
        this->ui->roughStrategyView->viewport()->update();
    }
    catch (const runtime_error& error)
    {
        qDebug().noquote() << tr("Encountering error:") << error.what();
    }
}

void StrategyExplorer::selection_changed(const QItemSelection &selected,
                                         const QItemSelection &deselected){
}

void StrategyExplorer::on_turnCardBox_currentIndexChanged(int index)
{
    if (this->qSolverJob->analysis_mode == QSolverJob::AnalysisMode::HAND_ANALYSIS) {
        // In hand analysis mode, the card is fixed and set during initialization.
        // The combo box is disabled, so this should not be triggered by the user.
        // The existing handler logic is incorrect for this mode.
        return;
    }
    if(index >= 0 && static_cast<size_t>(index) < this->cards.size()){
        this->tableStrategyModel->setTrunCard(this->cards[index]);
        this->tableStrategyModel->updateStrategyData();
        // TODO this somehow cause bugs, crashes, why?
        //this->roughStrategyViewerModel->onchanged();
        //this->ui->roughStrategyView->viewport()->update();
        this->process_board(this->tableStrategyModel->treeItem);
    }
    this->ui->strategyTableView->viewport()->update();
    this->ui->detailView->viewport()->update();
}

void StrategyExplorer::on_riverCardBox_currentIndexChanged(int index)
{
    if (this->qSolverJob->analysis_mode == QSolverJob::AnalysisMode::HAND_ANALYSIS) {
        // In hand analysis mode, the card is fixed and set during initialization.
        // The combo box is disabled, so this should not be triggered by the user.
        // The existing handler logic is incorrect for this mode.
        return;
    }
    if(index >= 0 && static_cast<size_t>(index) < this->cards.size()){
        this->tableStrategyModel->setRiverCard(this->cards[index]);
        this->tableStrategyModel->updateStrategyData();
        //this->roughStrategyViewerModel->onchanged();
        //this->ui->roughStrategyView->viewport()->update();
        this->process_board(this->tableStrategyModel->treeItem);
    }
    this->ui->strategyTableView->viewport()->update();
    this->ui->detailView->viewport()->update();
}

void StrategyExplorer::update_second(){
    // This timer is for auto-refreshing the view, for example if the solver is running in the background.
    // A full data update is expensive, so we just trigger a repaint.
    this->roughStrategyViewerModel->onchanged();
    this->ui->roughStrategyView->viewport()->update();
    this->ui->strategyTableView->viewport()->update();
    this->ui->detailView->viewport()->update();
}

void StrategyExplorer::onMouseMoveEvent(int i,int j){
    this->detailWindowSetting.grid_i = i;
    this->detailWindowSetting.grid_j = j;

    // --- DEBUG START ---
    if (i >= 0 && j >= 0) {
        const auto& combos = this->tableStrategyModel->ui_strategy_table[i][j];
        qDebug() << "[DEBUG] onMouseMoveEvent for cell (" << i << "," << j << "). Found" << combos.size() << "combos.";
        if (!combos.empty()) {
            const auto& first_combo = combos[0];
            const auto& card_map = this->tableStrategyModel->cardint2card;
            if (static_cast<size_t>(first_combo.first) < card_map.size() && static_cast<size_t>(first_combo.second) < card_map.size()) {
                const Card& c1 = card_map[first_combo.first];
                const Card& c2 = card_map[first_combo.second];
                qDebug() << "  - First combo:" << c1.toString().c_str() << c2.toString().c_str()
                         << "(ints:" << first_combo.first << "," << first_combo.second << ")";
                qDebug() << "  - Card 1 empty:" << c1.empty() << ", Card 2 empty:" << c2.empty();
            } else {
                qDebug() << "  - First combo card ints out of range for card_map.";
            }
        }
    }
    // --- DEBUG END ---

    this->ui->detailView->viewport()->update();
    this->ui->strategyTableView->viewport()->update();
}

void StrategyExplorer::on_strategyModeButtom_clicked()
{
    this->detailWindowSetting.mode = DetailWindowSetting::DetailWindowMode::STRATEGY;
    this->tableStrategyModel->updateStrategyData();
    this->ui->strategyTableView->viewport()->update();
    this->ui->detailView->viewport()->update();
    this->roughStrategyViewerModel->onchanged();
    this->ui->roughStrategyView->viewport()->update();
}

void StrategyExplorer::on_ipRangeButtom_clicked()
{
    this->detailWindowSetting.mode = DetailWindowSetting::DetailWindowMode::RANGE_IP;
    this->tableStrategyModel->updateStrategyData();
    this->ui->strategyTableView->viewport()->update();
    this->ui->detailView->viewport()->update();
    this->roughStrategyViewerModel->onchanged();
    this->ui->roughStrategyView->viewport()->update();
}

void StrategyExplorer::on_oopRangeButtom_clicked()
{
    this->detailWindowSetting.mode = DetailWindowSetting::DetailWindowMode::RANGE_OOP;
    this->tableStrategyModel->updateStrategyData();
    this->ui->strategyTableView->viewport()->update();
    this->ui->detailView->viewport()->update();
    this->roughStrategyViewerModel->onchanged();
    this->ui->roughStrategyView->viewport()->update();
}

void StrategyExplorer::on_evModeButtom_clicked()
{
    this->detailWindowSetting.mode = DetailWindowSetting::DetailWindowMode::EV;
    this->tableStrategyModel->updateStrategyData();
    this->ui->strategyTableView->viewport()->update();
    this->ui->detailView->viewport()->update();
    this->roughStrategyViewerModel->onchanged();
    this->ui->roughStrategyView->viewport()->update();
}

void StrategyExplorer::on_evOnlyModeButtom_clicked()
{
    this->detailWindowSetting.mode = DetailWindowSetting::DetailWindowMode::EV_ONLY;
    this->tableStrategyModel->updateStrategyData();
    this->ui->strategyTableView->viewport()->update();
    this->ui->detailView->viewport()->update();
    this->roughStrategyViewerModel->onchanged();
    this->ui->roughStrategyView->viewport()->update();
}
