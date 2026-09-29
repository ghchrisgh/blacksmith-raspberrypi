#pragma once

// util.h

#include <stddef.h>

// Generate two different cache-line-aligned offsets within [0, size)
void rand_pair(size_t size, size_t* off_a, size_t* off_b);
