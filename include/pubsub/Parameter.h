#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

//#include <cstdlib>
#include <stdio.h>

#include <pubsub/Node.h>
#include <pubsub/Publisher.h>
#include <pubsub/Subscriber.h>
#include <pubsub/System.h>

#include <pubsub/Parameters.msg.h>

#include <math.h>

typedef void(*ps_param_fancy_cb_t)(const char* name, double value, void* data);

struct ps_parameters
{
	struct pubsub__Parameters msg;
	struct ps_pub_t param_pub;
	ps_param_fancy_cb_t callback;
	void* cb_data;
};

void ps_create_parameters(struct ps_node_t* node, struct ps_parameters* params_out, ps_param_fancy_cb_t callback, void* data);

void ps_destroy_parameters(struct ps_parameters* params);

void ps_add_parameter_double(struct ps_parameters* params,
 const char* name, const char* description,
 double value, double min, double max);

#ifdef __cplusplus
}
#endif
