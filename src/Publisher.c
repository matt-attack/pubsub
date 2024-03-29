#include <pubsub/Publisher.h>
#include <pubsub/Node.h>
#include <pubsub/UDPTransport.h>

#include <stdio.h>
#ifndef ANDROID
#include <stdlib.h>
#endif

#include <pubsub/Net.h>


void ps_pub_publish_client(struct ps_pub_t* pub, struct ps_client_t* client, struct ps_msg_t* msg)
{
	// Skip messages if desired by the client
	if (client->modulo > 0)
	{
		if (pub->sequence_number % client->modulo != 0)
		{
			return;
		}
	}

    if (client->transport)
    {
      //printf("publishing to custom transport\n");
      client->transport->pub(client->transport, pub, client, msg->data, msg->len);
      return;
    }

	// Send it via UDP transport
	ps_udp_publish(pub, client, msg);
}

bool ps_pub_add_client(struct ps_pub_t* pub, const struct ps_client_t* client)
{
#ifdef PUBSUB_VERBOSE
	printf("Want to add client stream %i\n", client->stream_id);
#endif
	// first make sure we dont add any duplicate clients
	for (unsigned int i = 0; i < pub->num_clients; i++)
	{
		if (pub->clients[i].endpoint.address == client->endpoint.address
			&& pub->clients[i].endpoint.port == client->endpoint.port
			&& pub->clients[i].stream_id == client->stream_id)
		{
#ifdef PUBSUB_VERBOSE
			printf("We already have this client, ignoring request\n");
#endif
			return false;
		}
	}
	pub->num_clients++;
	
#ifdef PUBSUB_VERBOSE
	printf("Adding client stream %i\n", client->stream_id);
#endif

	struct ps_client_t* old_clients = pub->clients;
	pub->clients = (struct ps_client_t*)malloc(sizeof(struct ps_client_t)*pub->num_clients);
	for (unsigned int i = 0; i < pub->num_clients - 1; i++)
	{
		pub->clients[i] = old_clients[i];
	}
	pub->clients[pub->num_clients - 1] = *client;

	// todo this is probably the wrong spot for this
	// If we are latched, send the new client our last message
	if (pub->last_message.data && pub->latched)
	{
        //printf("publishing latched\n");
		ps_pub_publish_client(pub, &pub->clients[pub->num_clients - 1], &pub->last_message);
	}
    return true;
}

void ps_pub_add_endpoint_client(struct ps_pub_t* pub, const struct ps_endpoint_t* endpoint, const unsigned int stream_id)
{
	struct ps_client_t client;
	client.endpoint = *endpoint;
	client.last_keepalive = 0;// zero means to never time out this endpoint
	client.sequence_number = 0;
	client.stream_id = stream_id;
	client.modulo = 0;
	client.transport = 0;

	ps_pub_add_client(pub, &client);
}

void ps_pub_remove_client(struct ps_pub_t* pub, const struct ps_client_t* client)
{
	// first make sure we dont add any duplicate clients
	bool found = false;
	for (unsigned int i = 0; i < pub->num_clients; i++)
	{
		if (pub->clients[i].endpoint.address == client->endpoint.address
			&& pub->clients[i].endpoint.port == client->endpoint.port
			&& pub->clients[i].stream_id == client->stream_id)
		{
			//printf("Found the client, removing it\n");
			found = true;
			break;
		}
	}

	if (found == false)
	{
		//printf("Not removing client because we dont have it.\n");
		return;
	}
	pub->num_clients--;

	struct ps_client_t* old_clients = pub->clients;
	pub->clients = (struct ps_client_t*)malloc(sizeof(struct ps_client_t)*pub->num_clients);
	int pos = 0;
	for (unsigned int i = 0; i < pub->num_clients+1; i++)
	{
		if (old_clients[i].endpoint.address == client->endpoint.address
			&& old_clients[i].endpoint.port == client->endpoint.port
			&& old_clients[i].stream_id == client->stream_id)
		{

		}
		else
		{
			pub->clients[pos++] = old_clients[i];
		}
	}
}

void ps_pub_publish_ez(struct ps_pub_t* pub, void* msg)
{
	if (pub->num_clients > 0 || pub->latched)
	{
		struct ps_msg_t data = pub->message_definition->encode(0, msg);

		ps_pub_publish(pub, &data);
	}
}

void ps_pub_publish(struct ps_pub_t* pub, struct ps_msg_t* msg)
{
	pub->sequence_number++;

	for (unsigned int i = 0; i < pub->num_clients; i++)
	{
		struct ps_client_t* client = &pub->clients[i];
        
		ps_pub_publish_client(pub, client, msg);
	}

	if (pub->latched)
	{
		if (pub->last_message.data)
		{
			//free the old and add the new
			free(pub->last_message.data);// todo use allocator
		}
		pub->last_message = *msg;
	}
	else
	{
		free(msg->data);// todo use allocator
	}
}

unsigned int ps_pub_get_subscriber_count(const struct ps_pub_t* pub)
{
	return pub->num_clients;
}

void ps_pub_destroy(struct ps_pub_t* pub)
{
	free(pub->clients);

	//remove it from the node's list of pubs
	pub->node->num_pubs--;
	struct ps_pub_t** old_pubs = pub->node->pubs;
    if (pub->node->num_pubs)
    {
	  pub->node->pubs = (struct ps_pub_t**)malloc(sizeof(struct ps_pub_t*)*pub->node->num_pubs);
	  int ind = 0;
	  for (unsigned int i = 0; i < pub->node->num_pubs+1; i++)
	  {
		  if (old_pubs[i] == pub)
		  {
			 //skip me
		  }
		  else
		  {
			  pub->node->pubs[ind++] = old_pubs[i];
		  }
	  }
    }
    else
    {
      pub->node->pubs = 0;
    }
	free(old_pubs);

    // free my latched message
    if (pub->last_message.data)
	{
		free(pub->last_message.data);// todo use allocator
	}

	pub->clients = 0;
	pub->num_clients = 0;
	pub->node = 0;
	pub->message_definition = 0;
	pub->topic = 0;
}
