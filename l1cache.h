#ifndef L1CACHE_H
#define L1CACHE_H

#include "bus.h"
#include <vector>
#include <cstdint>

enum MESIState { INVALID, SHARED, EXCLUSIVE, MODIFIED };

struct CacheLine {
    uint32_t tag;
    MESIState state=INVALID;
    bool dirty=false;
    uint64_t lru_counter=-1;
    bool empty=true;
    CacheLine();
    CacheLine(uint32_t t, MESIState s, bool d, uint64_t l, bool e);
};

struct CacheSet {
    std::vector<CacheLine> lines;
    explicit CacheSet(int E);
};

class L1Cache {
public:
    L1Cache(int s_, int E_, int b_, int core_id_);
    
    bool try_access(char op, uint32_t addr, Bus& bus, std::vector<L1Cache>& all_caches, uint64_t global_cycle);
    void snoop(const BusTransaction& trans);
    CacheLine* find_line(uint32_t tag, uint32_t set_idx);
    CacheLine* find_lru(uint32_t set_idx);

    bool get_blocked()  { return blocked; }
    int get_block_cycles_left() const { return block_cycles_left; }
    uint32_t get_blocked_addr() const { return blocked_addr; }
    MESIState get_pending_state() const { return pending_state; }
    void set_blocked(bool b) { blocked = b; }
    void set_block_cycles_left(int cycles) { block_cycles_left = cycles; }
    void set_blocked_addr(uint32_t addr) { blocked_addr = addr; }
    void set_pending_state(MESIState state) { pending_state = state; }
    
    // Statistics
    int reads = 0, writes = 0, misses = 0, evictions = 0;
    int writebacks = 0, invalidations = 0, idle_cycles = 0;
    int execution_cycles = 0, bus_traffic = 0;
    
private:
    int s, E, b, S, B, core_id;
    std::vector<CacheSet> sets;
    bool blocked = false;
    int block_cycles_left = 0;
    uint32_t blocked_addr = 0;
    MESIState pending_state = INVALID;
    
   
    void process_busrd(uint32_t addr, std::vector<L1Cache>& all_caches);
    void process_busrdx(uint32_t addr, std::vector<L1Cache>& all_caches);
    void process_busupgr(uint32_t addr, std::vector<L1Cache>& all_caches);
};

#endif // L1CACHE_H