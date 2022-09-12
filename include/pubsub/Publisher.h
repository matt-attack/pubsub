#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>

#include "Serialization.h"

struct ps_node_t;
struct ps_message_definition_t;

struct ps_endpoint_t
{
	unsigned short port;
	int address;
	//bool multicast;// this is probably unnecessary
};

// publisher client to network to
struct ps_client_t
{
	struct ps_endpoint_t endpoint;
	unsigned short sequence_number;// sequence of the networked packets, incremented with each one
	unsigned long long last_keepalive;// timestamp of the last keepalive message, used to know when to deactiveate this connection
	unsigned int stream_id;// user-unique identifier of what topic this came from
	unsigned int modulo;
    struct ps_transport_t* transport;
};

struct ps_pub_t
{
	const char* topic;

	const struct ps_message_definition_t* message_definition;

	struct ps_node_t* node;
	unsigned int num_clients;
	struct ps_client_t* clients;

	bool latched;// todo make this an enum of options if we add more

	struct ps_msg_t last_message;//only used if latched
	unsigned int sequence_number;
};

// adds a client to a publisher
bool ps_pub_add_client(struct ps_pub_t* pub, const struct ps_client_t* client);

// adds a fake client to the publisher which doesnt time out to just send the message to a given UDP address and stream id
// make sure the stream id is unique for the given endpoint or the enduser wont be able to identify the topic
void ps_pub_add_endpoint_client(struct ps_pub_t* pub, const struct ps_endpoint_t* endpoint, const unsigned int stream_id);

void ps_pub_remove_client(struct ps_pub_t* pub, const struct ps_client_t* client);

// not threadsafe
// also eats the message and frees it at the end
void ps_pub_publish(struct ps_pub_t* pub, struct ps_msg_t* msg);

void ps_pub_publish_ez(struct ps_pub_t* pub, void* msg);

unsigned int ps_pub_get_subscriber_count(const struct ps_pub_t* pub);

void ps_pub_destroy(struct ps_pub_t* pub);

#ifdef __cplusplus
}
#endif
