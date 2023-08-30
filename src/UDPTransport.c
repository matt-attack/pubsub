#include <pubsub/UDPTransport.h>
#include <pubsub/Publisher.h>
#include <pubsub/Subscriber.h>

#include <pubsub/Net.h>

void ps_udp_publish(struct ps_pub_t* pub, struct ps_client_t* client, struct ps_msg_t* msg)
{
	// send da udp packet!
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(client->endpoint.address);
	address.sin_port = htons(client->endpoint.port);

	// todo split larger messages
	//need to add in the topic id
	struct ps_msg_header* hdr = (struct ps_msg_header*)msg->data;
	hdr->pid = PS_UDP_PROTOCOL_DATA;
	hdr->id = client->stream_id;
	hdr->seq = client->sequence_number++;
	hdr->index = 0;
	hdr->count = 1;// todo use me for larger packets

	int sent_bytes = sendto(pub->node->socket, (const char*)msg->data, msg->len + sizeof(struct ps_msg_header),
		0, (struct sockaddr*)&address, sizeof(struct sockaddr_in));
}

void ps_udp_subscribe(struct ps_sub_t* sub, const struct ps_endpoint_t* ep)
{
	// send da udp packet!
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(ep->address);
	address.sin_port = htons(ep->port);

	char data[1200];
	struct ps_sub_req_header_t* p = (struct ps_sub_req_header_t*)data;
	p->id = PS_UDP_PROTOCOL_SUBSCRIBE_REQUEST;
	p->addr = sub->node->addr;
	p->port = sub->node->port;
	p->sub_id = sub->sub_id;
	p->skip = sub->skip;
	
#ifdef PUBSUB_VERBOSE
	printf("Subscribing stream %i\n", p->sub_id);
#endif

	int off = sizeof(struct ps_sub_req_header_t);
	off += serialize_string(&data[off], sub->topic);
	off += serialize_string(&data[off], sub->type ? sub->type->name : "");

	//add topic name, node, and 
	int sent_bytes = sendto(sub->node->socket, (const char*)data, off, 0, (struct sockaddr*)&address, sizeof(struct sockaddr_in));
	//printf("Subscribing...\n");
}

void ps_udp_unsubscribe(struct ps_sub_t* sub)
{
	// send da udp packet!
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = sub->node->advertise_addr;
	address.sin_port = htons(sub->node->advertise_port);

	char data[1200];
	data[0] = PS_DISCOVERY_PROTOCOL_UNSUBSCRIBE;
	int* addr = (int*)&data[1];
	*addr = sub->node->addr;
	unsigned short* port = (unsigned short*)&data[5];
	*port = sub->node->port;
	
	unsigned int* stream_id = (unsigned int*)&data[7];
	*stream_id = sub->sub_id;
	
	int off = 11;
	off += serialize_string(&data[off], sub->topic);
	off += serialize_string(&data[off], sub->type ? sub->type->name : "");

	int sent_bytes = sendto(sub->node->socket, (const char*)data, off, 0, (struct sockaddr*)&address, sizeof(struct sockaddr_in));
}
