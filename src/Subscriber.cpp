#include "Subscriber.h"
#include "Publisher.h"
#include "Node.h"

#include <stdio.h>

#include "Net.h"


void ps_send_subscribe(ps_sub_t* sub, ps_endpoint_t* ep)
{
	// send da udp packet!
	sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(ep->address);// todo put this port in a global
	address.sin_port = htons(ep->port);

	char data[1200];
	ps_sub_req_header_t* p = (ps_sub_req_header_t*)data;
	p->id = 1;
	p->addr = sub->node->addr;
	p->port = sub->node->port;// todo need to assign port uniquely per node also replace me with my port
							//also add other indo...
	p->sub_id = sub->sub_id;

	int off = sizeof(ps_sub_req_header_t);
	off += serialize_string(&data[off], sub->topic);
	off += serialize_string(&data[off], sub->type);

	//add topic name, node, and 
	int sent_bytes = sendto(sub->node->socket, (const char*)data, off, 0, (sockaddr*)&address, sizeof(sockaddr_in));
	printf("Subscribing...\n");
}

void ps_sub_destroy(ps_sub_t* sub)
{
	//send out unsub message
	// send da udp packet!
	sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = sub->node->advertise_addr;
	address.sin_port = htons(sub->node->advertise_port);

	char data[1000];
	data[0] = 3;
	int* addr = (int*)&data[1];
	*addr = sub->node->addr;
	unsigned short* port = (unsigned short*)&data[5];
	*port = sub->node->port;

	int off = 7;
	off += serialize_string(&data[off], sub->topic);
	off += serialize_string(&data[off], sub->type);

	int sent_bytes = sendto(sub->node->socket, (const char*)data, off, 0, (sockaddr*)&address, sizeof(sockaddr_in));

	//remove it from my list of subs
	sub->node->num_subs--;
	ps_sub_t** old_subs = sub->node->subs;
	sub->node->subs = (ps_sub_t**)malloc(sizeof(ps_sub_t*)*sub->node->num_subs);
	int ind = 0;
	for (int i = 0; i < sub->node->num_subs; i++)
	{
		if (old_subs[ind] == sub)
		{
			//skip me
		}
		else
		{
			sub->node->subs[i] = old_subs[ind];
		}
		ind++;
	}
	free(old_subs);

	//free any queued up messages and our queue
	for (int i = 0; i < sub->queue_size; i++)
	{
		if (sub->queue[i] != 0)
			free(sub->queue[i]);
	}
	free(sub->queue);
}

void* ps_sub_deque(ps_sub_t* sub)
{
	if (sub->queue_len == 0)
	{
		printf("Warning: dequeued when there was nothing in queue\n");
		return 0;
	}
	sub->queue_len--;
	return sub->queue[0];
}