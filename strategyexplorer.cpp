#include "strategyexplorer.h"
#include "ui_strategyexplorer.h" // This must be after strategyexplorer.h
#include "qstandarditemmodel.h"
#include <QBrush>
#include <QApplication>
#include <QComboBox>
#include <QColor>
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
    this->tableStrategyModel = new TableStrategyModel(this->qSolverJob,this);
    this->ui->strategyTableView->setModel(this->tableStrategyModel);
    auto delegate_strategy = new StrategyItemDelegate(this->qSolverJob, &(this->detailWindowSetting), this);
    this->ui->strategyTableView->setItemDelegate(delegate_strategy);

    Deck* deck = this->qSolverJob->get_solver()->get_deck();
    this->ui->turnCardBox->addItem(tr("None"));
    this->ui->riverCardBox->addItem(tr("None"));
    int index = 0;
    QString board_qstring = QString::fromStdString(this->qSolverJob->board);
    for(Card one_card: deck->getCards()){
        if(board_qstring.contains(QString::fromStdString(one_card.toString())))continue;
        QString card_str_formatted = QString::fromStdString(one_card.toFormattedString());
        this->ui->turnCardBox->addItem(card_str_formatted);
        this->ui->riverCardBox->addItem(card_str_formatted);

        if(card_str_formatted.contains(QString::fromUtf8("♦️")) ||
                card_str_formatted.contains(QString::fromUtf8("♥️️"))){
            this->ui->turnCardBox->setItemData(index + 1, QBrush(Qt::red),Qt::ForegroundRole);
            this->ui->riverCardBox->setItemData(index + 1, QBrush(Qt::red),Qt::ForegroundRole);
        }else{
            this->ui->turnCardBox->setItemData(index + 1, QBrush(Qt::black),Qt::ForegroundRole);
            this->ui->riverCardBox->setItemData(index + 1, QBrush(Qt::black),Qt::ForegroundRole);
        }

        this->cards.push_back(one_card);
        index += 1;
    }
    if(this->qSolverJob->get_solver()->getGameTree()->getRoot()->getRound() == GameTreeNode::GameRound::TURN){
        this->ui->turnCardBox->setEnabled(false);
    }
    else if(this->qSolverJob->get_solver()->getGameTree()->getRoot()->getRound() == GameTreeNode::GameRound::RIVER){
        this->ui->turnCardBox->setEnabled(false);
        this->ui->riverCardBox->setEnabled(false);
    }

    // If in full board analysis mode, pre-select the turn/river cards and disable the dropdowns.
    if (this->qSolverJob->full_board_situation.has_value()) {
        const auto& situation = this->qSolverJob->full_board_situation.value();
        if (situation.board_cards.size() == 5) {
            int turn_card_int = -1;
            int river_card_int = -1;

            GameTreeNode::GameRound root_round = this->qSolverJob->get_solver()->getGameTree()->getRoot()->getRound();

            if (root_round == GameTreeNode::GameRound::FLOP) {
                turn_card_int = situation.board_cards[3];
                river_card_int = situation.board_cards[4];
                this->ui->turnCardBox->setEnabled(false);
                this->ui->riverCardBox->setEnabled(false);
            } else if (root_round == GameTreeNode::GameRound::TURN) {
                river_card_int = situation.board_cards[4];
                this->ui->riverCardBox->setEnabled(false);
            }

            // Find and set the turn card index in the dropdown
            if (turn_card_int != -1) {
                for (int i = 0; i < this->cards.size(); ++i) {
                    if (this->cards[i].getCardInt() == turn_card_int) {
                        this->ui->turnCardBox->setCurrentIndex(i + 1); // +1 for "None"
                        break;
                    }
                }
            }

            // Find and set the river card index in the dropdown
            if (river_card_int != -1) {
                for (int i = 0; i < this->cards.size(); ++i) {
                    if (this->cards[i].getCardInt() == river_card_int) {
                        this->ui->riverCardBox->setCurrentIndex(i + 1); // +1 for "None"
                        break;
                    }
                }
            }
        }
    }

    // Initize timer for strategy auto update
    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(update_second()));
    timer->start(1000);

    // On mouse event of strategy table
    connect(this->ui->strategyTableView,SIGNAL(itemMouseChange(int,int)),this,SLOT(onMouseMoveEvent(int,int)));

    // Initialize Detail Viewer window
    this->detailViewerModel = new DetailViewerModel(this->tableStrategyModel,this);
    this->ui->detailView->setModel(this->detailViewerModel);
    auto detailItemItemDelegate = new DetailItemDelegate(&(this->detailWindowSetting),this);
    this->ui->detailView->setItemDelegate(detailItemItemDelegate);

    // Initize Rough Strategy Viewer
    this->roughStrategyViewerModel = new RoughStrategyViewerModel(this->tableStrategyModel,this);
    this->ui->roughStrategyView->setModel(this->roughStrategyViewerModel);
    auto roughStrategyItemDelegate = new RoughStrategyItemDelegate(&(this->detailWindowSetting), this);
    this->ui->roughStrategyView->setItemDelegate(roughStrategyItemDelegate);

    // Programmatically select and click the root node to show the initial strategy.
    QModelIndex rootIndex = this->ui->gameTreeView->tree_model->index(0, 0, QModelIndex());
    if (rootIndex.isValid()) {
        this->ui->gameTreeView->setCurrentIndex(rootIndex);
        item_clicked(rootIndex);
    }
}

StrategyExplorer::~StrategyExplorer()
{
    // The timer is a child of this QDialog, so it will be deleted automatically
    // by Qt's parent-child mechanism. Explicitly stopping it is still good
    // practice to prevent signals from being processed during destruction.
    if (timer) timer->stop();
    delete ui;
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

void StrategyExplorer::process_board(TreeItem* treeitem){
    vector<string> board_str_arr = string_split(this->qSolverJob->board,',');
    vector<Card> cards;
    for(string one_board_str:board_str_arr){
        cards.push_back(Card(one_board_str));
    }
    if(treeitem != NULL && tableStrategyModel){
        if(treeitem->m_treedata.lock()->getRound() == GameTreeNode::GameRound::TURN && !this->tableStrategyModel->getTurnCard().empty()) {
            cards.push_back(this->tableStrategyModel->getTurnCard());
        }
        else if(treeitem->m_treedata.lock()->getRound() == GameTreeNode::GameRound::RIVER){
            if(!this->tableStrategyModel->getTurnCard().empty())
                cards.push_back(this->tableStrategyModel->getTurnCard());
            if(!this->tableStrategyModel->getRiverCard().empty())
                cards.push_back(this->tableStrategyModel->getRiverCard());
        }
    }
    this->ui->boardLabel->setText(QString("<b>%1: </b>").arg(tr("board")) + Card::boardCards2html(cards));
}

void StrategyExplorer::process_treeclick(TreeItem* treeitem){
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
        if (tableStrategyModel) {
            tableStrategyModel->setGameTreeNode(treeNode);
            tableStrategyModel->updateStrategyData();
            ui->strategyTableView->viewport()->update();
        }
        if (roughStrategyViewerModel) roughStrategyViewerModel->onchanged();
        if (ui) {
            ui->roughStrategyView->triger_resize();
            ui->roughStrategyView->viewport()->update();
        }
    }
    catch (const runtime_error& error)
    {
        qDebug().noquote() << tr("Encountering error:");//.toStdString() << endl;
        qDebug().noquote() << error.what() << "\n";
    }
}

void StrategyExplorer::selection_changed(const QItemSelection &selected,
                                         const QItemSelection &deselected){
}

void StrategyExplorer::on_turnCardBox_currentIndexChanged(int index)
{
    if (!tableStrategyModel) return;

    if (index == 0) { // "None" selected
        this->tableStrategyModel->setTurnCard(Card());
    } else if (this->cards.size() > 0 && (index - 1) < this->cards.size()){
        this->tableStrategyModel->setTurnCard(this->cards[index - 1]);
    }

    this->tableStrategyModel->updateStrategyData();
    if (roughStrategyViewerModel) this->roughStrategyViewerModel->onchanged();
    if (ui) this->ui->roughStrategyView->viewport()->update();
    this->process_board(this->tableStrategyModel->treeItem);
    if (ui) {
        this->ui->strategyTableView->viewport()->update();
        this->ui->detailView->viewport()->update();
    }
}

void StrategyExplorer::on_riverCardBox_currentIndexChanged(int index)
{
    if (!tableStrategyModel) return;

    if (index == 0) { // "None" selected
        this->tableStrategyModel->setRiverCard(Card());
    } else if(this->cards.size() > 0  && (index - 1) < this->cards.size()){
        this->tableStrategyModel->setRiverCard(this->cards[index - 1]);
    }

    this->tableStrategyModel->updateStrategyData();
    if (roughStrategyViewerModel) this->roughStrategyViewerModel->onchanged();
    if (ui) this->ui->roughStrategyView->viewport()->update();
    this->process_board(this->tableStrategyModel->treeItem);
    if (ui) {
        this->ui->strategyTableView->viewport()->update();
        this->ui->detailView->viewport()->update();
    }
}

void StrategyExplorer::update_second(){
    // Add guards to all members accessed in this slot, as it can be called
    // during the object's destruction sequence.
    if(tableStrategyModel && tableStrategyModel->treeItem != nullptr){
        this->tableStrategyModel->updateStrategyData();
        if (roughStrategyViewerModel) roughStrategyViewerModel->onchanged();
        if (ui) ui->roughStrategyView->viewport()->update();
        this->process_board(this->tableStrategyModel->treeItem);
    }
    if (ui) {
        if (ui->strategyTableView) ui->strategyTableView->viewport()->update();
        if (ui->detailView) ui->detailView->viewport()->update();
    }
}


void StrategyExplorer::onMouseMoveEvent(int i,int j){
    this->detailWindowSetting.grid_i = i;
    this->detailWindowSetting.grid_j = j;
    if (ui) {
        if (ui->detailView) ui->detailView->viewport()->update();
        if (ui->strategyTableView) ui->strategyTableView->viewport()->update();
    }
}

void StrategyExplorer::on_strategyModeButtom_clicked()
{
    this->detailWindowSetting.mode = DetailWindowSetting::DetailWindowMode::STRATEGY;
    if (ui) {
        if (ui->strategyTableView) ui->strategyTableView->viewport()->update();
        if (ui->detailView) ui->detailView->viewport()->update();
    }
    if (roughStrategyViewerModel) roughStrategyViewerModel->onchanged();
    if (ui) ui->roughStrategyView->viewport()->update();
}

void StrategyExplorer::on_ipRangeButtom_clicked()
{
    this->detailWindowSetting.mode = DetailWindowSetting::DetailWindowMode::RANGE_IP;
    if (ui) {
        if (ui->strategyTableView) ui->strategyTableView->viewport()->update();
        if (ui->detailView) ui->detailView->viewport()->update();
    }
    if (roughStrategyViewerModel) roughStrategyViewerModel->onchanged();
    if (ui) ui->roughStrategyView->viewport()->update();
}

void StrategyExplorer::on_oopRangeButtom_clicked()
{
    this->detailWindowSetting.mode = DetailWindowSetting::DetailWindowMode::RANGE_OOP;
    if (ui) {
        if (ui->strategyTableView) ui->strategyTableView->viewport()->update();
        if (ui->detailView) ui->detailView->viewport()->update();
    }
    if (roughStrategyViewerModel) roughStrategyViewerModel->onchanged();
    if (ui) ui->roughStrategyView->viewport()->update();
}

void StrategyExplorer::on_evModeButtom_clicked()
{
    this->detailWindowSetting.mode = DetailWindowSetting::DetailWindowMode::EV;
    if (ui) {
        if (ui->strategyTableView) ui->strategyTableView->viewport()->update();
        if (ui->detailView) ui->detailView->viewport()->update();
        if (ui->roughStrategyView) ui->roughStrategyView->viewport()->update();
    }
}

void StrategyExplorer::on_evOnlyModeButtom_clicked()
{
    this->detailWindowSetting.mode = DetailWindowSetting::DetailWindowMode::EV_ONLY;
    if (ui) {
        if (ui->strategyTableView) ui->strategyTableView->viewport()->update();
        if (ui->detailView) ui->detailView->viewport()->update();
    }
    if (roughStrategyViewerModel) roughStrategyViewerModel->onchanged();
    if (ui) ui->roughStrategyView->viewport()->update();
}
