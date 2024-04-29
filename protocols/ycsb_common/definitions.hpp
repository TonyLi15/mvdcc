#pragma once

#define NUM_CORE 64

#define NUM_TXS_IN_ONE_EPOCH_IN_ONE_CORE (NUM_TXS_IN_ONE_EPOCH / NUM_CORE) // the number of transactions in one epoch

#define NUM_TXS_IN_ONE_EPOCH 4096 // the number of transactions in one epoch
#define NUM_ALL_TXS  (NUM_TXS_IN_ONE_EPOCH * NUM_EPOCH) // total number of transactions executed in the experiment

// change NUM_EPOCH for experiments
#define NUM_EPOCH 1000
#define CLOCKS_PER_US 2100
#define CLOCKS_PER_MS (CLOCKS_PER_US * 1000)
#define CLOCKS_PER_S (CLOCKS_PER_MS * 1000)

#define NUM_REGIONS 10000 // Caracal's definition is 256, use 10000 here
#define MAX_SLOTS_OF_GLOBAL_ARRAY 100000

#define MAX_SLOTS_OF_PER_CORE_ARRAY 64 // serval
#define MAX_SLOTS_OF_PER_CORE_BUFFER 4 // caracal