#include "helper.h"

void pin_cpu(int core)
{
    cpu_set_t s;
    CPU_ZERO(&s);
    CPU_SET(core, &s);
    sched_setaffinity(0, sizeof(s), &s);
}
