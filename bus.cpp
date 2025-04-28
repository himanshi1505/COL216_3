#include "bus.h"
#include <iostream>

BusTransaction::BusTransaction(BusTransactionType t, uint32_t a, int s, int c) 
    : type(t), addr(a), source_core(s), transfer_cycles_left(c) {}

bool Bus::busy() const { 
    return current.type != NONE; 
}

void Bus::start(BusTransactionType type, uint32_t addr, int core, int cycles) {
    current = BusTransaction(type, addr, core, cycles);
}

void Bus::tick() {
    if (current.type != NONE) {
        current.transfer_cycles_left--;
        if (current.transfer_cycles_left == 0) {
            current.type = NONE;
        }
        if (current.transfer_cycles_left < 0)
        {
            exit(-1);
        }
    }
}

int Bus::transfer_cycle_left()
{
    return current.transfer_cycles_left;
}
