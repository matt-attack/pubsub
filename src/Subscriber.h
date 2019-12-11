#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>

#include "Serialization.h"
#include "Node.h"

struct ps_endpoint_t;
struct ps_node_t;
struct ps_allocator_t;
struct ps_message_definition_t;
struct ps_sub_t
{
	struct ps_node_t* node;

	const char* topic;
	const struct ps_message_definition_t* type;

	// for dynamic subscribers
	bool want_message_definition;
	struct ps_message_definition_t received_message_def;

	// ignores local publications
	bool ignore_local;

	int sub_id;

	struct ps_allocator_t* allocator;

	// used instead of a queue optionally
	ps_subscriber_fn_cb_t cb;
	void* cb_data;

	int queue_size;// maximum size of the queue
	int queue_len;
	void** queue;// pointers to each of the queue items
};

#pragma pack(push)
#pragma pack(1)
struct ps_sub_req_header_t
{
	char id;
	int addr;
	short port;
	int sub_id;
};
#pragma pack(pop)

void ps_sub_enqueue(struct ps_sub_t* sub, void* message);

// if the subscriber was initialized with a type this returns decoded messages
void* ps_sub_deque(struct ps_sub_t* sub);

void ps_sub_destroy(struct ps_sub_t* sub);

void ps_send_subscribe(struct ps_sub_t* sub, struct ps_endpoint_t* ep);

#ifdef __cplusplus
}
#endif