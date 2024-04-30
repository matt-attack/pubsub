#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>

struct ps_sub_t;
struct ps_pub_t;
struct ps_message_definition_t;
struct ps_advertise_req_t;

#ifndef _WIN32
typedef unsigned int ps_socket_t;
#else
typedef int ps_socket_t;
#endif

// defines a transport implementation
//so when you request a subscription, you would request a transport type 
//then the pub can either do it, or fall back to default UDP transport
struct ps_event_set_t;
struct ps_transport_t;
struct ps_node_t;
struct ps_endpoint_t;
struct ps_client_t;
struct ps_subscribe_req_t;
struct ps_allocator_t;
typedef void(*ps_transport_fn_pub_t)(struct ps_transport_t* transport, struct ps_pub_t* publisher, struct ps_client_t* client, const void* message, uint32_t length);
typedef int(*ps_transport_fn_spin_t)(struct ps_transport_t* transport, struct ps_node_t* node);
typedef void(*ps_transport_fn_add_publisher_t)(struct ps_transport_t* transport, struct ps_pub_t* publisher);
typedef void(*ps_transport_fn_remove_publisher_t)(struct ps_transport_t* transport, struct ps_pub_t* publisher);
typedef void(*ps_transport_fn_subscribe_t)(struct ps_transport_t* transport, struct ps_sub_t* subscriber, struct ps_endpoint_t* ep, uint32_t transport_info);
typedef void(*ps_transport_fn_unsubscribe_t)(struct ps_transport_t* transport, struct ps_sub_t* subscriber);
typedef unsigned int(*ps_transport_fn_num_subscribers_t)(struct ps_transport_t* transport, struct ps_pub_t* publisher);
typedef unsigned int(*ps_transport_fn_add_wait_set_t)(struct ps_transport_t* transport, struct ps_event_set_t* events);
typedef void(*ps_transport_fn_destroy_t)(struct ps_transport_t* transport);
struct ps_transport_t
{
	unsigned short uuid;// unique id for this transport type, listed in advertisements for it
	unsigned int transport_info;// could be a port number or something else, listed in each advertisement
    void* impl;
	ps_transport_fn_pub_t pub;
	ps_transport_fn_spin_t spin;
	ps_transport_fn_num_subscribers_t subscriber_count;
	ps_transport_fn_subscribe_t subscribe;
	ps_transport_fn_unsubscribe_t unsubscribe;
	ps_transport_fn_add_publisher_t add_pub;
	ps_transport_fn_remove_publisher_t remove_pub;
    ps_transport_fn_add_wait_set_t add_wait_set;
    ps_transport_fn_destroy_t destroy;
};

typedef void(*ps_adv_cb_t)(const char* topic, const char* type, const char* node, const struct ps_advertise_req_t* adv_data, void* data);
typedef void(*ps_sub_cb_t)(const char* topic, const char* type, const char* node, const struct ps_subscribe_req_t* sub_data, void* data);
typedef void(*ps_msg_def_cb_t)(const struct ps_message_definition_t* definition, void* data);
typedef double(*ps_param_change_cb_t)(const char* name, double value, void* data);
typedef void(*ps_param_confirm_cb_t)(const char* name, double value, void* data);

#ifndef PUBSUB_REAL_TIME
#include <pubsub/Events.h>
#endif
struct ps_node_t
{
	const char* name;
	unsigned int num_pubs;
	struct ps_pub_t** pubs;
	unsigned int num_subs;
	struct ps_sub_t** subs;

	//lets just have one big socket for everything I guess for the moment
	ps_socket_t socket;
	ps_socket_t mc_socket;// socket for multicast
	uint16_t advertise_port;

	// my core socket port and address
	uint16_t port;
	uint32_t addr;
	uint32_t advertise_addr;// in network layout because we dont need it in host

	uint32_t group_id; // indicates which group this node is a part of

	int sub_index;

	//optional callbacks
	ps_adv_cb_t adv_cb;
	void* adv_cb_data;
	ps_sub_cb_t sub_cb;
	void* sub_cb_data;
	ps_msg_def_cb_t def_cb;
	void* def_cb_data;
	ps_param_change_cb_t param_cb;
	void* param_cb_data;
	ps_param_confirm_cb_t param_confirm_cb;
	void* param_confirm_cb_data;

	//implementation data
	unsigned long long _last_advertise;
	unsigned long long _last_check;


#ifndef PUBSUB_REAL_TIME
    struct ps_event_set_t events;
#endif

	uint32_t supported_transports;

#ifndef ARDUINO
	unsigned int num_transports;
	struct ps_transport_t* transports;
#endif
};

void ps_node_add_transport(struct ps_node_t* node, struct ps_transport_t* transport);

// describes where a message came from
struct ps_msg_info_t
{
	uint32_t address;
	uint16_t port;
};

//todo move elsewhere probably
#pragma pack(push)
#pragma pack(1)
struct ps_msg_header
{
	uint8_t pid;//packet type id
	uint32_t id;//stream id
	uint16_t seq;//sequence number
	uint8_t index;
	uint8_t count;
};
#pragma pack(pop)

enum ps_transport_protocols
{
	PS_TRANSPORT_UDP = 0,
	PS_TRANSPORT_TCP = 1,
	PS_TRANSPORT_SHARED_MEMORY = 2,
	PS_TRANSPORT_RESERVED = 1 << 5
};

enum ps_advertise_flags
{
	PS_ADVERTISE_LATCHED = 1
};

#pragma pack(push)
#pragma pack(1)
struct ps_advertise_req_t
{
	uint8_t id;
	int32_t addr;
	uint16_t port;// port for udp communications
	uint8_t flags;// members of ps_advertise_flags
	uint32_t transports;// bitmask showing supported protocols for this publisher
	uint32_t type_hash;// to see if the type is correct
	uint32_t group_id;// unique (hopefully) id that indicates which process this node is a part of
};
#pragma pack(pop)

#pragma pack(push)
#pragma pack(1)
struct ps_subscribe_req_t
{
  uint8_t id;
  int32_t addr;
  uint16_t port;
};
#pragma pack(pop)

#pragma pack(push)
#pragma pack(1)
struct ps_subscribe_accept_t
{
	uint8_t pid;// packet type identifier
	uint32_t sid; //stream id
	//message definition goes here...
};
#pragma pack(pop)

//not threadsafe, but thats obvious isnt it
// set broadcast to true to use that for advertising instead of multicast
// give null ip to autodetect
// set 
void ps_node_init(struct ps_node_t* node, const char* name, const char* ip, bool broadcast);

void ps_node_init_ex(struct ps_node_t* node, const char* name, const char* ip, bool broadcast, bool setup_ctrl_c_handler);

void ps_node_create_publisher(struct ps_node_t* node, const char* topic, const struct ps_message_definition_t* type, struct ps_pub_t* pub, bool latched);

void ps_node_create_subscriber(struct ps_node_t* node, const char* topic, const struct ps_message_definition_t* type,
	struct ps_sub_t* sub,
	unsigned int queue_size,//make >= 1
	bool want_message_def,//usually want false
	struct ps_allocator_t* allocator,//give null to use default
	bool ignore_local);// if ignore local is set, this node ignores publications from itself
							 // this facilitiates passing messages through shared memory


typedef void(*ps_subscriber_fn_cb_t)(void* message, unsigned int size, void* data, const struct ps_msg_info_t* info);
struct ps_subscriber_options
{
	unsigned int queue_size;
	bool ignore_local;
	struct ps_allocator_t* allocator;
	bool want_message_def;
	unsigned int skip;// skips to every nth message for throttling
	ps_subscriber_fn_cb_t cb;
	void* cb_data;
    uint32_t preferred_transport;// falls back to udp otherwise
};

void ps_subscriber_options_init(struct ps_subscriber_options* options);

void ps_node_create_subscriber_adv(struct ps_node_t* node, const char* topic, const struct ps_message_definition_t* type,
	struct ps_sub_t* sub,
	const struct ps_subscriber_options* options);

void ps_node_create_subscriber_cb(struct ps_node_t* node, const char* topic, const struct ps_message_definition_t* type,
	struct ps_sub_t* sub,
	ps_subscriber_fn_cb_t cb,
	void* cb_data,
	bool want_message_def,
	struct ps_allocator_t* allocator,
	bool ignore_local
);

int ps_node_spin(struct ps_node_t* node);

// Functions to wait on a node
#ifndef PUBSUB_REAL_TIME
struct ps_event_set_t;
// waits for an event in the node to trigger before returning
// note this creates and destroys events so is probably not the fastest
// way to wait, the functions below should be better
// used to avoid polling spin
int ps_node_wait(struct ps_node_t* node, unsigned int timeout_ms);

// creates all the events that would need to be waited on for this node
// returns the number added
int ps_node_create_events(struct ps_node_t* node, struct ps_event_set_t* events);
#endif

int serialize_string(char* data, const char* str);

// sends out a system query message for all nodes to advertise
void ps_node_system_query(struct ps_node_t* node);

void ps_node_query_message_definition(struct ps_node_t* node, const char* message);

// Sets a parameter, returns immediately rather than waiting for ack
void ps_node_set_parameter(struct ps_node_t* node, const char* name, double value);


int ps_okay();

void ps_node_destroy(struct ps_node_t* node);


// implementation types

enum
{
	PS_DISCOVERY_PROTOCOL_SUBSCRIBE_QUERY = 1,
	PS_DISCOVERY_PROTOCOL_ADVERTISE = 6,// 2 is old version, bumped to ensure no compatibility mismatches
	PS_DISCOVERY_PROTOCOL_UNSUBSCRIBE = 3,
	PS_DISCOVERY_PROTOCOL_QUERY_ALL = 4,
	PS_DISCOVERY_PROTOCOL_QUERY_MSG_DEFINITION = 5// used for getting message formats
};

#ifdef __cplusplus
}
#endif
