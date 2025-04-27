#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "l1cache.h"
#include "bus.h"
#include <vector>
#include <string>

class Simulator {
public:
    Simulator(int s_, int E_, int b_, const std::string& tracebase);
    ~Simulator();
    void run();
    void print_stats(const std::string& outfilename);

private:
    int s, E, b, S, B;
    std::vector<L1Cache> caches;
    Bus bus;
    std::vector<std::ifstream*> tracefiles;
    std::vector<bool> done;
    std::vector<std::string> next_line;
    std::vector<bool> waiting_for_bus;
    uint64_t global_cycle = 1;

    bool all_done();
};

#endif // SIMULATOR_H
