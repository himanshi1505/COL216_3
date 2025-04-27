#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <queue>
#include <iomanip>
#include <cassert>
#include <algorithm>
#include "simulator.h"
#include "bus.h"                                                       
using namespace std;


int main(int argc, char* argv[]) {
    string tracebase, outfilename;
    int s = 5, E = 2, b = 5;
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "-t") tracebase = argv[++i];
        else if (arg == "-s") s = stoi(argv[++i]);
        else if (arg == "-E") E = stoi(argv[++i]);
        else if (arg == "-b") b = stoi(argv[++i]);
        else if (arg == "-o") outfilename = argv[++i];
        else if (arg == "-h") {
            cout << "Usage: ./L1simulate -t <tracefile> -s <s> -E <E> -b <b> -o <outfilename>\n";
            return 0;
        }
    }
    if (tracebase.empty() || outfilename.empty()) {
        cerr << "Missing required arguments.\n";
        return 1;
    }
    Simulator sim(s, E, b, tracebase);
    sim.run();
    sim.print_stats(outfilename);
    return 0;
}
