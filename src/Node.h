#pragma once

struct ps_sub_t;
struct ps_pub_t;
struct ps_message_definition_t;

typedef void(*ps_adv_cb_t)(const char* topic, const char* type, const char* node, void* data);
typedef void(*ps_sub_cb_t)(const char* topic, const char* type, const char* node, void* data);
typedef void(*ps_msg_def_cb_t)(const ps_message_definition_t* definition);
struct ps_node_t
{
	const char* name;
	unsigned int num_pubs;
	ps_pub_t** pubs;
	unsigned int num_subs;
	ps_sub_t** subs;

	//lets just have one big socket for everything I guess for the moment
	int socket;
	int mc_socket;// socket for multicast
	unsigned short advertise_port;

	// my core socket port and address
	unsigned short port;
	unsigned int addr;
	unsigned int advertise_addr;// in network layout because we dont need it in host

	int sub_index;

	//optional callbacks
	ps_adv_cb_t adv_cb;
	ps_sub_cb_t sub_cb;
	ps_msg_def_cb_t def_cb;

	//implementation data
	unsigned long long _last_advertise;
};

//todo move elsewhere probably
struct ps_allocator_t
{
	void*(*alloc)(unsigned int size, void* context);
	void(*free)(void*);
	void* context;
};

//todo move elsewhere probably
#pragma pack(push)
#pragma pack(1)
struct ps_msg_header
{
	unsigned char pid;//packet type id
	unsigned int id;//stream id
	unsigned short seq;//sequence number
	unsigned char index;
	unsigned char count;
};
#pragma pack(pop)

#pragma pack(push)
#pragma pack(1)
struct ps_advertise_req_t
{
	char id;
	int addr;
	unsigned short port;
};
#pragma pack(pop)

#pragma pack(push)
#pragma pack(1)
struct ps_subscribe_accept_t
{
	char pid;// packet type identifier
	unsigned int sid; //stream id
	//message definition goes here...
};
#pragma pack(pop)

// defines a transport implementation
//so when you request a subscription, you would request a transport type 
//then the pub can either do it, or fall back to default UDP transport

typedef void(*ps_transport_fn_pub_t)(const char* topic, const char* type, const char* node, void* data);
typedef void(*ps_transport_fn_spin_t)(const char* topic, const char* type, const char* node, void* data);
struct ps_transport_t
{
	unsigned short uuid;// unique id for this transport type, listed in advertisements for it
	ps_transport_fn_pub_t pub;
	ps_transport_fn_spin_t spin;
};

//not threadsafe, but thats obvious isnt it
// set broadcast to true to use that for advertising instead of multicast
void ps_node_init(ps_node_t* node, const char* name, const char* ip = "", bool broadcast = false);

void ps_node_create_publisher(ps_node_t* node, const char* topic, const ps_message_definition_t* type, ps_pub_t* pub, bool latched = false);

void ps_node_create_subscriber(ps_node_t* node, const char* topic, const ps_message_definition_t* type,
	ps_sub_t* sub,
	unsigned int queue_size = 1,
	bool want_message_def = false,
	ps_allocator_t* allocator = 0);

int ps_node_spin(ps_node_t* node);

int serialize_string(char* data, const char* str);

// sends out a system query message for all nodes to advertise
void ps_node_system_query(ps_node_t* node);

void ps_node_query_message_definition(ps_node_t* node, const char* message);


int ps_okay();

void ps_node_destroy(ps_node_t* node);


// implementation types

enum
{
	PS_UDP_PROTOCOL_DATA = 2,
	PS_UDP_PROTOCOL_KEEP_ALIVE = 3,
	PS_UDP_PROTOCOL_SUBSCRIBE_ACCEPT = 4,
	PS_UDP_PROTOCOL_SUBSCRIBE_REQUEST = 1,
	PS_UDP_PROTOCOL_MESSAGE_DEFINITION = 8,
};

enum
{
	PS_DISCOVERY_PROTOCOL_SUBSCRIBE_QUERY = 1,
	PS_DISCOVERY_PROTOCOL_ADVERTISE = 2,
	PS_DISCOVERY_PROTOCOL_UNSUBSCRIBE = 3,
	PS_DISCOVERY_PROTOCOL_QUERY_ALL = 4,
	PS_DISCOVERY_PROTOCOL_QUERY_MSG_DEFINITION = 5// used for getting message formats
};