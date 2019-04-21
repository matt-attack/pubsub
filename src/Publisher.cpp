#include "Publisher.h"
#include "Node.h"

#include <stdio.h>

#include "Net.h"

void ps_pub_add_client(ps_pub_t* pub, const ps_client_t* client)
{
	// first make sure we dont add any duplicate clients
	for (int i = 0; i < pub->num_clients; i++)
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
	for (int i = 0; i < pub->num_clients - 1; i++)
	{
		pub->clients[i] = old_clients[i];
	}
	pub->clients[pub->num_clients - 1] = *client;
}

void ps_pub_remove_client(ps_pub_t* pub, const ps_client_t* client)
{
	// first make sure we dont add any duplicate clients
	bool found = false;
	for (int i = 0; i < pub->num_clients; i++)
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
	for (int i = 0; i < pub->num_clients; i++)
	{
		if (pub->clients[i].endpoint.address == client->endpoint.address
			&& pub->clients[i].endpoint.port == client->endpoint.port)
		{

		}
		else
		{
			pub->clients[i] = old_clients[pos];
		}
		pos++;
	}
}

void ps_pub_publish(ps_pub_t* pub, ps_msg_t* msg)
{
	for (int i = 0; i < pub->num_clients; i++)
	{
		ps_client_t* client = &pub->clients[i];
		// send da udp packet!
		sockaddr_in address;
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(client->endpoint.address);
		address.sin_port = htons(client->endpoint.port);

		//need to add in the topic id
		ps_msg_header* hdr = (ps_msg_header*)msg->data;
		hdr->pid = 2;
		hdr->id = client->stream_id;
		hdr->seq = client->sequence_number++;
		hdr->index = 0;
		hdr->count = 1;// todo use me for larger packets

		int sent_bytes = sendto(pub->node->socket, (const char*)msg->data, msg->len + sizeof(ps_msg_header), 0, (sockaddr*)&address, sizeof(sockaddr_in));

		//return sent_bytes == size;
	}
}

int ps_pub_get_subscriber_count(const ps_pub_t* pub)
{
	return pub->num_clients;
}