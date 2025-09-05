#include "strategyexplorer.h"
#include "ui_strategyexplorer.h"
#include "qstandarditemmodel.h"
#include <QComboBox>
#include "include/Card.h"

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
    delete this->delegate_strategy;
    delete this->tableStrategyModel;
    delete this->detailViewerModel;
    delete this->roughStrategyViewerModel;
    delete this->timer;
}

void StrategyExplorer::initializeView() {
    // Clear previous state
    ui->turnCardBox->clear();
    ui->riverCardBox->clear();
    this->cards.clear();

    if (qSolverJob->analysis_mode == QSolverJob::AnalysisMode::HAND_ANALYSIS) {
        // In hand analysis mode, lock the turn and river cards to the specific hand.
        vector<string> board_cards = string_split(qSolverJob->full_board, ',');

        if (board_cards.size() >= 4) {
            Card turn_card(board_cards[3]);
            ui->turnCardBox->addItem(QString::fromStdString(turn_card.toFormattedString()));
            ui->turnCardBox->setCurrentIndex(0);
            ui->turnCardBox->setEnabled(false);
            this->tableStrategyModel->setTrunCard(turn_card);
        }
        if (board_cards.size() == 5) {
            Card river_card(board_cards[4]);
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
    for(const string& one_board_str:board_str_arr){
        local_cards.push_back(Card(one_board_str));
    }
    if(treeitem != nullptr){
        if(treeitem->m_treedata.lock()->getRound() == GameTreeNode::GameRound::TURN && !this->tableStrategyModel->getTrunCard().empty()){
            local_cards.push_back(Card(this->tableStrategyModel->getTrunCard()));
        }
        else if(treeitem->m_treedata.lock()->getRound() == GameTreeNode::GameRound::RIVER){
            if(!this->tableStrategyModel->getTrunCard().empty())
                local_cards.push_back(Card(this->tableStrategyModel->getTrunCard()));
            if(!this->tableStrategyModel->getRiverCard().empty())
                local_cards.push_back(Card(this->tableStrategyModel->getRiverCard()));
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
