#include "l1cache.h"
#include <iostream>
#include <algorithm>

using namespace std;

CacheLine::CacheLine() 
    : tag(0), state(INVALID), valid(false), dirty(false), lru_counter(0) {}

CacheLine::CacheLine(uint32_t t, MESIState s, bool v, bool d, uint64_t l) 
    : tag(t), state(s), valid(v), dirty(d), lru_counter(l) {}

CacheSet::CacheSet(int E) : lines(E) {}

L1Cache::L1Cache(int s_, int E_, int b_, int core_id_) 
    : s(s_), E(E_), b(b_), core_id(core_id_) {
    S = 1 << s;
    B = 1 << b;
    sets = std::vector<CacheSet>(S, CacheSet(E));
}

CacheLine* L1Cache::find_line(uint32_t tag, uint32_t set_idx) {
    for (auto& line : sets[set_idx].lines) {
        if (line.valid && line.tag == tag) {
            return &line;
        }
    }
    return nullptr;
}

CacheLine* L1Cache::find_lru(uint32_t set_idx) {
    auto& lines = sets[set_idx].lines;
    return &(*std::min_element(lines.begin(), lines.end(),
        [](const CacheLine& a, const CacheLine& b) { 
            return a.lru_counter < b.lru_counter; 
        }));
}

bool L1Cache::try_access(char op, uint32_t addr, Bus& bus, std::vector<L1Cache>& all_caches, uint64_t global_cycle) {
    uint32_t tag = addr >> (s + b);
    uint32_t set_idx = (addr >> b) & ((1 << s) - 1);
    CacheSet& set = sets[set_idx];

    CacheLine* line = find_line(tag, set_idx);
    if (!line)
    {
        cout << 4 << endl << endl;
    }
    if (line && line->state == INVALID)
    {
        cout << 5 << endl << endl;
    }
    if (line && line->state == MODIFIED)
    {
        cout << 6 << endl << endl;
    }
    if (line && line->state == SHARED)
    {
        cout << 7 << endl << endl;
    }
    if (line && line->state == EXCLUSIVE)
    {
        cout << 8 << endl << endl;
    }
    if (line && line->valid && line->state != INVALID) {
        // Cache hit
        if (op == 'R') {
            reads++;
        } else if (op == 'W') {
            writes++;
            if (line->state == SHARED) {
                if (bus.busy()) return false;
                bus.start(BusUpgr, addr, core_id, 1);
                process_busupgr(addr, all_caches);
            }
            line->state = MODIFIED;
            line->dirty = true;
        }
        line->lru_counter = global_cycle;
        return true;
    }

    //somethingn fring wrong here

    // Cache miss
    misses++;
    if (op == 'R') reads++; else writes++;

    blocked = true;
    blocked_op = op;
    blocked_addr = addr;
    
    
    // 1. Search for an invalid line (empty slot) in the set
    CacheLine* target_line = nullptr;
    for (auto& line : sets[set_idx].lines) {
        if (line.empty) {
            target_line = &line;
            break;
        }
    }

    // 2. If no invalid line found, evict LRU
    if (!target_line) {
        target_line = find_lru(set_idx);
        evictions++;  // True eviction
        if (target_line->dirty) {
            writebacks++;
            bus_traffic += B;
            total_cycles += 100;
        }
    }

    // 3. Use target_line (whether invalid or LRU)
    *target_line = CacheLine(tag, INVALID, true, op == 'W', global_cycle);
    target_line->empty = false;

    if (bus.busy()) return false;

    bool found_in_other = false;
    bool found_in_M = false;
    int owner = -1;
    for (size_t i = 0; i < all_caches.size(); ++i) {
        if (i == static_cast<size_t>(core_id)) continue;
        CacheLine* other = all_caches[i].find_line(tag, set_idx);
        if (other && other->valid && other->state != INVALID) {
            found_in_other = true;
            if (other->state == MODIFIED) { 
                found_in_M = true; 
                owner = i; 
            }
        }
    }

    int transfer_cycles = 0;
    if (op == 'R') {
        if (found_in_M) {
            transfer_cycles = 2 * B / 4;
            bus.start(BusRd, addr, core_id, transfer_cycles);
            all_caches[owner].find_line(tag, set_idx)->state = SHARED;
            pending_state = SHARED;
            bus_traffic += B;
        } else if (found_in_other) {
            transfer_cycles = 2 * B / 4;
            bus.start(BusRd, addr, core_id, transfer_cycles);
            pending_state = SHARED;
            bus_traffic += B;
        } else {
            transfer_cycles = 100;
            bus.start(BusRd, addr, core_id, transfer_cycles);
            pending_state = EXCLUSIVE;
            bus_traffic += B;
        }
    } else {
        transfer_cycles = found_in_other ? 2 * B / 4 : 100;
        bus.start(BusRdX, addr, core_id, transfer_cycles);
        pending_state = MODIFIED;
        bus_traffic += B;
        if (found_in_other) {
            process_busrdx(addr, all_caches);
        }
    }
    if (transfer_cycles == 0)
    {
        exit(-1);
    }
    block_cycles_left = transfer_cycles;
    if (block_cycles_left == 0 && blocked == true)
    {
        cout << 105 << endl;
    }
    return true;
}

void L1Cache::snoop(const BusTransaction& trans) {
    if (trans.type == NONE) return;
    uint32_t tag = trans.addr >> (s + b);
    uint32_t set_idx = (trans.addr >> b) & ((1 << s) - 1);
    CacheLine* line = find_line(tag, set_idx);
    if (!line || !line->valid || line->state == INVALID) return;

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
            line->valid = false;
            line->state = INVALID;
            invalidations++;
            break;
        case BusUpgr:
            invalidations++;
            line->valid = false;
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