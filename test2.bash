#!/bin/bash

IA32_PERFEVTSELx=0x186

IA32_PMCx=0x0C1

IA32_PERF_GLOBAL_CTRL=0x38F

PMCNUM=0 # an available performance counter. one of these days I will automate the checking of availability


CONFIG_ENABLE_BITS=0x430000
EVENTSELECT=0xC0 # UNC_PROCHOT_ASSERTION event number (see Intel SDM Vol. 3B, Appendix A)
UMASK=0x00 # UNC_PROCHOT_ASSERTION umask value 

# quick-and-dirty. 0 and 1 are on different sockets in our configuration
for cpu in {0..1}
do
    # configure the counter at MSR_UNCORE_PERFEVTSEL$PMCNUM
    sudo wrmsr -p $(($cpu)) $(($IA32_PERFEVTSELx+$PMCNUM)) $(($CONFIG_ENABLE_BITS|$UMASK<<8|$EVENTSELECT))

    OLD_IA32_PERF_GLOBAL_CTRL_VALUE=`sudo rdmsr -p $(($cpu)) -c $IA32_PERF_GLOBAL_CTRL`
    # enable the counter at MSR_UNCORE_PMC$PMCNUM
    sudo wrmsr -p $(($cpu)) $(($IA32_PERF_GLOBAL_CTRL)) $(($OLD_IA32_PERF_GLOBAL_CTRL_VALUE|(1<<$PMCNUM)))
done

for cpu in {0..1}
do
    sudo rdmsr -p $(($cpu)) -d $(($IA32_PMCx+$PMCNUM))
done

for cpu in {0..1}
do
    sudo rdmsr -p $(($cpu)) -d $(($IA32_PMCx+$PMCNUM))
done

sleep 30

for cpu in {0..1}
do
    sudo rdmsr -p $(($cpu)) -d $(($IA32_PMCx+$PMCNUM))
done

# quick-and-dirty. 0 and 1 are on different sockets in our configuration
for cpu in {0..1}
do
    OLD_IA32_PERF_GLOBAL_CTRL_VALUE=`sudo rdmsr -p $(($cpu)) -c $IA32_PERF_GLOBAL_CTRL`
    # disable the counter at MSR_UNCORE_PMC$PMCNUM
    sudo wrmsr -p $(($cpu)) $IA32_PERF_GLOBAL_CTRL $(($OLD_IA32_PERF_GLOBAL_CTRL_VALUE&~(1<<$PMCNUM)))
done
