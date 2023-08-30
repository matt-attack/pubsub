#ifndef _PUBSUB_UDP_TRANSPORT_HEADER
#define _PUBSUB_UDP_TRANSPORT_HEADER

enum
{
	PS_UDP_PROTOCOL_DATA = 2,
	PS_UDP_PROTOCOL_KEEP_ALIVE = 3,
	PS_UDP_PROTOCOL_SUBSCRIBE_ACCEPT = 4,
	PS_UDP_PROTOCOL_SUBSCRIBE_REQUEST = 1,
	PS_UDP_PROTOCOL_MESSAGE_DEFINITION = 8,
	
	PS_UDP_PROTOCOL_PARAM_CHANGE = 10,
	PS_UDP_PROTOCOL_PARAM_ACK = 11
};

struct ps_endpoint_t;
struct ps_sub_t;
struct ps_pub_t;
struct ps_msg_t;
struct ps_client_t;

void ps_udp_subscribe(struct ps_sub_t* sub, const struct ps_endpoint_t* ep);

void ps_udp_unsubscribe(struct ps_sub_t* sub);

void ps_udp_publish(struct ps_pub_t* pub, struct ps_client_t* client, struct ps_msg_t* msg);

#endif
