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
                
                cache.set_block_cycles_left(cache.get_block_cycles_left()-1);
                cache.execution_cycles++;
              
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

void Simulator::print_stats(const string& outfilename, const string& tracebase) {
    // ... existing print_stats implementation ...
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
            out << "Cache Size (KB per core): TO BE CONFIRMED " << pow(2,s) * pow(2,b) * E /1024 << "\n";
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
                out << "  Miss rate: " << fixed << setprecision(4)
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
    // ... existing all_done implementation ...
    for (int i = 0; i < 4; ++i)
                if (!done[i] || caches[i].get_blocked())
                    return false;
            return true;
}


//execution cycles - 12. Execution cycles mean the core is actively processing an instruction.
//If there is a cache hit, it takes 1 cycle.
//If there is a cache miss, the core is stalled for many cycles (counted under idle cycles), and then spends 1 execution cycle to complete the instruction after the data is available.

//bus access - 14. No, invalidate signals do not need to retry.
//They are logical events that happen immediately without needing exclusive bus access.
//You do not wait or retry invalidates.
//Bus being busy only affects new data transfers, not invalidate signa

//ASSUMPTION IN CASE OF WRITE MISS DATA IS TRANSFERRED THROUGH MEMORY ONLY NO CACHE TO CACHE EVER

//cycles -18. Idle cycles include any cycles where the processor is stalled waiting for a cache miss to be serviced (memory fetch or cache-to-cache transfer).
//Execution cycles include cycles where instructions are processed (hits) or bus requests are being made or attempted.
//Total cycles = execution cycles + idle cycles.


//bus access - 19. Corrected assumption 
//At any point, the bus processes only one request at a time, whether it is a read miss, a write miss, a snooping invalidate, or a data transfer.
//New bus requests (including snooping actions) are only initiated when the bus becomes completely free.
//Thus, snooping and bus transactions are serialized, and happen strictly one after the other.

//cycles 22. Idle cycles = cycles when core is stalled and cannot issue a request.
//Execution cycles = cycles when instructions are actively processed or waiting after bus request is issued.
//23. The time taken to copy back the block from M state cache to memory is not considered part of the execution cycles of the processor that had the miss. 
//It is an independent bus transaction handled after the read miss is completed.
//So, the requesting core continues with its execution once it receives the data, and the writeback happens separately.

