#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <stdio.h>

#if DEBUG_PRINT
#define DEBUG(...) printf("%s: ", __func__), printf(__VA_ARGS__), printf("\n")
#else
#define DEBUG(...)
#endif

#endif
