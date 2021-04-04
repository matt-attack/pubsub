#pragma once
#include <inttypes.h>
#ifdef __cplusplus
extern "C"
{
#endif

void ps_sleep(unsigned int time_ms);

uint64_t GetTimeMs();

#ifdef __cplusplus
}
#endif
