#!/bin/bash

MSR_UNCORE_PERFEVTSEL0=0x3c0

MSR_UNCORE_PMC0=0x3b0

# 
CONFIG_ENABLE_BITS=0x420000

MSR_UNCORE_PERF_GLOBAL_CTRL=0x391

PMCNUM=0 # an available performance counter. one of these days I will automate the checking of availability
EVENTNUM=0x00 # UNC_PROCHOT_ASSERTION event number (see Intel SDM Vol. 3B, Appendix A)
UMASKVAL=0x00 # UNC_PROCHOT_ASSERTION umask value 

COUNTER=$(($CONFIG_ENABLE_BITS|$UMASKVAL<<8|$EVENTNUM))
# quick-and-dirty. 0 and 1 are on different sockets in our configuration
for cpu in {0}
do
    # configure the counter at MSR_UNCORE_PERFEVTSEL$PMCNUM
    echo $(($MSR_UNCORE_PERFEVTSEL0+$PMCNUM)) $COUNTER

    # enable the counter at MSR_UNCORE_PMC$PMCNUM
    echo $(($(echo $MSR_UNCORE_PERF_GLOBAL_CTRL)|(1<<$PMCNUM)))
done

# quick-and-dirty. 0 and 1 are on different sockets in our configuration
for cpu in {0}
do
    echo $(($MSR_UNCORE_PMC0+$PMCNUM))
done
#watch the counter increment or add your own code to do something useful
# watch -n 5 "for cpu in {0}; do rdmsr -p\$cpu -c $(($MSR_UNCORE_PMC0+$PMCNUM)); done"

# quick-and-dirty. 0 and 1 are on different sockets in our configuration
for cpu in {0}
do
    # disable the counter at MSR_UNCORE_PMC$PMCNUM
    echo $(($(echo $MSR_UNCORE_PERF_GLOBAL_CTRL)&~(1<<$PMCNUM)))
done
