//
// Created by Xuefeng Huang on 2020/1/31.
//

#ifndef TEXASSOLVER_SOLVER_H
#define TEXASSOLVER_SOLVER_H


#include <include/GameTree.h>
#include <optional>
#include "solver_options.h" // For LockedNode

class Solver {
public:
    enum MonteCarolAlg {
        NONE,
        PUBLIC
    };
    /**
     * @brief For analyzing specific board runouts, you should first construct a
     * specialized GameTree. This tree will be much smaller as it won't have
     * chance nodes for the board cards. Then, pass this pruned tree to the
     * solver's constructor.
     *
     * @brief For node locking, pass a vector of LockedNode objects to the train()
     * method to force specific strategies at certain game nodes.
     */
    Solver();
    Solver(shared_ptr<GameTree> tree);
    shared_ptr<GameTree> getTree();

    // Default train methods that delegate to the full version.
    virtual void train() { train({}, std::nullopt); }
    virtual void train(const vector<LockedNode>& locked_nodes) { train(locked_nodes, std::nullopt); }
    // Train with node locking and/or a specific full board runout.
    // This must be implemented by concrete solver classes.
    virtual void train(const vector<LockedNode>& locked_nodes, const std::optional<FullBoardSituation>& full_board) = 0;

    virtual void stop() = 0;
    virtual json dumps(bool with_status,int depth) = 0;
    virtual vector<vector<vector<float>>> get_strategy(shared_ptr<ActionNode> node,vector<Card> cards, const std::string& path) = 0;
    virtual vector<vector<vector<float>>> get_evs(shared_ptr<ActionNode> node,vector<Card> cards, const std::string& path) = 0;
    shared_ptr<GameTree> tree;
};


#endif //TEXASSOLVER_SOLVER_H
