#!/bin/bash

if [ "$1" == "" ]; then
    echo "Expected argument (powersave/ performance)"
    exit
fi

max_proc=$(cat /proc/cpuinfo | grep processor | tail -n 1 | cut -d ":" -f 2)
mode=$1

echo "Max processor index: $max_proc"
echo "Setting mode: $mode"
echo

for i in $(seq 0 $max_proc)
do
    # Aktuellen Governor
    current=$(cat /sys/devices/system/cpu/cpu$i/cpufreq/scaling_governor)

    # Neuen Governor
    echo $mode | sudo tee -a /sys/devices/system/cpu/cpu$i/cpufreq/scaling_governor > /dev/null

    new=$(cat /sys/devices/system/cpu/cpu$i/cpufreq/scaling_governor)
    echo "CPU$i | current governor: $current | new governor: $new"
done
