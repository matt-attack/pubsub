
// This file contains easy bindings for using this from c# or other languages

#include <stdbool.h>

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT 
#endif

EXPORT int ps_create_node(const char* name, const char* ip, bool broadcast);

EXPORT void ps_spin_node(int node);

EXPORT void ps_destroy_node(int node);

EXPORT int ps_create_publisher(int node, const char* topic, const char* definition, bool latched);

// publishes already encoded messages
EXPORT void ps_publish(int pub, const void* msg, int len);

EXPORT bool ps_is_okay();

EXPORT void ps_set_parameter_cb(int node, void* cb);