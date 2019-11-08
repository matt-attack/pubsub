#include "Publisher.h"
#include "Node.h"

#include <stdio.h>

#include "Net.h"

inline void ps_pub_publish_client(ps_pub_t* pub, ps_client_t* client, ps_msg_t* msg)
{
	// send da udp packet!
	sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(client->endpoint.address);
	address.sin_port = htons(client->endpoint.port);

	//need to add in the topic id
	ps_msg_header* hdr = (ps_msg_header*)msg->data;
	hdr->pid = PS_UDP_PROTOCOL_DATA;// todo use enums
	hdr->id = client->stream_id;
	hdr->seq = client->sequence_number++;
	hdr->index = 0;
	hdr->count = 1;// todo use me for larger packets

	int sent_bytes = sendto(pub->node->socket, (const char*)msg->data, msg->len + sizeof(ps_msg_header), 0, (sockaddr*)&address, sizeof(sockaddr_in));
}

void ps_pub_add_client(ps_pub_t* pub, const ps_client_t* client)
{
	// first make sure we dont add any duplicate clients
	for (unsigned int i = 0; i < pub->num_clients; i++)
	{
		if (pub->clients[i].endpoint.address == client->endpoint.address
			&& pub->clients[i].endpoint.port == client->endpoint.port)
		{
			printf("We already have this client, ignoring request\n");
			return;
		}
	}
	pub->num_clients++;

	ps_client_t* old_clients = pub->clients;
	pub->clients = (ps_client_t*)malloc(sizeof(ps_client_t)*pub->num_clients);
	for (unsigned int i = 0; i < pub->num_clients - 1; i++)
	{
		pub->clients[i] = old_clients[i];
	}
	pub->clients[pub->num_clients - 1] = *client;

	//okay, if we are latched, send it our last message
	if (pub->last_message.data && pub->latched)
	{
		ps_pub_publish_client(pub, &pub->clients[pub->num_clients - 1], &pub->last_message);
	}
}

void ps_pub_remove_client(ps_pub_t* pub, const ps_client_t* client)
{
	// first make sure we dont add any duplicate clients
	bool found = false;
	for (unsigned int i = 0; i < pub->num_clients; i++)
	{
		if (pub->clients[i].endpoint.address == client->endpoint.address
			&& pub->clients[i].endpoint.port == client->endpoint.port)
		{
			printf("Found the client, removing it\n");
			found = true;
			break;
		}
	}

	if (found == false)
	{
		printf("Not removing client because we dont have it.\n");
		return;
	}
	pub->num_clients--;

	ps_client_t* old_clients = pub->clients;
	pub->clients = (ps_client_t*)malloc(sizeof(ps_client_t)*pub->num_clients);
	int pos = 0;
	for (unsigned int i = 0; i < pub->num_clients+1; i++)
	{
		if (old_clients[i].endpoint.address == client->endpoint.address
			&& old_clients[i].endpoint.port == client->endpoint.port)
		{

		}
		else
		{
			pub->clients[pos++] = old_clients[i];
		}
	}
}

void ps_pub_publish_ez(ps_pub_t* pub, void* msg)
{
	ps_msg_t data = pub->message_definition->encode(0, msg);

	ps_pub_publish(pub, &data);
}

void ps_pub_publish(ps_pub_t* pub, ps_msg_t* msg)
{
	for (unsigned int i = 0; i < pub->num_clients; i++)
	{
		ps_client_t* client = &pub->clients[i];
        
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
}

int ps_pub_get_subscriber_count(const ps_pub_t* pub)
{
	return pub->num_clients;
}

void ps_pub_destroy(ps_pub_t* pub)
{
	free(pub->clients);

	//remove it from my list of subs
	pub->node->num_pubs--;
	ps_pub_t** old_pubs = pub->node->pubs;
	pub->node->pubs = (ps_pub_t**)malloc(sizeof(ps_pub_t*)*pub->node->num_pubs);
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
	free(old_pubs);

	pub->clients = 0;
	pub->num_clients = 0;
	pub->node = 0;
	pub->message_definition = 0;
	pub->topic = 0;
}