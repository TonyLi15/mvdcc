#pragma once

#ifdef PAYLOAD_SIZE
using Record = Payload<PAYLOAD_SIZE>;
#else
#define PAYLOAD_SIZE 1024
using Record = Payload<PAYLOAD_SIZE>;
#endif

#define NUM_CORE 64

#define NUM_TXS_IN_ONE_EPOCH_IN_ONE_CORE                                       \
    (NUM_TXS_IN_ONE_EPOCH / NUM_CORE) // the number of transactions in one epoch

// #define NUM_TXS_IN_ONE_EPOCH 4096 // the number of transactions in one epoch
#define NUM_ALL_TXS 4096 * 1000
// #define NUM_ALL_TXS (4096 * 10)
// total number of transactions executed in the exeperiment

// change NUM_EPOCH for experiments
#define NUM_EPOCH (NUM_ALL_TXS / NUM_TXS_IN_ONE_EPOCH)

#define CLOCKS_PER_US 2100
#define CLOCKS_PER_MS (CLOCKS_PER_US * 1000)
#define CLOCKS_PER_S (CLOCKS_PER_MS * 1000)

#define NUM_REGIONS 1000 // Caracal's definition is 256, use 10000 here
// #define NUM_REGIONS 1000 // Caracal's definition is 256, use 10000 here

#define MAX_SLOTS_OF_PER_CORE_ARRAY 64 // for Serval
