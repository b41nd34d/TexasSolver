#include "include/ui/treemodel.h"

// Constructor and other model methods need to be implemented.
// The following is a minimal implementation based on the context.

TreeModel::TreeModel(QSolverJob *qSolverJob, QObject *parent)
    : QAbstractItemModel(parent), qSolverJob(qSolverJob)
{
    rootItem = new TreeItem(qSolverJob->get_solver()->get_game_tree()->getRoot());
}

TreeModel::~TreeModel()
{
    delete rootItem;
}

QModelIndex TreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    if (!parent.isValid()) {
        // This is a request for a top-level item.
        if (rootItem && row == 0) {
            return createIndex(row, column, rootItem);
        }
        return QModelIndex();
    }

    // This is a request for a child of a valid parent item.
    TreeItem *parentItem = static_cast<TreeItem*>(parent.internalPointer());
    TreeItem *childItem = parentItem->child(row);
    return childItem ? createIndex(row, column, childItem) : QModelIndex();
}

QModelIndex TreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    TreeItem *childItem = static_cast<TreeItem*>(index.internalPointer());
    TreeItem *parentItem = childItem->parentItem();

    if (parentItem == rootItem || !parentItem)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

int TreeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) {
        return 0;
    }
    if (!parent.isValid()) {
        return rootItem ? 1 : 0; // There is one root item.
    }
    TreeItem *parentItem = static_cast<TreeItem*>(parent.internalPointer());
    return parentItem->childCount();
}

bool TreeModel::hasChildren(const QModelIndex &parent) const
{
    // This function is key for lazy-loading. It tells the view whether to draw
    // an expansion indicator, even if the children haven't been fetched yet.

    if (!parent.isValid()) {
        // The invisible root of the model has one child (the game tree's root).
        return rootItem != nullptr;
    }

    TreeItem *item = static_cast<TreeItem*>(parent.internalPointer());
    if (!item) return false;

    // If children are already populated, we can rely on childCount.
    if (item->childCount() > 0) return true;

    // Otherwise, check the underlying data source to see if it can have children.
    shared_ptr<GameTreeNode> node = item->m_treedata.lock();
    if (!node) return false;

    if (node->getType() == GameTreeNode::GameTreeNodeType::ACTION) {
        return !std::dynamic_pointer_cast<ActionNode>(node)->getChildrens().empty();
    } else if (node->getType() == GameTreeNode::GameTreeNodeType::CHANCE) {
        return std::dynamic_pointer_cast<ChanceNode>(node)->getChildren() != nullptr;
    }

    return false; // Terminal/Showdown nodes have no children.
}

int TreeModel::columnCount(const QModelIndex &parent) const
{
    return 1;
}

QVariant TreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole)
        return QVariant();

    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());
    return item->data();
}

Qt::ItemFlags TreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    return QAbstractItemModel::flags(index);
}

QVariant TreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return QVariant(tr("Game Tree"));
    return QVariant();
}

void TreeModel::populate(const QModelIndex &index)
{
    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());
    if (!item || item->childCount() > 0) {
        return; // Already populated
    }

    shared_ptr<GameTreeNode> node = item->m_treedata.lock();
    if (!node) return;

    const auto& locked_nodes = this->qSolverJob->locked_nodes;
    const auto& full_board_situation = this->qSolverJob->full_board_situation;
    bool analysis_mode = full_board_situation.has_value() || !locked_nodes.empty();

    if (node->getType() == GameTreeNode::GameTreeNodeType::ACTION) {
        shared_ptr<ActionNode> actionNode = dynamic_pointer_cast<ActionNode>(node);
        string path = item->getActionPath();
        const auto& actions = actionNode->getActions();
        const auto& childrens = actionNode->getChildrens();
        std::vector<shared_ptr<GameTreeNode>> children_to_add = childrens;

        if (analysis_mode) {
            children_to_add.clear();
            if (!locked_nodes.empty()) {
                // Locked mode: filter children based on rules
                const LockedNode* lock_rule = nullptr;
                for(const auto& rule : locked_nodes) {
                    if (rule.node_path == path && rule.player_to_lock == actionNode->getPlayer()) {
                        lock_rule = &rule;
                        break;
                    }
                }

                for (size_t i = 0; i < actions.size(); ++i) {
                    bool add_child = false;
                    if (lock_rule) {
                        // A rule exists for this specific node, only add actions in the rule
                        Action action_key;
                        switch(actions[i].getAction()) {
                            case GameTreeNode::PokerActions::FOLD: action_key = -1; break;
                            case GameTreeNode::PokerActions::CHECK: case GameTreeNode::PokerActions::CALL: action_key = 0; break;
                            case GameTreeNode::PokerActions::BET: case GameTreeNode::PokerActions::RAISE: action_key = static_cast<Action>(actions[i].getAmount()); break;
                            default: continue;
                        }
                        if (lock_rule->locked_strategy.count(action_key)) {
                            add_child = true;
                        }
                    } else {
                        // No rule for this node, check if it's on a path to a future locked node
                        string child_path = path + actions[i].toString() + "/";
                        for (const auto& rule : locked_nodes) {
                            if (rule.node_path.rfind(child_path, 0) == 0) {
                                add_child = true;
                                break;
                            }
                        }
                    }
                    if (add_child) {
                        children_to_add.push_back(childrens[i]);
                    }
                }
            } else if (full_board_situation.has_value()) {
                // Full board analysis without locks, show the whole tree
                children_to_add = childrens;
            }
        }

        // Add the filtered children to the model
        if (!children_to_add.empty()) {
            beginInsertRows(index, 0, children_to_add.size() - 1);
            for (const auto& child_node : children_to_add) {
                // Defensive check to ensure child's parent pointer is correct.
                if (!child_node->getParent() || child_node->getParent() != node) {
                    qWarning() << "Detected a child with an incorrect parent pointer. Skipping.";
                    continue;
                }
                // Defensive check to prevent infinite loops from cyclic trees
                if (child_node == node) {
                    qWarning() << "Detected a cycle in the game tree. Skipping child.";
                    continue;
                }
                item->insertChild(new TreeItem(child_node, item));
            }
            endInsertRows();
        }
    } else if (node->getType() == GameTreeNode::GameTreeNodeType::CHANCE) {
        shared_ptr<ChanceNode> chanceNode = dynamic_pointer_cast<ChanceNode>(node);
        shared_ptr<GameTreeNode> childNode = chanceNode->getChildren();
        // Also check for cycles here
        if (childNode && childNode != node) {
            beginInsertRows(index, 0, 0);
            item->insertChild(new TreeItem(childNode, item));
            endInsertRows();
        }
    }
}