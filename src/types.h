#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define eprintf(...) fprintf(stderr, __VA_ARGS__)

typedef uint8_t byte;
typedef int8_t sbyte;
typedef uint16_t hword;
typedef int16_t shword;
typedef uint32_t word;
typedef int32_t sword;
typedef uint64_t dword;
typedef int64_t sdword;

#endif