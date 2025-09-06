#ifndef STRATEGYEXPLORER_H
#define STRATEGYEXPLORER_H

#include <QDialog>
#include <QPointer>
#include <QTimer>
#include "include/runtime/qsolverjob.h"
#include "include/ui/tablestrategymodel.h"
#include "include/ui/strategyitemdelegate.h"
#include "include/ui/detailviewermodel.h"
#include "include/ui/detailitemdelegate.h"
#include "include/ui/roughstrategyviewermodel.h"
#include "include/ui/roughstrategyitemdelegate.h"
#include "include/ui/detailwindowsetting.h"
#include <QItemSelection>

namespace Ui {
class StrategyExplorer;
}

class StrategyExplorer : public QDialog
{
    Q_OBJECT

public:
    explicit StrategyExplorer(QWidget *parent, QSolverJob * qSolverJob);
    ~StrategyExplorer();

private slots:
    void item_expanded(const QModelIndex& index);
    void item_clicked(const QModelIndex& index);
    void selection_changed(const QItemSelection &selected,
                           const QItemSelection &deselected);
    void on_turnCardBox_currentIndexChanged(int index);
    void on_riverCardBox_currentIndexChanged(int index);
    void update_second();
    void onMouseMoveEvent(int i,int j);
    void on_strategyModeButtom_clicked();
    void on_ipRangeButtom_clicked();
    void on_oopRangeButtom_clicked();
    void on_evModeButtom_clicked();
    void on_evOnlyModeButtom_clicked();

private:
    Ui::StrategyExplorer *ui;
    QSolverJob * qSolverJob;
    QPointer<TableStrategyModel> tableStrategyModel;
    QPointer<DetailViewerModel> detailViewerModel;
    QPointer<RoughStrategyViewerModel> roughStrategyViewerModel;
    QPointer<QTimer> timer;
    DetailWindowSetting detailWindowSetting;
    vector<Card> cards;
    void process_board(TreeItem* treeitem);
    void process_treeclick(TreeItem* treeitem);
};

#endif // STRATEGYEXPLORER_H