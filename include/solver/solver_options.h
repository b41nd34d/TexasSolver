//
// Created by Christian Foernges
//

#ifndef TEXASSOLVER_SOLVER_OPTIONS_H
#define TEXASSOLVER_SOLVER_OPTIONS_H

#include <string>
#include <vector>
#include <map>

// Forward-declare Card, as its definition is in another header (e.g., GameTree.h).
// It's typically an integer or a small struct.
class Card;

// An Action is represented by an integer.
// e.g., -1=Fold, 0=Call/Check, >0 = Raise amount
using Action = int;

// A strategy is a probability distribution over possible actions.
using Strategy = std::map<Action, double>;

/**
 * @brief Defines a locked strategy for a specific player at a specific game node.
 * This forces the solver to use a fixed strategy at this point, allowing analysis
 * of non-standard lines.
 */
struct LockedNode {
    // A unique string identifying the node, built from the sequence of actions.
    // Example: "r100/c/" for a preflop raise to 100 followed by a call.
    std::string node_path;

    // The player index (0 for OOP, 1 for IP) whose strategy is to be locked.
    int player_to_lock;

    // The fixed strategy to use at this node. The probabilities must sum to 1.0.
    Strategy locked_strategy;
};

/**
 * @brief Defines a specific hand scenario with a full 5-card board.
 * When a solver is analyzing a game with this option, it will prune the
 * game tree traversal to only this specific board runout, dramatically
 * increasing performance.
 */
struct FullBoardSituation {
    // The 5 community cards (flop, turn, and river), represented by their integer IDs.
    // Must have a size of 5.
    std::vector<int> board_cards;
};

#endif //TEXASSOLVER_SOLVER_OPTIONS_H