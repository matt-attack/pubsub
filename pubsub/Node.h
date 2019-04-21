#pragma once

struct ps_sub_t;
struct ps_pub_t;

typedef void(*ps_adv_cb_t)(const char* topic, const char* type, const char* node, void* data);
typedef void(*ps_sub_cb_t)(const char* topic, const char* type, const char* node, void* data);
struct ps_node_t
{
	const char* name;
	int num_pubs;
	ps_pub_t** pubs;
	int num_subs;
	ps_sub_t** subs;

	//lets just have one big socket for everything I guess for the moment
	int socket;
	int mc_socket;// socket for multicast
	unsigned short advertise_port;

	// my core socket port and address
	unsigned short port;
	unsigned int addr;
	unsigned int mc_addr;// in network layout because we dont need it in host

	int sub_index;

	//optional callbacks
	ps_adv_cb_t adv_cb;
	ps_sub_cb_t sub_cb;
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

//not threadsafe, but thats obvious isnt it
void ps_node_init(ps_node_t* node, const char* name, const char* ip);

void ps_node_create_publisher(ps_node_t* node, const char* topic, const char* type, ps_pub_t* pub);

void ps_node_create_subscriber(ps_node_t* node, const char* topic, const char* type,
	ps_sub_t* sub,
	unsigned int queue_size = 1,
	ps_allocator_t* allocator = 0);

int ps_node_spin(ps_node_t* node);

int serialize_string(char* data, const char* str);

// sends out a system query message for all nodes to advertise
void ps_node_system_query(ps_node_t* node);


void ps_node_set_advertise_cb(ps_node_t* node, ps_adv_cb_t cb, void* data);
