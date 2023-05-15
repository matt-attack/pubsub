#pragma once
#include <inttypes.h>
#ifdef __cplusplus
extern "C"
{
#endif

void ps_sleep(unsigned int time_ms);

uint64_t GetTimeMs();

#ifdef ARDUINO
unsigned long ps_get_tick_count();
#else
uint64_t ps_get_tick_count();
#endif

void ps_print_socket_error(const char* description);

void ps_networking_init();

#ifdef __cplusplus
}
#endif
