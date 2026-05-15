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

	// for dynamic subscribers (type == 0)
	struct ps_message_definition_t received_message_def;

	// ignores local publications
	bool ignore_local;

	int sub_id;

	struct ps_allocator_t* allocator;

	ps_subscriber_fn_cb_t cb;
	ps_subscriber_fn_cb_t cb_raw;
	void* cb_data;

	int preferred_transport;// udp or tcp, or -1 for no preference

	unsigned int skip;
};

#pragma pack(push)
#pragma pack(1)
struct ps_sub_req_header_t
{
	char id;
	int addr;
	short port;
	int sub_id;
	unsigned int skip;
};
#pragma pack(pop)

void ps_sub_receive(struct ps_sub_t* sub, void* encoded_message, int data_size, bool is_reference, const struct ps_msg_info_t* message_info);

void ps_sub_destroy(struct ps_sub_t* sub);

void ps_udp_subscribe(struct ps_sub_t* sub, const struct ps_endpoint_t* ep);

#ifdef __cplusplus
}
#endif
