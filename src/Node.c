#include "Node.h"
#include "Publisher.h"
#include "Subscriber.h"
#include "Serialization.h"

#include <stdio.h>

#include "Net.h"

#include <string.h>

#ifdef ARDUINO
#include "Arduino.h"
#else
#include <stdlib.h>
#endif

// sends out a system query message for all nodes to advertise
void ps_node_system_query(struct ps_node_t* node)
{
	// send da udp packet!
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = node->advertise_addr;
	address.sin_port = htons(node->advertise_port);

	char data[1400];
	data[0] = PS_DISCOVERY_PROTOCOL_QUERY_ALL;
	int* addr = (int*)&data[1];
	*addr = node->addr;
	unsigned short* port = (unsigned short*)&data[5];
	*port = node->port;

	int off = 7;
	off += serialize_string(&data[off], node->name);

	int sent_bytes = sendto(node->socket, (const char*)data, off, 0, (struct sockaddr*)&address, sizeof(struct sockaddr_in));
}

void ps_node_query_message_definition(struct ps_node_t* node, const char* message)
{
	// send da udp packet!
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = node->advertise_addr;
	address.sin_port = htons(node->advertise_port);

	char data[1400];
	data[0] = PS_DISCOVERY_PROTOCOL_QUERY_MSG_DEFINITION;
	int* addr = (int*)&data[1];
	*addr = node->addr;
	unsigned short* port = (unsigned short*)&data[5];
	*port = node->port;

	int off = 7;
	off += serialize_string(&data[off], message);

	//add topic name, node, and 
	int sent_bytes = sendto(node->socket, (const char*)data, off, 0, (struct sockaddr*)&address, sizeof(struct sockaddr_in));
}

// sends out a multicast request looking for publishers on this topic
void ps_node_subscribe_query(struct ps_sub_t* sub)
{
	//printf("Advertising subscriber %s\n", sub->topic);
	// send da udp packet!
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = sub->node->advertise_addr;
	address.sin_port = htons(sub->node->advertise_port);

	char data[1400];
	data[0] = PS_DISCOVERY_PROTOCOL_SUBSCRIBE_QUERY;
	int* addr = (int*)&data[1];
	*addr = sub->node->addr;
	unsigned short* port = (unsigned short*)&data[5];
	*port = sub->node->port;

	int off = 7;
	off += serialize_string(&data[off], sub->topic);
	off += serialize_string(&data[off], sub->type ? sub->type->name : "");
	off += serialize_string(&data[off], sub->node->name);

	//add topic name, node, and 
	int sent_bytes = sendto(sub->node->socket, (const char*)data, off, 0, (struct sockaddr*)&address, sizeof(struct sockaddr_in));
}

// advertizes the publisher via multicast
void ps_node_advertise(struct ps_pub_t* pub)
{
	//printf("Advertising topic\n");
	// send da udp packet!
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = pub->node->advertise_addr;
	address.sin_port = htons(pub->node->advertise_port);

	char data[1400];
	struct ps_advertise_req_t* p = (struct ps_advertise_req_t*)data;
	p->id = PS_DISCOVERY_PROTOCOL_ADVERTISE;
	p->addr = pub->node->addr;
	p->port = pub->node->port;

	int off = sizeof(struct ps_advertise_req_t);
	off += serialize_string(&data[off], pub->topic);
	off += serialize_string(&data[off], pub->message_definition->name);
	off += serialize_string(&data[off], pub->node->name);

	//also add other info...
	int sent_bytes = sendto(pub->node->socket, (const char*)data, off, 0, (struct sockaddr*)&address, sizeof(struct sockaddr_in));
}


void ps_node_create_publisher(struct ps_node_t* node, const char* topic, const struct ps_message_definition_t* type, struct ps_pub_t* pub, bool latched)
{
	node->num_pubs++;
	struct ps_pub_t** old_pubs = node->pubs;
	node->pubs = (struct ps_pub_t**)malloc(sizeof(struct ps_pub_t*)*node->num_pubs);
	for (unsigned int i = 0; i < node->num_pubs - 1; i++)
	{
		node->pubs[i] = old_pubs[i];
	}
	node->pubs[node->num_pubs - 1] = pub;
	free(old_pubs);

	// initialize the pub
	pub->clients = 0;
	pub->num_clients = 0;
	pub->message_definition = type;
	pub->topic = topic;
	pub->node = node;
	pub->latched = latched;
	pub->last_message.data = 0;
	pub->last_message.len = 0;

	ps_node_advertise(pub);
}


static bool initialized = false;
void networking_init()
{
#ifdef _WIN32
	if (!initialized)
	{
		struct WSAData wsaData;
		int retval;
		if ((retval = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0)
		{
			char sz[56];
			sprintf_s(sz, "WSAStartup failed with error %d\n", retval);
			OutputDebugStringA(sz);
			WSACleanup();
		}
		initialized = true;
	}
#endif
}

//setup OS calls

#ifdef _WIN32
static int ps_shutdown = 0;
BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
	switch (fdwCtrlType)
	{
		// Handle the CTRL-C signal. 
	case CTRL_C_EVENT:
		//printf("Ctrl-C event\n\n");
		ps_shutdown = 1;
		return TRUE;
	default:
		return FALSE;
	}
}

int ps_okay()
{
	return ps_shutdown ? 0 : 1;
}

#else

int ps_okay()
{
	return 1;
}

#endif

char* GetPrimaryIp()
{
	//assert(buflen >= 16);

	ps_socket_t sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	//assert(sock != -1);

	struct sockaddr_in serv;
	memset(&serv, 0, sizeof(serv));
	serv.sin_family = AF_INET;
	serv.sin_addr.s_addr = inet_addr("1.1.1.1");
	serv.sin_port = htons(53);

	int err = connect(sock, (const struct sockaddr*)&serv, sizeof(serv));
	//assert(err != -1);

	struct sockaddr_in name;
	socklen_t namelen = sizeof(name);
	err = getsockname(sock, (struct sockaddr*)&name, &namelen);
	//assert(err != -1);
	char* ip = inet_ntoa(name.sin_addr);
	//assert(p);

#ifdef _WIN32
	closesocket(sock);
#else
	close(sock);
#endif
	return ip;
}

void ps_node_init(struct ps_node_t* node, const char* name, const char* ip, bool broadcast)
{
	networking_init();

#ifdef _WIN32
	SetConsoleCtrlHandler(CtrlHandler, TRUE);
#endif

	node->name = name;
	node->num_pubs = 0;
	node->pubs = 0;
	node->num_subs = 0;
	node->subs = 0;

	node->adv_cb = 0;
	node->sub_cb = 0;
	node->def_cb = 0;

	node->_last_advertise = 0;

	node->advertise_port = 11311;// todo make this configurable

	// find an ip (lets just assume the externally facing one)
	if (strlen(ip) == 0)
	{
		ip = GetPrimaryIp();
	}

	unsigned int mc_addr = inet_addr("239.255.255.249");// todo make this configurable
	if (broadcast)
	{
		//convert to a broadcast address (just the subnet wide one)
		node->advertise_addr = inet_addr(ip);
		node->advertise_addr |= 0xFF000000;
	}
	else
	{
		node->advertise_addr = mc_addr;
	}

	node->sub_index = 100;// maybe should randomize this?

	// setup the core socket
	node->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	node->addr = ntohl(inet_addr(ip));
	if (node->socket == 0)
	{
		printf("Failed To Create Socket!\n");
		node->socket = 0;
		return;
	}

	// bind to an ephemeral port
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;// htonl(node->addr);
	address.sin_port = 0;//ephemeral port!

	if (bind(node->socket, (const struct sockaddr*)&address, sizeof(struct sockaddr_in)) < 0)
	{
#ifdef _WIN32
		int err = WSAGetLastError();
		printf("Failed to Bind Socket, %i\n", err);
#endif
#ifdef _WIN32
		closesocket(node->socket);
#else
		close(node->socket);
#endif
		return;
	}

	socklen_t outlen = sizeof(struct sockaddr_in);
	struct sockaddr_in outaddr;
	getsockname(node->socket, (struct sockaddr*)&outaddr, &outlen);
	node->port = ntohs(outaddr.sin_port);

	printf("Bound to %i\n", node->port);

	// set non-blocking i
#ifdef _WIN32
	DWORD nonBlocking = 1;
	if (ioctlsocket(node->socket, FIONBIO, &nonBlocking) != 0)
	{
		printf("Failed to Set Socket as Non-Blocking!\n");
#ifdef _WIN32
		closesocket(node->socket);
#else
		close(node->socket);
#endif
		return;
	}
#endif
#ifdef ARDUINO
    fcntl(node->socket, F_SETFL, O_NONBLOCK);
#endif
#ifdef LINUX
	int flags = fcntl(node->socket, F_GETFL);
	fcntl(node->socket, F_SETFL, flags | O_NONBLOCK);
#endif

	//setup multicast socket
	node->mc_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (node->mc_socket == 0)
	{
		printf("Failed To Create mc Socket!\n");
		node->mc_socket = 0;
		return;
	}

	int opt = 1;
	if (setsockopt(node->mc_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	if (broadcast)
	{
		int opt = 1;
		if (setsockopt(node->mc_socket, SOL_SOCKET, SO_BROADCAST, (char *)&opt, sizeof(opt)) < 0)
		{
			perror("setsockopt");
			exit(EXIT_FAILURE);
		}
	}

	// bind to port
	struct sockaddr_in mc_address;
	mc_address.sin_family = AF_INET;
	mc_address.sin_addr.s_addr = htonl(node->addr);//INADDR_ANY;
	mc_address.sin_port = htons(node->advertise_port);

	if (bind(node->mc_socket, (const struct sockaddr*)&mc_address, sizeof(struct sockaddr_in)) < 0)
	{
#ifdef _WIN32
		int err = WSAGetLastError();
		printf("Failed to Bind mc Socket, %i\n", err);
#endif
#ifdef _WIN32
		closesocket(node->mc_socket);
#else
		close(node->mc_socket);
#endif
		return;
	}

	// set non-blocking i
#ifdef _WIN32
	if (ioctlsocket(node->mc_socket, FIONBIO, &nonBlocking) != 0)
	{
		printf("Failed to Set Socket as Non-Blocking!\n");
		closesocket(node->mc_socket);
		return;
	}
#endif
#ifdef ARDUINO
    fcntl(node->mc_socket, F_SETFL, O_NONBLOCK);
#endif
#ifdef LINUX
	int flags2 = fcntl(socket, F_GETFL);
	fcntl(node->mc_socket, F_SETFL, flags2 | O_NONBLOCK);
#endif

	struct ip_mreq mreq;
	mreq.imr_multiaddr.s_addr = mc_addr;
	mreq.imr_interface.s_addr = htonl(node->addr);// sets the interface to subscribe to multicast on
	if (setsockopt(node->mc_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
		(char*)&mreq, sizeof(mreq)) < 0)
	{
		perror("setsockopt");
		return;
	}
}

void ps_node_destroy(struct ps_node_t* node)
{
	for (unsigned int i = 0; i < node->num_pubs; i++)
	{
		ps_pub_destroy(node->pubs[i]);
	}

	for (unsigned int i = 0; i < node->num_subs; i++)
	{
		ps_sub_destroy(node->subs[i]);
	}

#ifdef _WIN32
	closesocket(node->socket);
	closesocket(node->mc_socket);
#else
	close(node->socket);
	close(node->mc_socket);
#endif

	node->socket = node->mc_socket = 0;
	node->num_pubs = node->num_subs = 0;
}

void* ps_malloc_alloc(unsigned int size, void* _)
{
	return malloc(size);
}

void ps_malloc_free(void* data)
{
	free(data);
}

struct ps_allocator_t ps_default_allocator = { ps_malloc_alloc, ps_malloc_free, 0 };

void ps_node_create_subscriber(struct ps_node_t* node, const char* topic, const struct ps_message_definition_t* type,
	struct ps_sub_t* sub,
	unsigned int queue_size,
	bool want_message_def,
	struct ps_allocator_t* allocator,
	bool ignore_local)
{
	node->num_subs++;
	struct ps_sub_t** old_subs = node->subs;
	node->subs = (struct ps_sub_t**)malloc(sizeof(struct ps_sub_t*)*node->num_subs);
	for (unsigned int i = 0; i < node->num_subs - 1; i++)
	{
		node->subs[i] = old_subs[i];
	}
	node->subs[node->num_subs - 1] = sub;
	free(old_subs);

	if (allocator == 0)
	{
		allocator = &ps_default_allocator;
	}

	sub->node = node;
	sub->topic = topic;
	sub->type = type;
	sub->sub_id = node->sub_index++;
	sub->allocator = allocator;
	sub->ignore_local = ignore_local;

	sub->want_message_definition = type ? want_message_def : true;
	sub->received_message_def.fields = 0;
	sub->received_message_def.hash = 0;
	sub->received_message_def.num_fields = 0;

	// force queue size to be > 0
	if (queue_size <= 0)
	{
		queue_size = 1;
	}

	// allocate queue data
	sub->queue_len = 0;
	sub->queue_size = queue_size;
	sub->queue = (void**)malloc(sizeof(void*)*queue_size);

	for (unsigned int i = 0; i < queue_size; i++)
	{
		sub->queue[i] = 0;
	}

	// send out the subscription query while we are at it
	ps_node_subscribe_query(sub);
}

void ps_node_create_subscriber_cb(struct ps_node_t* node, const char* topic, const struct ps_message_definition_t* type,
	struct ps_sub_t* sub,
	ps_subscriber_fn_cb_t cb,
	void* cb_data,
	bool want_message_def,
	struct ps_allocator_t* allocator,
	bool ignore_local
)
{
	node->num_subs++;
	struct ps_sub_t** old_subs = node->subs;
	node->subs = (struct ps_sub_t**)malloc(sizeof(struct ps_sub_t*)*node->num_subs);
	for (unsigned int i = 0; i < node->num_subs - 1; i++)
	{
		node->subs[i] = old_subs[i];
	}
	node->subs[node->num_subs - 1] = sub;
	free(old_subs);

	if (allocator == 0)
	{
		allocator = &ps_default_allocator;
	}

	sub->node = node;
	sub->topic = topic;
	sub->type = type;
	sub->sub_id = node->sub_index++;
	sub->allocator = allocator;
	sub->ignore_local = ignore_local;

	sub->want_message_definition = type ? want_message_def : true;
	sub->received_message_def.fields = 0;
	sub->received_message_def.hash = 0;
	sub->received_message_def.num_fields = 0;

	sub->cb = cb;
	sub->cb_data = cb_data;

	// we dont need a queue
	sub->queue_len = 0;
	sub->queue_size = 0;
	sub->queue = 0;

	// send out the subscription query while we are at it
	ps_node_subscribe_query(sub);
}

void ps_pub_publish_accept(struct ps_pub_t* pub, struct ps_client_t* client, const struct ps_message_definition_t* msg)
{
	//printf("Sending subscribe accept\n");
	// send da udp packet!
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(client->endpoint.address);
	address.sin_port = htons(client->endpoint.port);

	char data[1500];
	struct ps_subscribe_accept_t* p = (struct ps_subscribe_accept_t*)data;
	p->pid = 4;
	p->sid = client->stream_id;

	//todo only send this to clients who request it
	int off = sizeof(struct ps_subscribe_accept_t);
	off += ps_serialize_message_definition(&data[off], msg);

	//also add other info...
	int sent_bytes = sendto(pub->node->socket, (const char*)data, off, 0, (struct sockaddr*)&address, sizeof(struct sockaddr_in));
}

#ifndef _WIN32
#ifndef ARDUINO
#include <time.h>
/// Returns the number of ticks since an undefined time (usually system startup).
static uint64_t GetTickCount64()
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	return (uint64_t)(ts.tv_nsec / 1000000) + ((uint64_t)ts.tv_sec * 1000ull);
}
#endif
#endif

//returns nonzero if we got a message
int ps_node_spin(struct ps_node_t* node)
{
	// look for any subs/requests and process them
	int size = 1500;
	char data[1500];
#ifdef _WIN32
	typedef int socklen_t;
#endif

	//ok, now we need to add timeouts for when nodes disappear and dont make any keepalives on their subs
	//	for now we can use sub queries as keepalives

	//need to also send out our occasional advertisements
	//also send out keepalives which are still todo
#ifndef ARDUINO
	unsigned long long now = GetTickCount64();
	if (node->_last_advertise + 25 * 1000 < GetTickCount64())
	{
		node->_last_advertise = GetTickCount64();
#else
	if (node->_last_advertise + 5 * 1000 < millis())
	{
		node->_last_advertise = millis();
#endif
        //printf("Advertising\n");

		// send out an advertisement for each publisher we have
		for (unsigned int i = 0; i < node->num_pubs; i++)
		{
			ps_node_advertise(node->pubs[i]);
		}

		// also send out a keepalive/request for each subscription we have
		for (unsigned int i = 0; i < node->num_subs; i++)
		{
			ps_node_subscribe_query(node->subs[i]);// for now lets use this as the keep alive too
		}
	}

	int packet_count = 0;
	while (true)
	{
		struct sockaddr_in from;
		socklen_t fromLength = sizeof(from);

		int received_bytes = recvfrom(node->socket, (char*)data, size, 0, (struct sockaddr*)&from, &fromLength);

		if (received_bytes <= 0)
			break;

		unsigned int address = ntohl(from.sin_addr.s_addr);
		unsigned int port = ntohs(from.sin_port);

		// look at the header for the id to see if we should do something with it
		// then keep doing this until we run out of packets or hit a max

		//we got message data or a specific request
		if (data[0] == PS_UDP_PROTOCOL_DATA)// actual message
		{
			struct ps_msg_header* hdr = (struct ps_msg_header*)data;

			//printf("Got message packet seq %i\n", hdr->seq);

			// find the sub
			// todo, is this the fastest method?
			// works great for small subscriber counts
			struct ps_sub_t* sub = 0;
			for (unsigned int i = 0; i < node->num_subs; i++)
			{
				if (node->subs[i]->sub_id == hdr->id)
				{
					sub = node->subs[i];
					break;
				}
			}
			if (sub == 0)
			{
				printf("ERROR: Could not find sub matching stream id %i\n", hdr->id);
				continue;
			}

			// queue up the data, and copy :/ (can make zero copy for arduino version)
			int data_size = received_bytes - sizeof(struct ps_msg_header);

			// todo also allow queueless operation and just call callbacks here


			// also todo fastpath for PoD message types
			
			// okay, if we have the message definition, deserialize and output in a message
			void* out_data;
			if (sub->type)
			{
				out_data = sub->type->decode(data + sizeof(struct ps_msg_header), sub->allocator);
			}
			else
			{
				out_data = sub->allocator->alloc(data_size, sub->allocator->context);
				memcpy(out_data, data + sizeof(struct ps_msg_header), data_size);
			}

			// maybe todo, this doesnt do fifo very correctly
			// only does first newest than two old ones
			//add it to the fifo packet queue which always shows the n most recent packets
			//most recent is always first
			//so lets push to the front (highest open index or highest if full)
			if (sub->queue_size == 0)
			{
				sub->cb(out_data, data_size, sub->cb_data);
			}
			else if (sub->queue_size == sub->queue_len)
			{
				sub->queue[sub->queue_size - 1] = out_data;
			}
			else
			{
				sub->queue[sub->queue_len++] = out_data;
			}
			//printf("Got message, queue len %i\n", sub->queue_len);

			packet_count++;
		}
		else if (data[0] == PS_UDP_PROTOCOL_KEEP_ALIVE)
		{
			// keep alives, actually maybe lets not use this at all?
			unsigned long stream_id = *(unsigned long*)&data[1];
			//find the client and update the keep alive

			for (unsigned int i = 0; i < node->num_pubs; i++)
			{
				for (unsigned int c = 0; c < node->pubs[i]->num_clients; i++)
				{
					if (node->pubs[i]->clients[c].stream_id == stream_id && node->pubs[i]->clients[c].endpoint.port == port)
					{
						node->pubs[i]->clients[c].last_keepalive = GetTickCount64();
						break;
					}
				}
			}
		}
		else if (data[0] == PS_UDP_PROTOCOL_SUBSCRIBE_ACCEPT)
		{
			struct ps_subscribe_accept_t* p = (struct ps_subscribe_accept_t*)data;
			printf("Got subscribe accept for stream %i\n", p->sid);

			// find the sub
			struct ps_sub_t* sub = 0;
			for (unsigned int i = 0; i < node->num_subs; i++)
			{
				if (node->subs[i]->sub_id == p->sid)
				{
					sub = node->subs[i];
					break;
				}
			}
			if (sub == 0)
			{
				printf("Could not find sub matching stream id %i\n", p->sid);
				continue;
			}

			if (sub->want_message_definition)
			{
				ps_deserialize_message_definition(&data[sizeof(struct ps_subscribe_accept_t)], &sub->received_message_def);
			}
		}
		else if (data[0] == PS_UDP_PROTOCOL_SUBSCRIBE_REQUEST)
		{
			//received subscribe request
			struct ps_sub_req_header_t* p = (struct ps_sub_req_header_t*)data;

			char* topic = (char*)&data[sizeof(struct ps_sub_req_header_t)];

			printf("Got subscribe request for %s\n", topic);

			//check if we have a sub matching that topic
			struct ps_pub_t* pub = 0;
			for (unsigned int i = 0; i < node->num_pubs; i++)
			{
				if (strcmp(node->pubs[i]->topic, topic) == 0)
				{
					pub = node->pubs[i];
					break;
				}
			}
			if (pub == 0)
			{
				printf("Got subscribe request, but it was for a topic we don't have\n");
				continue;
			}

			// now check that the type matches or none is given...
			char* type = (char*)&data[strlen(topic) + sizeof(struct ps_sub_req_header_t) + 1];
			if (strlen(type) != 0 && strcmp(type, pub->message_definition->name) != 0)
			{
				printf("The types didnt match! Ignoring\n");
				continue;
			}

			// send response and start publishing
			struct ps_client_t client;
			client.endpoint.address = p->addr;
			client.endpoint.port = p->port;
			client.last_keepalive = GetTickCount64();// use the current time stamp
			client.sequence_number = 0;
			client.stream_id = p->sub_id;

			printf("Got subscribe request, adding client if we haven't already\n");
			ps_pub_add_client(pub, &client);

			//send out the data format to the client
			ps_pub_publish_accept(pub, &client, pub->message_definition);

			//todo if it is a latched topic, need to publish our last value
		}
		else if (data[0] == PS_UDP_PROTOCOL_MESSAGE_DEFINITION/*8*/)
		{
			//printf("got message definition");
			if (node->def_cb)
			{
				char* type = (char*)&data[1];

				// todo fix memory leak here
				struct ps_message_definition_t def;
				def.name = malloc(strlen(type) + 1);
				strcpy(def.name, type);
				ps_deserialize_message_definition(&data[1 + strlen(type) + 1], &def);
				node->def_cb(&def);
			}
		}
	}

	// now receive multicast data and process it
	// todo abstract me out from the normal loop, same with discovery updates
	while (true)
	{
		struct sockaddr_in from;
		socklen_t fromLength = sizeof(from);

		int received_bytes = recvfrom(node->mc_socket, (char*)data, size, 0, (struct sockaddr*)&from, &fromLength);

		if (received_bytes <= 0)
			break;

		unsigned int address = ntohl(from.sin_addr.s_addr);
		unsigned int port = ntohs(from.sin_port);

		// look at the header for the id to see if we should do something with it
		// then keep doing this until we run out of packets or hit a max

		//todo add enums for these
		if (data[0] == PS_DISCOVERY_PROTOCOL_SUBSCRIBE_QUERY)
		{
			//subscribe query, from a node subscribing to a topic
			int* addr = (int*)&data[1];
			unsigned short* port = (unsigned short*)&data[5];

			char* topic = (char*)&data[7];

			if (node->sub_cb)
			{
				char* type = (char*)&data[strlen(topic) + 8];
				char* node_name = type + 1 + strlen(type);
				node->sub_cb(topic, type, node_name, 0);
			}

			//check if we have a sub matching that topic
			struct ps_pub_t* pub = 0;
			for (unsigned int i = 0; i < node->num_pubs; i++)
			{
				if (strcmp(node->pubs[i]->topic, topic) == 0)
				{
					pub = node->pubs[i];
					break;
				}
			}
			if (pub == 0)
			{
				//printf("Got subscribe query, but it was for a topic we don't have\n");
				continue;
			}

			// now check that the type matches...
			char* type = (char*)&data[strlen(topic) + 8];
			if (strlen(type) != 0 && strcmp(type, pub->message_definition->name) != 0)
			{
				printf("The types didnt match! Ignoring\n");
				continue;
			}

			char* node_name = type + 1 + strlen(type);

			//if we already have a sub for this, ignore
			for (unsigned int i = 0; i < pub->num_clients; i++)
			{
				if (pub->clients[i].endpoint.port == *port && pub->clients[i].endpoint.address == *addr)
				{
					//printf("Got subscribe query from a current client, not advertising\n");
					pub->clients[i].last_keepalive = GetTickCount64();
					break;
				}
				else if (i == pub->num_clients - 1)
				{
					printf("Got new subscribe query, advertising\n");
					ps_node_advertise(pub);
				}
			}

			if (pub->num_clients == 0)
			{
				printf("Got new subscribe query, advertising\n");
				ps_node_advertise(pub);
			}
		}
		else if (data[0] == PS_DISCOVERY_PROTOCOL_ADVERTISE)
		{
			struct ps_advertise_req_t* p = (struct ps_advertise_req_t*)data;

			char* topic = (char*)&data[sizeof(struct ps_advertise_req_t)];

			if (node->adv_cb)
			{
				char* type = (char*)&data[strlen(topic) + sizeof(struct ps_advertise_req_t) + 1];
				char* node_name = type + 1 + strlen(type);
				node->adv_cb(topic, type, node_name, 0);
			}

			//printf("Got advertise notice\n");
			// is it something im subscribing to? if so send an actual subscribe request to that specific port
			//check if we have a sub matching that topic
			struct ps_sub_t* sub = 0;
			for (unsigned int i = 0; i < node->num_subs; i++)
			{
				if (strcmp(node->subs[i]->topic, topic) == 0)
				{
					sub = node->subs[i];
					break;
				}
			}
			if (sub == 0)
			{
				//printf("Got advertise notice, but it was for a topic we don't have\n");
				continue;
			}

			if (sub->ignore_local && sub->node->port == p->port && sub->node->addr == p->addr)
			{
				printf("Got advertise notice, but it was for a local pub which we are set to ignore\n");
				continue;
			}

			// now check that the type matches or we are dynamic...
			char* type = (char*)&data[strlen(topic) + sizeof(struct ps_advertise_req_t) + 1];
			if (sub->type != 0 && strcmp(type, sub->type->name) != 0)
			{
				printf("The types didnt match! Ignoring\n");
				continue;
			}

			// check if we are already getting data from this person, if so lets not send another request to their advertise

			//printf("Got advertise notice for a topic we need\n");

			struct ps_endpoint_t ep;
			ep.address = p->addr;
			ep.port = p->port;
			ps_send_subscribe(sub, &ep);
		}
		else if (data[0] == PS_DISCOVERY_PROTOCOL_UNSUBSCRIBE)
		{
			printf("Got unsubscribe request\n");

			int* addr = (int*)&data[1];
			unsigned short* port = (unsigned short*)&data[5];

			char* topic = (char*)&data[7];

			//check if we have a sub matching that topic
			struct ps_pub_t* pub = 0;
			for (unsigned int i = 0; i < node->num_pubs; i++)
			{
				if (strcmp(node->pubs[i]->topic, topic) == 0)
				{
					pub = node->pubs[i];
					break;
				}
			}
			if (pub == 0)
			{
				printf("Got unsubscribe request, but it was for a topic we don't have\n");
				continue;
			}

			// remove the client
			struct ps_client_t client;
			client.endpoint.address = *addr;
			client.endpoint.port = *port;
			ps_pub_remove_client(pub, &client);
		}
		else if (data[0] == PS_DISCOVERY_PROTOCOL_QUERY_ALL)
		{
			printf("Got query request\n");
			//overall query, maybe put this into fewer packets?
			// also todo maybe make this use unicast instead

			// send out an advertisement for each publisher we have
			for (unsigned int i = 0; i < node->num_pubs; i++)
			{
				ps_node_advertise(node->pubs[i]);
			}

			// also send out a keepalive for each subscription we have
			for (unsigned int i = 0; i < node->num_subs; i++)
			{
				ps_node_subscribe_query(node->subs[i]);// for now lets use this as the keep alive too
			}
		}
		else if (data[0] == PS_DISCOVERY_PROTOCOL_QUERY_MSG_DEFINITION)
		{
			// data format request
			int* addr = (int*)&data[1];
			unsigned short* port = (unsigned short*)&data[5];

			char* type = (char*)&data[7];
			//printf("Got query message definition for %s", type);

			// check if we have that message type
			const struct ps_message_definition_t* def = 0;
			for (unsigned int i = 0; i < node->num_pubs; i++)
			{
				if (def == 0 && strcmp(node->pubs[i]->message_definition->name, type) == 0)
				{
					def = node->pubs[i]->message_definition;
					break;
				}
			}

			for (unsigned int i = 0; i < node->num_subs; i++)
			{
				if (def == 0 && node->subs[i]->type && strcmp(node->subs[i]->type->name, type) == 0)
				{
					def = node->subs[i]->type;
					break;
				}
			}

			if (!def)
			{
				break;
			}

			// send da udp packet!
			struct sockaddr_in address;
			address.sin_family = AF_INET;
			address.sin_addr.s_addr = htonl(*addr);
			address.sin_port = htons(*port);

			char data[1500];
			data[0] = PS_UDP_PROTOCOL_MESSAGE_DEFINITION;

			int off = 1;
			off += serialize_string(&data[off], def->name);
			off += ps_serialize_message_definition(&data[off], def);

			//also add other info...
			int sent_bytes = sendto(node->socket, (const char*)data, off, 0, (struct sockaddr*)&address, sizeof(struct sockaddr_in));
		}
	}

	// perform time out checks on publishers
	for (unsigned int i = 0; i < node->num_pubs; i++)
	{
		for (unsigned int c = 0; c < node->pubs[i]->num_clients; c++)
		{
			struct ps_client_t* client = &node->pubs[i]->clients[c];
			if (client->last_keepalive < now - 120 * 1000)
			{
				printf("Client has timed out, unsubscribing...");
				// remove the client
				struct ps_client_t cl = *client;
				ps_pub_remove_client(node->pubs[i], &cl);
			}
		}
	}
	return packet_count;
}

int serialize_string(char* data, const char* str)
{
	// todo do this smarter
	strcpy(data, str);

	return strlen(str) + 1;
}