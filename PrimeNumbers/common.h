#pragma once
#include "EventImpl.h"

#define PRINT_STATS(msg, ...) printf(msg ".\n", __VA_ARGS__);
//#define PRINT_THEAD_STATS(msg, ...) printf(msg ".\n", __VA_ARGS__);
//#define PRINT_ONELINE_STATS(msg, ...) printf(msg "\n", __VA_ARGS__);

#ifdef _DEBUG
//#if 1
#define PRINT_PROGRESS(msg, ...) printf("[PROGRESS #%d] " msg ".\n", __VA_ARGS__);
#define PRINT_ANSWER(msg, ...) printf("[ANSWER #%d] " msg ".\n", __VA_ARGS__);
#define PRINT_SOFT_WAIT(msg, ...) printf("[SOFT WAIT #%d] " msg ".\n", __VA_ARGS__);
#define PRINT_HARD_WAIT(msg, ...) printf("[HARD WAIT #%d] " msg ".\n", __VA_ARGS__);
#define PRINT_SOFT_WAIT_LATENCY(msg, ...) printf("[SOFT WAIT LATENCY #%d] " msg ".\n", __VA_ARGS__);
#define PRINT_HARD_WAIT_LATENCY(msg, ...) printf("[HARD WAIT LATENCY #%d] " msg ".\n", __VA_ARGS__);
#define PRINT_RELEASE(msg, ...) printf("[RELEASE #%d] " msg ".\n", __VA_ARGS__);
#endif


#ifndef PRINT_PROGRESS
#define PRINT_PROGRESS(msg, ...)
#endif // !PRINT_PROGRESS

#ifndef  PRINT_ANSWER
#define PRINT_ANSWER(msg, ...)
#endif // ! PRINT_ANSWER

#ifndef PRINT_SOFT_WAIT
#define PRINT_SOFT_WAIT(msg, ...)
#endif // !PRINT_SOFT_WAIT

#ifndef PRINT_SOFT_WAIT_LATENCY
#define PRINT_SOFT_WAIT_LATENCY(msg, ...)
#endif // !PRINT_SOFT_WAIT_LATENCY

#ifndef PRINT_HARD_WAIT
#define PRINT_HARD_WAIT(msg, ...)
#endif // !PRINT_HARD_WAIT

#ifndef PRINT_HARD_WAIT_LATENCY
#define PRINT_HARD_WAIT_LATENCY(msg, ...)
#endif // !PRINT_HARD_WAIT_LATENCY

#ifndef PRINT_RELEASE
#define PRINT_RELEASE(msg, ...)
#endif // !PRINT_RELEASE

#ifndef PRINT_STATS
#define PRINT_STATS(msg, ...)
#endif // !PRINT_STATS

#ifndef PRINT_THEAD_STATS
#define PRINT_THEAD_STATS(msg, ...)
#endif // !PRINT_THEAD_STATS

#ifndef PRINT_ONELINE_STATS
#define PRINT_ONELINE_STATS(msg, ...)
#endif // !PRINT_ONELINE_STATS

typedef unsigned long long ulong;

extern int SPIN_COUNT;
