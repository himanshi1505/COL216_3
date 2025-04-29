#include "l1cache.h"
#include <iostream>
#include <algorithm>

using namespace std;

CacheLine::CacheLine() 
    : tag(0), state(INVALID), dirty(false), lru_counter(-1), empty(true) {}

CacheLine::CacheLine(uint32_t t, MESIState s, bool d, uint64_t l, bool e) 
    : tag(t), state(s),  dirty(d), lru_counter(l), empty(e) {}

CacheSet::CacheSet(int E) : lines(E) {}

L1Cache::L1Cache(int s_, int E_, int b_, int core_id_) 
    : s(s_), E(E_), b(b_), core_id(core_id_) {
    S = 1 << s;
    B = 1 << b;
    sets = std::vector<CacheSet>(S, CacheSet(E));
}

CacheLine* L1Cache::find_line(uint32_t tag, uint32_t set_idx) {
    for (auto& line : sets[set_idx].lines) {
        //cout << tag << endl << line.tag << endl << endl;
        if ( line.tag == tag ) {
            //cout << 5 << endl;
            return &line;
        }
    }
    return nullptr;
}

//test
CacheLine* L1Cache::find_lru(uint32_t set_idx) {
    auto& lines = sets[set_idx].lines;
    return &(*std::min_element(lines.begin(), lines.end(),
        [](const CacheLine& a, const CacheLine& b) { 
            return a.lru_counter <= b.lru_counter; 
        }));
}

bool L1Cache::try_access(char op, uint32_t addr, Bus& bus, std::vector<L1Cache>& all_caches, uint64_t global_cycle) {
    uint32_t tag = addr >> (s + b);
    uint32_t set_idx = (addr >> b) & ((1 << s) - 1);
    //cout<<"set_idx: " << set_idx << endl;

    CacheLine* line = find_line(tag, set_idx);
 

    if (line  && line->state != INVALID) {
        // Cache hit
        line->lru_counter = global_cycle; // ASSUMPTION even if bus is busy we update LRU COUNTER
        if (op == 'R') {
            reads++;
        } else if (op == 'W') {
            
            
            if (line->state == SHARED) {
                // if (bus.busy()) return false; //invalidate can be sent even if bus is busy
                //bus.start(BusUpgr, addr, core_id, 1);
                if (bus.busy()) return false; //ASSUMPTION BUS CANT BE USED FOR INVALIDATIONS IF BUSY
                process_busupgr(addr, all_caches);
                invalidations++;
                //int x = 0;
                // for (size_t i = 0; i < all_caches.size(); ++i) {
                //     if (i == static_cast<size_t>(core_id)) continue;
                //     CacheLine* other = all_caches[i].find_line(tag, set_idx);
                //     invalidations++;
                   
                //     if (other && other->state == SHARED) {
                //         x++;
                //     }
                // }
                // if (x == 0)
                // {
                //     exit(-1);
                // }
            }   
            //ASSUMPTION( DO NOTY WRITE) WE DONT HAVE TO WRITEBACK IN CASE OF M hit
            writes++;
            line->state = MODIFIED;
            line->dirty = true;
        }
        //line->lru_counter = global_cycle;
        //cout << 2 <<  endl;;
        execution_cycles++;
        return true;
    }
  
    //somethingn fring wrong here
    if (bus.busy()) return false;

    // Cache miss
    
    

    
    
    
    // 1. Search for an invalid line (empty slot) in the set
    CacheLine* target_line = nullptr;
    for (auto& line : sets[set_idx].lines) {
        if (line.empty) {
            target_line = &line;
            break;
        }
    }

    // 2. If no invalid line found, evict LRU

    //test
    if (!target_line) {
        target_line = find_lru(set_idx);
        evictions++;  // True eviction
        if (target_line->dirty) {
            writebacks++;
            bus_traffic += B;
           
            bus.start(BusRdX, tag << (s + b), core_id, 100);
            target_line->dirty = false; // Reset dirty bit
            target_line->state = INVALID;
            target_line->empty = true;
            return false;

        }
        target_line->state = INVALID; // Writeback // do we need to increase the invalidation count?
       
    }
    blocked = true;
    misses++;
    blocked_addr = addr;
    //now line is the line to be used, and hence the line will no more be empty
    target_line->tag = tag;
    target_line->lru_counter = global_cycle; //lrcounter is set to start touch of change 
    target_line->empty = false;
    if (op == 'R'){
        reads++;
        //cout<<"read miss"<<global_cycle<<endl;
    }  else{
        writes++;
        //cout<<"write miss"<<global_cycle<<endl;

    } 
    execution_cycles++;

    
    bool found_in_other = false;
    bool found_in_M = false;
    //cout<<all_caches.size()<<endl;
    for (size_t i = 0; i < all_caches.size(); ++i) {
        if (i == static_cast<size_t>(core_id)) continue;
        CacheLine* other = all_caches[i].find_line(tag, set_idx);
        if (other && other->state != INVALID) {
            found_in_other = true;
            if (other->state == MODIFIED) { 
                found_in_M = true; 
            }
        }
    }

    int transfer_cycles = 0;
    if (op == 'R') {
        if (found_in_M) {
            transfer_cycles = (2 * B/4 )+100 ; // transfer and writeback  //ASSUMPTION CHECK HOW MANY TRANSFER CYCLE IN EACH CASE
            writebacks++;
            bus.start(BusRd, addr, core_id, transfer_cycles);
            process_busrd(addr, all_caches); // change others to SHARED
            //all_caches[owner].find_line(tag, set_idx)->state = SHARED;
            pending_state = SHARED;
            bus_traffic += B;
            
        } else if (found_in_other) { //several S copy, or one cache has E copy
            transfer_cycles = 2 * B/4;
            bus.start(BusRd, addr, core_id, transfer_cycles);
            process_busrd(addr, all_caches); // Invalidate others
            pending_state = SHARED;
            bus_traffic += B;
        } else {
            //cout<<"here"<<endl;
            transfer_cycles = 100; //read from mem to cache
            bus.start(BusRd, addr, core_id, transfer_cycles);
            pending_state = EXCLUSIVE;
            bus_traffic += B;
        }
        //cout<<"bus read"<<bus.transfer_cycle_left()<<endl;
    } else { // Write miss (BusRdX)
        if (found_in_M) {
            // Case 3: Dirty copy exists (M state)
            
            transfer_cycles = 200; // 100 writeback + 100 read from memory
            bus.start(BusRdX, addr, core_id, transfer_cycles);
            writebacks++;
            
            // Force writeback and invalidate owner
            //all_caches[owner].find_line(tag, set_idx)->state = INVALID;
            process_busrdx(addr, all_caches); // Invalidate others
            invalidations++;
            bus_traffic += 2 * B; // Count writeback + transfer
        } 
        else if (found_in_other) {
            // Case 2: Clean copies exist (S/E states)
            transfer_cycles = 100; // read from memory to cache
            bus.start(BusRdX, addr, core_id, transfer_cycles);
            process_busrdx(addr, all_caches); // Invalidate others
            invalidations++;
            bus_traffic += B;
        } 
        else {
            // Case 1: No other copies
            transfer_cycles = 100; // read from memory to cache
            bus.start(BusRdX, addr, core_id, transfer_cycles);
            bus_traffic += B;
        }
       // cout<<"bus write"<<bus.transfer_cycle_left()<<endl;
        pending_state = MODIFIED;
    }
        block_cycles_left = transfer_cycles;
    return true;
}

void L1Cache::snoop(const BusTransaction& trans) {
    if (trans.type == NONE) return;
    uint32_t tag = trans.addr >> (s + b);
    uint32_t set_idx = (trans.addr >> b) & ((1 << s) - 1);
    CacheLine* line = find_line(tag, set_idx);
    if (!line || line->state == INVALID) return;

    switch (trans.type) {
        case BusRd:
            if (line->state == MODIFIED) {
                bus_traffic += B;
                line->state = SHARED;  // to be written in memory +100 cycles
                line->dirty = false;
            } else if (line->state == EXCLUSIVE) {
                line->state = SHARED;
            }
            break;
        case BusRdX:
            line->state = INVALID;
            line->empty = true; // Mark as empty
            // invalidations++;
            break;
        case BusUpgr:
            // invalidations++;
           line->empty = true; // Mark as empty
            line->state = INVALID;
            break;
        default:
            break;
    }
}

void L1Cache::process_busrd(uint32_t addr, std::vector<L1Cache>& all_caches) {
    for (auto& cache : all_caches) {
        if (cache.core_id != core_id)
            cache.snoop(BusTransaction(BusRd, addr, core_id));
    }
}

void L1Cache::process_busrdx(uint32_t addr, std::vector<L1Cache>& all_caches) {
    for (auto& cache : all_caches) {
        if (cache.core_id != core_id)
            cache.snoop(BusTransaction(BusRdX, addr, core_id));
    }
}

void L1Cache::process_busupgr(uint32_t addr, std::vector<L1Cache>& all_caches) {
    for (auto& cache : all_caches) {
        if (cache.core_id != core_id)
            cache.snoop(BusTransaction(BusUpgr, addr, core_id));
    }
}
