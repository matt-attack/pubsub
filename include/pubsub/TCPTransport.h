#ifndef _PUBSUB_TCP_TRANSPORT_HEADER
#define _PUBSUB_TCP_TRANSPORT_HEADER

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

// Must be included first on windows
#include <pubsub/Net.h>

#include <pubsub/Node.h>
#include <pubsub/Publisher.h>
#include <pubsub/Subscriber.h>
#include <pubsub/System.h>
#include <pubsub/UDPTransport.h>
//#include <pubsub/Net.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef __unix__
#include <signal.h>
#endif

#define PUBSUB_TCP_TRANSPORT 1

enum
{
	PS_TCP_PROTOCOL_DATA = PS_UDP_PROTOCOL_DATA,
	PS_TCP_PROTOCOL_MESSAGE_DEFINITION = 0x03,
};

/*

typedef void(*ps_transport_fn_pub_t)(struct ps_transport_t* transport, struct ps_pub_t* publisher, void* message);
typedef void(*ps_transport_fn_spin_t)(struct ps_transport_t* transport, struct ps_node_t* node);
typedef void(*ps_transport_fn_add_publisher_t)(struct ps_transport_t* transport, struct ps_pub_t* publisher);
typedef void(*ps_transport_fn_remove_publisher_t)(struct ps_transport_t* transport, struct ps_pub_t* publisher);
typedef void(*ps_transport_fn_subscribe_t)(struct ps_transport_t* transport, struct ps_sub_t* subscriber, struct ps_endpoint_t* ep);
typedef void(*ps_transport_fn_unsubscribe_t)(struct ps_transport_t* transport, struct ps_sub_t* subscriber);
typedef unsigned int(*ps_transport_fn_num_subscribers_t)(struct ps_transport_t* transport, struct ps_pub_t* publisher);
typedef unsigned int(*ps_transport_fn_add_wait_set_t)(struct ps_transport_t* transport, struct ps_event_set_t* events);
struct ps_transport_t
{
    unsigned short uuid;// unique id for this transport type, listed in advertisements for it
    ps_transport_fn_pub_t pub;
    ps_transport_fn_spin_t spin;
    ps_transport_fn_num_subscribers_t subscriber_count;
    ps_transport_fn_subscribe_t subscribe;
    ps_transport_fn_unsubscribe_t unsubscribe;
    ps_transport_fn_add_publisher_t add_pub;
    ps_transport_fn_remove_publisher_t remove_pub;
    ps_transport_fn_add_wait_set_t add_wait_set;
    void* impl;
};*/

struct ps_tcp_transport_connection
{
  int socket;
  struct ps_endpoint_t endpoint;

  struct ps_sub_t* subscriber;

  bool connecting;
  bool waiting_for_header;
  int packet_size;

  char packet_type;
  int current_size;
  char* packet_data;
};

struct ps_tcp_client_queued_message_t
{
  struct ps_msg_ref_t* msg;
};

struct ps_tcp_client_t
{
  int socket;
  bool needs_removal;

  struct ps_pub_t* publisher;

  int32_t current_packet_size;
  int32_t desired_packet_size;
  char* packet_data;

  struct ps_msg_ref_t* queued_message;
  int32_t queued_message_length;
  int32_t queued_message_written;

  // this stores queued messages > 1
  int32_t num_queued_messages;
  struct ps_tcp_client_queued_message_t* queued_messages;
};

struct ps_tcp_transport_impl
{
  int socket;

  struct ps_node_t* node;

  struct ps_tcp_client_t* clients;
  int num_clients;

  struct ps_tcp_transport_connection* connections;
  int num_connections;
};

void ps_tcp_transport_destroy(struct ps_transport_t* transport);

void ps_tcp_transport_init(struct ps_transport_t* transport, struct ps_node_t* node);

#ifdef __cplusplus
}
#endif

#endif
