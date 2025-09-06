//
// Created by bytedance on 7.6.21.
//

#ifndef BINDSOLVER_COMMANDLINETOOL_H
#define BINDSOLVER_COMMANDLINETOOL_H
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include "include/runtime/PokerSolver.h"
#include "include/solver/solver_options.h"
#include <map>

using namespace std;
class CommandLineTool{
public:
    CommandLineTool(string mode,string resource_dir);
    void startWorking();
    void execFromFile(string input_file);
    void processCommand(string input);
private:
    void parseAndAddNodeLockRule(const std::string& rule_line);
    enum Mode{
        HOLDEM,
        SHORTDECK
    };
    Mode mode;
    string resource_dir;
    PokerSolver ps;
    float oop_commit=5;
    float ip_commit=5;
    int current_round=1;
    int raise_limit=4;
    int thread_number=1;
    float small_blind=0.5;
    float big_blind=1;
    float stack=20 + 5;
    float allin_threshold = 0.67f;
    string range_ip;
    string range_oop;
    string board;
    float accuracy;
    int max_iteration=100;
    int use_isomorphism=0;
    int print_interval=10;
    int dump_rounds = 1;
    // Add these new members for analysis features
    bool m_use_full_board_analysis = false;
    std::map<std::pair<std::string, int>, LockedNode> m_locked_nodes_map;
    shared_ptr<GameTreeBuildingSettings> gtbs;
};

#endif //BINDSOLVER_COMMANDLINETOOL_H
