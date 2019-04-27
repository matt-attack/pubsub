#pragma once

#include "Serialization.h"

struct ps_endpoint_t;
struct ps_node_t;
struct ps_allocator_t;
struct ps_message_definition_t;
struct ps_sub_t
{
	ps_node_t* node;

	const char* topic;
	const char* type;
	//todo add hash

	bool want_message_definition;
	ps_message_definition_t received_message_def;

	int sub_id;

	ps_allocator_t* allocator;
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

void* ps_sub_deque(ps_sub_t* sub);

void ps_sub_destroy(ps_sub_t* sub);

void ps_send_subscribe(ps_sub_t* sub, ps_endpoint_t* ep);