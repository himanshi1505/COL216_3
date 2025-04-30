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
    for (auto it = tracefiles.begin(); it != tracefiles.end(); ++it) {
        if (*it) {
            (*it)->close();
            delete *it;
        }
    }
    
}

void Simulator::run() {
    
    for (int i = 0; i < 4; ++i) {
        getline(*tracefiles[i], next_line[i]);
        
    }
    


    while (!all_done()) {
        // 1. Process bus (tick down busy cycles)
       

        bus.tick();
        // 3. For each core, in order, try to issue a memory op if not blocked
        for (int core = 0; core < 4; ++core) {

            L1Cache& cache = caches[core];
            
          
            if (done[core]) continue;

            // If blocked on miss, count idle cycles, decrement block
            if (cache.get_blocked()) {

                
                cache.set_block_cycles_left(cache.get_block_cycles_left()-1);
                cache.execution_cycles++;
              
                if (cache.get_block_cycles_left() == 0) {

                    uint32_t tag = cache.get_blocked_addr() >> (s + b);
                    uint32_t set_idx = (cache.get_blocked_addr() >> b) & ((1 << s) - 1);
                    CacheLine* line = cache.find_line(tag, set_idx);
                    
                    line->state = cache.get_pending_state();
                    line->lru_counter = global_cycle;
                    if (cache.get_pending_state() == MODIFIED) {
                        line->dirty = true;
                    }
                    cache.set_pending_state(INVALID);
                    cache.set_blocked(false);                  
                }
                continue;
            }

            // If no more instructions, mark done
            if (next_line[core].empty()) {
                done[core] = true;
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
            bool issued = cache.try_access(op, addr, bus, caches, global_cycle);

        
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
        if (bus.transfer_cycle_left() == 0)
        {
            bus.change();
        }
    }
}

void Simulator::print_stats(const string& outfilename, const string& tracebase) {
    int total_traffic = 0;
    int total_transaction = 0;
    ofstream out(outfilename);
            out << "Simulation Parameters:\n";
            out << "Trace Prefix: " << tracebase << "\n";
            out << "Set Index Bits: " << s << "\n";
            out << "Associativity: " << E << "\n";
            out << "Block Bits: " << b << "\n";
            out << "Block Size (Bytes): " << pow(2,b) << "\n";
            out << "Number of Sets: " << pow(2,s) << "\n";
            out << "Cache Size (KB per core): " << pow(2,s) * pow(2,b) * E /1024 << "\n";
            out << "MESI Protocol: Enabled\n";
            out << "Write Policy: Write-back, Write-allocate\n";
            out << "Replacement Policy: LRU\n";
            out << "Bus: Central snooping bus\n";
            for (int i = 0; i < 4; ++i) {
                out << "Core " << i << ":\n";
                out << "  Reads: " << caches[i].reads << "\n";
                out << "  Writes: " << caches[i].writes << "\n";
                out << "  Misses: " << caches[i].misses << "\n";
                out << "  Execution cycles: " << caches[i].execution_cycles << "\n";
                out << "  Idle cycles: " << caches[i].idle_cycles << "\n";
                out << "  Miss rate: " << fixed << setprecision(2)
                    << (double)100 * caches[i].misses / (double)  (caches[i].reads + caches[i].writes) << "%\n";
                out << "  Evictions: " << caches[i].evictions << "\n";
                out << "  Writebacks: " << caches[i].writebacks << "\n";
                out << "  Invalidations: " << caches[i].invalidations << "\n";
                out << "  Bus traffic: " << caches[i].bus_traffic << " bytes\n";
                total_traffic += caches[i].bus_traffic;
                total_transaction += caches[i].bus_transaction;
            }
            out << "\nOverall Bus Summary:\n";
            out << "Total Bus Transactions: " << total_transaction << "\n";
            out << "Total Bus Traffic (Bytes): " << total_traffic << "\n";
        }


bool Simulator::all_done() {
    for (int i = 0; i < 4; ++i)
                if (!done[i] || caches[i].get_blocked())
                    return false;
            return true;
}

