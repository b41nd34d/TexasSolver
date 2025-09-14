//
// Created by bytedance on 9.6.21.
//
#include "include/tools/CommandLineTool.h"
#include "include/tools/argparse.hpp"
#include <iostream>

int main(int argc,const char **argv) {
    try {
        ArgumentParser parser;

        parser.addArgument("-i", "--input_file", 1, true);
        parser.addArgument("-r", "--resource_dir", 1, true);
        parser.addArgument("-m", "--mode", 1, true);

        parser.parse(argc, argv);

        string input_file = parser.retrieve<string>("input_file");
        string resource_dir = parser.retrieve<string>("resource_dir");
        if(resource_dir.empty()){
            resource_dir = "./resources";
        }
        string mode = parser.retrieve<string>("mode");
        if(mode.empty()){mode = "holdem";}
        if(mode != "holdem" && mode != "shortdeck")
            throw runtime_error(tfm::format("mode %s error, not in ['holdem','shortdeck']",mode));

        if(input_file.empty()) {
            CommandLineTool clt = CommandLineTool(mode,resource_dir);
            clt.startWorking();
        }else{
            cout << "EXEC FROM FILE" << endl;
            CommandLineTool clt = CommandLineTool(mode,resource_dir);
            clt.execFromFile(input_file);
        }
    } catch (const std::runtime_error& e) {
        cerr << "A runtime error occurred: " << e.what() << endl;
        cerr << "This might be due to missing resource files. Please check your --resource_dir path." << endl;
        return 1;
    } catch (...) {
        cerr << "An unknown error occurred." << endl;
        return 1;
    }
    return 0;
}
