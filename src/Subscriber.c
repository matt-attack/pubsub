#include <pubsub/Subscriber.h>
#include <pubsub/Publisher.h>
#include <pubsub/Node.h>
#include <pubsub/UDPTransport.h>

#include <stdio.h>
#ifndef ANDROID
#include <stdlib.h>
#endif

#include <pubsub/Net.h>

void ps_sub_enqueue(struct ps_sub_t* sub, void* data, int data_size, const struct ps_msg_info_t* message_info)
{
  // Implement a LIFO queue. Is this the best option?
  int new_start = sub->queue_start - 1;
  if (new_start < 0)
  {
  	new_start += sub->queue_size;
  }
  
  // If no queue size, just run the callback immediately
  if (sub->queue_size == 0)
  {
    sub->cb(data, data_size, sub->cb_data, message_info);
  }
  // Handle replacement if the queue is full
  else if (sub->queue_size == sub->queue_len)
  {
    // we'll replace the item at the back by shifting the queue around
    free(sub->queue[sub->queue_start]);
    // add at the front
    sub->queue[new_start] = data;
    sub->queue_start = new_start;
  }
  else
  {
    // add to the front
    sub->queue_len++;
    sub->queue[new_start] = data;
    sub->queue_start = new_start;
  }
}

void ps_sub_destroy(struct ps_sub_t* sub)
{
	// unsubscribe from udp transport
	ps_udp_unsubscribe(sub);
	
	// unsubcribe from all other transports
	for (int i = 0; i < sub->node->num_transports; i++)
	{
		sub->node->transports[i].unsubscribe(&sub->node->transports[i], sub);
    }

	//remove it from my list of subs
	sub->node->num_subs--;
    if (sub->node->num_subs == 0)
    {
      free(sub->node->subs);
      sub->node->subs = 0;
    }
    else
    {
	  struct ps_sub_t** old_subs = sub->node->subs;
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
      free(old_subs);
    }

	// free any queued up received messages and the queue itself
	for (int i = 0; i < sub->queue_len; i++)
	{
		int index = (sub->queue_start + i)%sub->queue_size;
		if (sub->queue[index] != 0)
			free(sub->queue[index]);
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

	// we are dequeueing, so remove the newest first (from the front)
	sub->queue_len--;
	
	void* data = sub->queue[sub->queue_start];
	sub->queue[sub->queue_start] = 0;
	
	int new_start = sub->queue_start+1;
	if (new_start >= sub->queue_size)
	{
		new_start -= sub->queue_size;
	}
	sub->queue_start = new_start;
	return data;
}
