#include <pubsub/Subscriber.h>
#include <pubsub/Publisher.h>
#include <pubsub/Node.h>
#include <pubsub/UDPTransport.h>

#include <stdio.h>
#ifndef ANDROID
#include <stdlib.h>
#endif
#include <string.h>

#include <pubsub/Net.h>

void ps_sub_receive(struct ps_sub_t* sub, void* encoded_message, int data_size, bool is_reference, const struct ps_msg_info_t* message_info)
{
  // if is_reference is true, we must make a copy for the subscriber to own
  // okay, so how do we let the callback specify if it wants a copy or not?
  
  //todo make this not always require owning the data
  //how do I release a "loaned" message? also, how do I loan?
  if (sub->cb_raw)
  {
    // todo can avoid the copy if the allocator is used
    void* out_data;
    if (is_reference)
    {
      out_data = sub->allocator->alloc(data_size, sub->allocator->context);
	    memcpy(out_data, encoded_message, data_size);    
    }
    else
    {
      out_data = encoded_message;
	  }
	  sub->cb_raw(out_data, data_size, sub->cb_data, message_info);
  }
  
  if (sub->cb)
  {
    void* out_data = sub->type->decode(encoded_message, sub->allocator);
    sub->cb(out_data, data_size, sub->cb_data, message_info);
  }
  
  if (!is_reference && !sub->cb_raw)
  {
    sub->allocator->free(encoded_message, sub->allocator->context);
  }
}

void ps_sub_destroy(struct ps_sub_t* sub)
{
	// unsubscribe from udp transport
	ps_udp_unsubscribe(sub);
	
	// unsubcribe from all other transports
	for (unsigned int i = 0; i < sub->node->num_transports; i++)
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
}
