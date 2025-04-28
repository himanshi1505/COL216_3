#ifndef BUS_H
#define BUS_H

#include <cstdint>
enum BusTransactionType { NONE, BusRd, BusRdX, BusUpgr };

struct BusTransaction {
    BusTransactionType type;
    uint32_t addr;
    int source_core;
    int transfer_cycles_left;
    
    BusTransaction(BusTransactionType t = NONE, uint32_t a = 0, int s = -1, int c = 0);
};

class Bus {
public:
    BusTransaction current;
    bool busy() const;
    void start(BusTransactionType type, uint32_t addr, int core, int cycles);
    void tick();
    int transfer_cycle_left();
};

#endif // BUS_H
