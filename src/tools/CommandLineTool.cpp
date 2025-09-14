//
// Created by bytedance on 7.6.21.
//
#include "include/tools/CommandLineTool.h"
#include <QString>

CommandLineTool::CommandLineTool(string mode,string resource_dir) {
    string suits = "c,d,h,s";
    string ranks;
    this->resource_dir = resource_dir;
    string compairer_file,compairer_file_bin;
    int lines;
    if(mode == "holdem"){
        ranks = "2,3,4,5,6,7,8,9,T,J,Q,K,A";
        compairer_file = this->resource_dir + "/compairer/card5_dic_sorted.txt";
        compairer_file_bin = this->resource_dir + "/compairer/card5_dic_zipped.bin";
        lines = 2598961;
    }else if(mode == "shortdeck"){
        ranks = "6,7,8,9,T,J,Q,K,A";
        compairer_file = this->resource_dir + "/compairer/card5_dic_sorted_shortdeck.txt";
        compairer_file_bin = this->resource_dir + "/compairer/card5_dic_zipped_shortdeck.bin";
        lines = 376993;
    }else{
        throw runtime_error(tfm::format("mode not recognized : ",mode));
    }
    string logfile_name = "../resources/outputs/outputs_log.txt";
    this->ps = PokerSolver(ranks,suits,compairer_file,lines,compairer_file_bin);

    StreetSetting gbs_flop_ip = StreetSetting(vector<float>{},vector<float>{},vector<float>{},true);
    StreetSetting gbs_turn_ip = StreetSetting(vector<float>{},vector<float>{},vector<float>{},true);
    StreetSetting gbs_river_ip = StreetSetting(vector<float>{},vector<float>{},vector<float>{},true);

    StreetSetting gbs_flop_oop = StreetSetting(vector<float>{},vector<float>{},vector<float>{},true);
    StreetSetting gbs_turn_oop = StreetSetting(vector<float>{},vector<float>{},vector<float>{},true);
    StreetSetting gbs_river_oop = StreetSetting(vector<float>{},vector<float>{},vector<float>{},true);

    this->gtbs = make_shared<GameTreeBuildingSettings>(gbs_flop_ip,gbs_turn_ip,gbs_river_ip,gbs_flop_oop,gbs_turn_oop,gbs_river_oop);
}

void CommandLineTool::startWorking() {
    string input_line;
    while(cin) {
        getline(cin, input_line);
        this->processCommand(input_line);
    };
}

void CommandLineTool::execFromFile(string input_file){
    std::ifstream infile(input_file);
    std::string input_line;
    while (std::getline(infile, input_line))
    {
        this->processCommand(input_line);
    }

}

void split(const string& s, char c,
           vector<string>& v) {
    string::size_type i = 0;
    string::size_type j = s.find(c);
    while (j != string::npos) {
        v.push_back(s.substr(i, j - i));
        i = j + 1;
        j = s.find(c, i);
    }
    v.push_back(s.substr(i));
}


void CommandLineTool::processCommand(string input) {
    vector<string> contents;
    split(input,' ',contents);
    if(contents.size() == 0) contents = {input};
    if(contents.size() > 2 || contents.size() < 1)throw runtime_error(tfm::format("command not valid: %s",input));
    string command = contents[0];
    string paramstr = contents.size() == 1 ? "" : contents[1];
    if(command == "set_pot"){
        this->ip_commit = stof(paramstr) / 2;
        this->oop_commit = stof(paramstr) / 2;
    }else if(command == "set_effective_stack"){
        this->stack = stof(paramstr) + this->ip_commit;
    }else if(command == "set_board"){
        this->board = paramstr;
        vector<string> board_str_arr = string_split(board,',');
        if(board_str_arr.size() == 3){
            this->current_round = 1;
        }else if(board_str_arr.size() == 4){
            this->current_round = 2;
        }else if(board_str_arr.size() == 5){
            this->current_round = 3;
        }else{
            throw runtime_error(tfm::format("board %s not recognized",this->board));
        }
    }else if(command == "set_range_ip"){
        this->range_ip = paramstr;
    }else if(command == "set_range_oop"){
        this->range_oop = paramstr;
    }else if(command == "set_bet_sizes"){
        vector<string> params;
        split(paramstr,',',params);
        if(params.size() < 3)throw runtime_error("param number error");
        // oop,turn,bet,30,70,100
        string player = params[0];
        string round = params[1];
        string bet_type = params[2];
        StreetSetting& streetSetting = this->gtbs->get_setting(player,round);
        vector<float>* sizes;
        if(bet_type == "allin") streetSetting.allin = true;
        else if(bet_type == "bet") sizes = &(streetSetting.bet_sizes);
        else if(bet_type == "raise") sizes = &(streetSetting.raise_sizes);
        else if(bet_type == "donk") sizes = &(streetSetting.donk_sizes);
        else throw runtime_error("");

        if(bet_type == "bet" || bet_type == "raise" || bet_type == "donk"){
            sizes->clear();
            for(std::size_t i = 3;i < params.size();i ++ ){
                sizes->push_back(stof(params[i]));
            }
        }
    }else if(command == "set_accuracy"){
        this->accuracy = stof(paramstr);
    }else if(command == "set_allin_threshold"){
        this->allin_threshold = stof(paramstr);
    }else if(command == "set_thread_num"){
        this->thread_number = stoi(paramstr);
    }else if(command == "build_tree"){
        this->ps.build_game_tree(oop_commit,ip_commit,current_round,raise_limit,small_blind,big_blind,stack,*gtbs.get(),allin_threshold);
    }else if(command == "set_max_iteration"){
        this->max_iteration = stoi(paramstr);
    }else if(command == "set_use_isomorphism"){
        this->use_isomorphism = stoi(paramstr);
    }else if(command == "set_print_interval"){
        this->print_interval = stoi(paramstr);
    }else if(command == "set_raise_limit"){
        this->raise_limit = stoi(paramstr);
    }else if(command == "start_solve"){
        if (this->hand_analysis) {
            this->ps.build_game_tree(oop_commit,ip_commit, 1 /* FLOP */,raise_limit,small_blind,big_blind,stack,*gtbs.get(),allin_threshold);
        }
        if (this->ps.getGameTree() == nullptr) {
            throw runtime_error("Game tree not built. Please use the build_tree command first.");
        }
        if (this->board.empty()) {
            throw runtime_error("Board is empty. Please use the set_board command first.");
        }
        cout << "<<<START SOLVING>>>" << endl;
        this->ps.analysis_mode = this->hand_analysis ? Solver::AnalysisMode::HAND_ANALYSIS : Solver::AnalysisMode::STANDARD;
        if (this->hand_analysis) {
            this->ps.full_board = this->board;
        }
        this->ps.train(
                this->range_ip,
                this->range_oop,
                this->board,
                "tmp_log.txt",
                max_iteration,
                this->print_interval,
                "discounted_cfr",
                -1,
                this->accuracy,
                this->use_isomorphism,
                0, // TODO: enable half float option for command line tool
                this->thread_number
        );
    }else if(command == "dump_result"){
        string output_file = paramstr;
        this->ps.dump_strategy(QString::fromStdString(output_file),this->dump_rounds);
    }else if(command == "set_dump_rounds"){
        this->dump_rounds = stoi(paramstr);
    }else if(command == "set_hand_analysis"){
        this->hand_analysis = (stoi(paramstr) != 0);
    }else{
        cout << "command not recognized: " << command << endl;
    }
}
