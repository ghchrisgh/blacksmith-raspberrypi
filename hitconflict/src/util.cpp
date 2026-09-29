// util.cpp

#include "util.h"

#include <stdlib.h>

void rand_pair(size_t size, size_t* off_a, size_t* off_b)
{
    do {
        *off_a = ((size_t)rand() % (size / 64)) * 64;
        *off_b = ((size_t)rand() % (size / 64)) * 64;
    } while (*off_a == *off_b);
}
