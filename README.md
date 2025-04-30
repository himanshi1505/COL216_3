# COL216_3

# L1 Cache Simulator with MESI Protocol for Quad-core Processors

This simulator implements a detailed model of quad-core L1 data caches connected via a shared bus, using the MESI coherence protocol. Each core has an independent L1 cache with configurable set associativity, block size, and total size, simulated using a C++ implementation.

## File Structure

- **main.cpp:** 
-Entry point of the simulator

-Handles command line argument parsing
-Validates input parameters

-Creates and manages the Simulator instance

- **simulator.cpp and simulator.h:**
-Simulator class orchestrates the entire simulation
-Manages:4 L1 cache instances (one per core), Global bus operations, Cycle-by-cycle execution, Trace file handling

- **bus.cpp and bus.h**
-Models the shared communication bus
-Handles: Bus arbitration between cores , Transaction timing (BusRd, BusRdX, BusUpgr), Snooping operations
- **l1cache.cpp and l1cache.h**
-L1Cache class models individual caches
-Features: Configurable size/associativity, LRU replacement policy, MESI state tracking

###  Design Decision Assumptions

-If Core 0 was blocked and on cycle 100 it was unblocked, then on cycle 100 all other cores will still the bus as busy, only on the 101st cycle will the cores find the bus to be free and the first core to execute a free bus instruction would be Core 0 itself.
- Cachelines with Invalid states are assumed as Empty, i.e. they are given the highest priority to evict and also dont count in evictions when we evict them.
-Even if the bus is busy and we arrive at a instruction which is waiting for the bus, we update the LRU COUNTER because we dont want to evict it as it will soon be used. We have implement the LRU strategy to evict from cache, i.e. the least recently used bit will be evicted if no cache line is empty or invalid.
- If Two or more cores are both going to use the bus then we give priority to the lower core.
-Bus can't be used for send signals like invalidate , when it is busy. That is, bus is considered busy while transfering data and signals , and they canonly happen one at a time.
- transfer_cycles = (2 * B/4 )+100 we consider core to be busy in whole of this time similarly in write miss another modified case for 200 cycles.

##  Run Locally using
  - to run: make run
        or 
  - to compile: make 
    then
     ./L1simulate -t traces/tracebase -s 5 -E 2 -b 5 -o results.txt

  - to clean use : make clean

## Parameters
- -t: Trace file base name (appends _proc0.trace, etc.)

- -s: Number of set index bits (sets = 2^s)

- -E: Associativity (ways per set)

- -b: Block offset bits (block size = 2^b bytes)

- -o: Output file for statistics

## Output Statistics
- Reads
- Writes
- Misses           
- Execution cycles  
- Idle Cycles              
- Miss rate      
-  Evictions
-  Writebacks
-  Invalidations
-  Bus traffic
         
- Total Bus Transactions
- Total Bus Traffic (Bytes)

## Authors

- Himanshi Bhandari (2023CS10888)
- Mohil Shukla (2023CS10186)
