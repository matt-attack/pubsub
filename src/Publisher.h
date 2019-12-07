#pragma once

#include "../src/Serialization.h"

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
	ps_endpoint_t endpoint;
	unsigned short sequence_number;// sequence of the networked packets, incremented with each one
	unsigned long long last_keepalive;//timestamp of the last keepalive message, used to know when to deactiveate this connection
	unsigned int stream_id;
};

struct ps_pub_t
{
	const char* topic;

	const ps_message_definition_t* message_definition;

	ps_node_t* node;
	unsigned int num_clients;
	ps_client_t* clients;
	bool latched;// todo make this an enum of options if we add more
	ps_msg_t last_message;//only used if latched
};

// adds a client to a publisher
void ps_pub_add_client(ps_pub_t* pub, const ps_client_t* client);

void ps_pub_remove_client(ps_pub_t* pub, const ps_client_t* client);

// not threadsafe
// also eats the message and frees it at the end
void ps_pub_publish(ps_pub_t* pub, ps_msg_t* msg);

void ps_pub_publish_ez(ps_pub_t* pub, void* msg);

int ps_pub_get_subscriber_count(const ps_pub_t* pub);

void ps_pub_destroy(ps_pub_t* pub);