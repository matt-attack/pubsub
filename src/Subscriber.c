#include <../include/pubsub/Subscriber.h>
#include <../include/pubsub/Publisher.h>
#include <../include/pubsub/Node.h>

#include <stdio.h>
#ifndef ANDROID
#include <stdlib.h>
#endif

#include <../include/pubsub/Net.h>

void ps_sub_enqueue(struct ps_sub_t* sub, void* out_data, int data_size, const struct ps_msg_info_t* message_info)
{
  // maybe todo, this doesnt do fifo very correctly
  // only does first newest than two old ones
  //add it to the fifo packet queue which always shows the n most recent packets
  //most recent is always first
  //so lets push to the front (highest open index or highest if full)
  if (sub->queue_size == 0)
  {
    sub->cb(out_data, data_size, sub->cb_data, message_info);
  }
  else if (sub->queue_size == sub->queue_len)
  {
    sub->queue[sub->queue_size - 1] = out_data;
  }
  else
  {
    sub->queue[sub->queue_len++] = out_data;
  }
}


void ps_send_subscribe(struct ps_sub_t* sub, const struct ps_endpoint_t* ep)
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

	int off = sizeof(struct ps_sub_req_header_t);
	off += serialize_string(&data[off], sub->topic);
	off += serialize_string(&data[off], sub->type ? sub->type->name : "");

	//add topic name, node, and 
	int sent_bytes = sendto(sub->node->socket, (const char*)data, off, 0, (struct sockaddr*)&address, sizeof(struct sockaddr_in));
	//printf("Subscribing...\n");
}

void ps_sub_destroy(struct ps_sub_t* sub)
{
	//send out unsub message
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

	int off = 7;
	off += serialize_string(&data[off], sub->topic);
	off += serialize_string(&data[off], sub->type ? sub->type->name : "");

	int sent_bytes = sendto(sub->node->socket, (const char*)data, off, 0, (struct sockaddr*)&address, sizeof(struct sockaddr_in));

	//remove it from my list of subs
	sub->node->num_subs--;
	struct ps_sub_t** old_subs = sub->node->subs;
    if (sub->node->num_subs == 0)
    {
      sub->node->subs = 0;
    }
    else
    {
	  sub->node->subs = (struct ps_sub_t**)malloc(sizeof(struct ps_sub_t*)*sub->node->num_subs);
	  int ind = 0;
	  for (unsigned int i = 0; i < sub->node->num_subs+1; i++)
	  {
		if (old_subs[i] == sub)
		{
		  //skip me
		}
		else
		{
          sub->node->subs[ind++] = old_subs[i];
		}
	  }
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

void* ps_sub_deque(struct ps_sub_t* sub)
{
	if (sub->queue_len == 0)
	{
		//printf("Warning: dequeued when there was nothing in queue\n");
		return 0;
	}
	
	void* data = sub->queue[--sub->queue_len];
	sub->queue[sub->queue_len] = 0;
	return data;
}
