# mvdcc

An implementation of Multi-version Deterministic Concurrency Control protocols by Haowen Li and Rina Onishi.

# Key features

- Extended from [tpcc-runner](https://github.com/rotaki/tpcc-runner) written by Riki Otaki
- Support YCSB benchmarks
- TPC-C benchmark is currently under construction

# Details

In this repository, 3 Deterministic Concurrency Control (DCC) protocols with 2 benchmarks are supported.

## Protocols

- Caracal
  - A Multi-version Deterministic Concurrency Control (MVDCC) protocol proposed in the paper: [&#34;Caracal: Contention Management with Deterministic Concurrency Control&#34;](https://www.eecg.toronto.edu/~ashvin/publications/caracal.pdf)which is published in SOSP '21.
- Serval
  - A MVDCC protocol that incorporates wait-free data structure which optimize upon Caracal proposed by the authors of current repository.
- Cheetah
  - A MVDCC protocol that eliminates Non-visible write and redesigns garbage collection which optimize upon Caracal proposed by the authors of the current repository.

### Optimizations in Serval

- Uses Per-core Version Array instead of Per-core Per-row buffer in Caracal.
- Incorporates 2 bitmaps for contended data item:
  - Core Bitmap
  - Transaction Bitmap
- Optimizes Version Search of Read Operations by using bitmaps.

### Optimizations in Cheetah

- Uses per-core version Array and incorporates 2 bitmaps just like Serval.
- Incorporates 2 new data structures for contended data item:
  - Placeholders
  - Reference Counter
- Optimizes Garbage Collection by introducing final reader & writer GC instead of performing major GC

## Benchmarks

- YCSB
  - [YCSB](https://ycsb.site) is a micro-benchmark for database systems. YCSB provides six sets of core workloads (A to F) that define a basic benchmark for cloud systems. serval supports four of them (A, B, C, F).
- TPC-C (Currently unsupported)
  - [TPC-C](http://www.tpc.org/tpcc/) is a benchmark for online transaction processing systems used as "realistic workloads" in academia.
  - TPC-C executes a mix of five different concurrent transactions of different types and complexity to measure the various performances of transaction engines.

# Build and Execute (Experiments)

## YCSB

Perform experiments of MVDCC protocols implemented in the current repository with YCSB workloads by executing the following command in the base directory to generate varying contention experiments:

```sh
python3 scripts/ycsb.py
```

### Protocols that support to be evaluated
- Caracal
- Serval
- Cheetah
- *Choose the number of protocols that will be evaluated by modifying ```ycsb.py```*

### Workload Types Available (```workloads```)
- ```X```: Write-only Workload (R:0%, W100%)
- ```W90```: Write-intensive Workload (R:10%, W90%)
- ```W80```: Write moderatively intensive Workload (R:20%, W80%)
- ```A```: YCSB-A Workload (R:50%, W50%)
- ```B```: YCSB-B Workload (R:95%, W5%)
- ```C```: Read-only (YCSB-C) Workload (R:100%, W0%)
- *See [ycsb documentation](https://github.com/brianfrankcooper/YCSB/wiki/Core-Workloads) for the details of the workload*

### Constants
- Number of Slots in Buffer (```buffer_slots```, only applicable to Caracal): 255
- Number of Transactions in each epoch (```txs_in_epochs```): 4,096 
- Number of Operations in each transaction (```repps```): 10
- Total number of records in Database (```records```): 10000000
- Number of threads (```threads```): 64

### Plots that will be generated

- Total Time varying Contention
- Initialization Time varying Contention
- Execution Time varying Contention
- Version Created varying Contention
- *Other Plots are under construction*

### ***Notes***

- Note that in YCSB, an extra `-PAYLOAD_SIZE=X` argument to determine the payload size at compile time is needed.
- After building, the executable will be stored into the `build/bin` directory.
- In our experiments, we use Payload size of 4 bytes as default.
- The number of experiments in each trial is set to 10.

<!-- ```sh
python3 scripts/ycsb.py A 10000000 64 1 0.5,0.99 10 
```

The above example will create table with 10M records (each with four bytes) and executes YCSB-A with varying skews of 0.5 and 0.99, 10 operations per transaction using 64 threads for 1 second.  -->

## TPC-C

Currently unsupported

<!-- # Performance

Under Construction -->

# Authors

- Haowen Li (Keio University, M1)
- Rina Onishi (Keio University, M1.5)
