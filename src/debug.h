#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <stdio.h>

#ifndef DEBUG
#define DEBUG(...) printf("%s: ", __func__), printf(__VA_ARGS__), printf("\n")
#endif

#endif
