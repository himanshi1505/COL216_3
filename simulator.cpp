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
using namespace std;
// simulator.cpp

// Include any other necessary headers for L1Cache and Bus here

Simulator::Simulator(int s_, int E_, int b_, const string& tracebase) : s(s_), E(E_), b(b_) {
   
    S = 1 << s; //num of sets
    B = 1 << b; //block size
    for (int i = 0; i < 4; ++i)
        caches.emplace_back(s, E, b, i);
    for (int i = 0; i < 4; ++i) {
        string fname = tracebase + "_proc" + to_string(i) + ".trace";
        tracefiles.push_back(new ifstream(fname));
        if (!tracefiles.back()->is_open()) {
            cerr << "Error opening " << fname << endl;
            exit(1);
        }
    }
    done = vector<bool>(4, false);
    next_line = vector<string>(4, "");
    waiting_for_bus = vector<bool>(4, false);
    
}

Simulator::~Simulator() {
    // ... existing destructor implementation ...
    for (auto it = tracefiles.begin(); it != tracefiles.end(); ++it) {
        if (*it) {
            (*it)->close();
            delete *it;
        }
    }
    
}

void Simulator::run() {
    
    // Prime first instruction for each core
    for (int i = 0; i < 4; ++i) {
        getline(*tracefiles[i], next_line[i]);
        
    }
    

    while (!all_done()) {
        // 1. Process bus (tick down busy cycles)
       

        bus.tick();
        // cout << bus.transfer_cycle_left() << endl;
        // 2. Process snooping for ongoing transaction (all cores except source)
        // if (bus.transfer_cycle_left() == 1) {
        //     for (int i = 0; i < 4; ++i) {
        //         if (i != bus.current.source_core)
        //             caches[i].snoop(bus.current);
        //             //when to invalid, and how do we see if it is being accessed before updating
        //     }
        // }

        // 3. For each core, in order, try to issue a memory op if not blocked
        for (int core = 0; core < 4; ++core) {

            L1Cache& cache = caches[core];
            
            //if (done[core] && !cache.get_blocked()) continue;
            if (done[core] && cache.get_blocked()) exit(-1);
            if (done[core]) continue;

            // If blocked on miss, count idle cycles, decrement block
            if (cache.get_blocked()) {
                //cout<<"global_cycle"<<global_cycle << endl;
                cache.idle_cycles++;
                cache.set_block_cycles_left(cache.get_block_cycles_left()-1);
                
                cache.total_cycles++;
                if (cache.get_block_cycles_left() == 0) {
                    // Block done, install line if needed
                    //test
                    uint32_t tag = cache.get_blocked_addr() >> (s + b);
                    uint32_t set_idx = (cache.get_blocked_addr() >> b) & ((1 << s) - 1);
                    CacheLine* line = cache.find_line(tag, set_idx);
                    //endtest
                    
                    
                    if (cache.get_pending_state() == INVALID || !line)
                    {  
                        exit(-1);
                    }
                    
                    line->state = cache.get_pending_state();
                    line->lru_counter = global_cycle;
                    if (cache.get_pending_state() == MODIFIED) {
                        line->dirty = true;
                    }
                    cache.set_pending_state(INVALID);
                    cache.set_blocked(false);
                    
                }
                // // cout << cache.get_block_cycles_left() << endl;
                // if (cache.get_block_cycles_left() < 0)
                // {
                //     exit(-1);
                // }
                continue;
            }
            
            // If bus is busy, can't issue a new bus transaction (but can do cache hits)
            if (waiting_for_bus[core]) {  //wtf
                cache.idle_cycles++;
                cache.total_cycles++;
              // cout << 9 << endl;
                continue;
            }

            // If no more instructions, mark done
            if (next_line[core].empty()) {
                done[core] = true;
               // cout << -2 << endl;
                continue;
            }

            // Parse next instruction
            istringstream iss(next_line[core]);
            char op;
            string addr_str;
            if (!(iss >> op >> addr_str)) {
                exit(-1);
            }
            uint32_t addr = stoul(addr_str, nullptr, 16);

            // Try to access (returns true if op was started, false if needs to wait for bus)
           // cout << core << endl;
            bool issued = cache.try_access(op, addr, bus, caches, global_cycle);

            cache.total_cycles++;
            if (issued) {
                // Read next instruction for this core for next cycle
                if (!getline(*tracefiles[core], next_line[core]))
                    next_line[core] = "";
            } else {
                // Mark that this core is waiting for bus
                waiting_for_bus[core] = true;
            }
        }
        
        // 4. Reset waiting_for_bus for next cycle if bus is now free
        if (!bus.busy()) {
            fill(waiting_for_bus.begin(), waiting_for_bus.end(), false); 
        }
        global_cycle++;
    }
}

void Simulator::print_stats(const string& outfilename) {
    // ... existing print_stats implementation ...
    ofstream out(outfilename);
            for (int i = 0; i < 4; ++i) {
                out << "Core " << i << ":\n";
                out << "  Reads: " << caches[i].reads << "\n";
                out << "  Writes: " << caches[i].writes << "\n";
                out << "  Misses: " << caches[i].misses << "\n";
                out << "  Total cycles: " << caches[i].total_cycles << "\n";
                out << "  Idle cycles: " << caches[i].idle_cycles << "\n";
                out << "  Miss rate: " << fixed << setprecision(4)
                    << (double)caches[i].misses / (double) (caches[i].reads + caches[i].writes) << "\n";
                out << "  Evictions: " << caches[i].evictions << "\n";
                out << "  Writebacks: " << caches[i].writebacks << "\n";
                out << "  Invalidations: " << caches[i].invalidations << "\n";
                out << "  Bus traffic: " << caches[i].bus_traffic << " bytes\n";
            }
        }


bool Simulator::all_done() {
    // ... existing all_done implementation ...
    for (int i = 0; i < 4; ++i)
                if (!done[i] || caches[i].get_blocked())
                    return false;
            return true;
}
