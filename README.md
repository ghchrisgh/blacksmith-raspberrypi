# blacksmith-raspberry

This repository contains a set of tools developed for the purpose of investigating the Rowhammer vulnerability in ARM-based Raspberry Pi devices, particularly the Raspberry Pi 4 Model B and the Raspberry Pi 5, across a range of RAM configurations, from 4 GB to 16 GB.
This work is accompanied by the practical work in which the Blacksmith Rowhammer fuzzer was ported from the x86 architecture to AArch64.

## Utilities

This repository reuses several utility scripts and Makefile targets for setting up
and running the experiments, namely the PMU kernel-module targets (`build-pmu-kernel-module`,
`load-pmu`, `unload-pmu`, `test-pmu`), as well as `setup_venv.sh`, `check_dependencies.py`, and
`performance.sh`. These are
based on the repository [rowhammer-raspberry](https://github.com/averageTalking/rowhammer-raspberry) by [Jakob Drescher], which employed analogous tooling
to the same set of devices.

## Content
| File | Description |
|------|-------------|
| `hitconflict/` | Contains "hitconflict/" for Reverse Engineering of the DRAM addresses. |
| `blacksmith/` | Contains the port of Blacksmith from x86 to ARM. |
| `enable_arm_pmu.c` | C program to enable and configure the ARM Performance Monitoring Unit. |
| `load-module` | Script to load the custom kernel module required for Performance Monitoring Unit access. |
| `unload-module` | Script to safely unload the Performance Monitoring Unit access kernel module. |
| `setup_venv.sh` | Automates creation and activation of the Python virtual environment. |


## Usage  
**make** - Build all components and prepare the environment for experiments.  
**make venv** - Create Virtual Environment.  
**make check** - Check Python Dependencies.  
**make load-pmu** - Load ARM Performance Monitoring Unit.  
**make unload-pmu** - Unload ARM Performance Monitoring Unit.  
**make test-pmu** - Run PMU tests.  
**make clean** - Perform clean.  
