#include <pubsub/Node.h>
#include <pubsub/Publisher.h>
#include <pubsub/Subscriber.h>
#include <pubsub/Serialization.h>
#include <pubsub/System.h>
#include <pubsub/UDPTransport.h>

#include <stdio.h>

#include <pubsub/Net.h>

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
	p->type_hash = pub->message_definition->hash;
	p->transports = pub->node->supported_transports;
	p->group_id = pub->node->group_id;

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
	node->pubs = (struct ps_pub_t**)malloc(sizeof(struct ps_pub_t*) * node->num_pubs);
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
	pub->sequence_number = 0;

	ps_node_advertise(pub);
}

// Setup Control-C handlers
#ifdef _WIN32
static int ps_shutdown = 0;
BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
	switch (fdwCtrlType)
	{
	case CTRL_C_EVENT:
		ps_shutdown = 1;

		// Return true to cancel the event propagating further
		return TRUE;
	default:
		// Treat all other events normally
		return FALSE;
	}
}
#else
#include <signal.h>
volatile static int ps_shutdown = 0;
void CtrlHandler(int sig)
{
	ps_shutdown = 1;
}
#endif

int ps_okay()
{
	return ps_shutdown ? 0 : 1;
}

// Tries to find a good IP to bind to for discovery by looking for one which has a route out
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

	if (name.sin_addr.s_addr == 0)
	{
		ip = "127.0.0.1";
	}

	printf("IP: %s\n", ip);

#ifdef _WIN32
	closesocket(sock);
#else
	close(sock);
#endif
	return ip;
}

void ps_node_init(struct ps_node_t* node, const char* name, const char* ip, bool broadcast)
{
	ps_node_init_ex(node, name, ip, broadcast, true);
}

void ps_node_init_ex(struct ps_node_t* node, const char* name, const char* ip, bool broadcast,
	bool setup_ctrl_c_handler)
{
	// Initialize any platform specific networking
	ps_networking_init();

	// Setup a control-c handler to allow us nice shutdown
	if (setup_ctrl_c_handler)
	{
#ifdef _WIN32
		SetConsoleCtrlHandler(CtrlHandler, TRUE);
#else
		signal(SIGINT, CtrlHandler);
#endif
	}

	node->name = name;
	node->num_pubs = 0;
	node->pubs = 0;
	node->num_subs = 0;
	node->subs = 0;

	node->adv_cb = 0;
	node->sub_cb = 0;
	node->def_cb = 0;
	node->param_cb = 0;
	node->param_confirm_cb = 0;

	node->supported_transports = PS_TRANSPORT_UDP;
	node->num_transports = 0;
	node->transports = 0;

	node->_last_advertise = 0;
	node->_last_check = 0;

	node->advertise_port = 11311;// todo make this configurable

	// If no IP is given, try and find one
	if (ip == 0 || strlen(ip) == 0)
	{
		ip = GetPrimaryIp();
	}

#ifdef _WIN32
	node->group_id = GetCurrentProcessId() + (10000 * ((inet_addr(ip) >> 24) && 0xFF));
#else
	node->group_id = 0;// ignore the group
#endif

	unsigned int mc_bind_addr = INADDR_ANY;
	unsigned int mc_addr = inet_addr("239.255.255.249");// todo make this configurable
	if (strcmp(ip, "127.0.0.1") == 0)
	{
		// hack for localhost
		broadcast = true;
		node->advertise_addr = inet_addr("127.255.255.255");
		mc_bind_addr = mc_bind_addr;// this seems like a typo....
	}
	else if (broadcast)
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

	// Setup the core socket
	node->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	node->addr = ntohl(inet_addr(ip));
	if (node->socket == 0)
	{
		printf("Failed To Create Socket!\n");
		node->socket = 0;
		return;
	}

	// Bind the core socket to an ephemeral port
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;// htonl(node->addr);
	address.sin_port = 0;// zero indicates ephemeral port!

	if (bind(node->socket, (const struct sockaddr*)&address, sizeof(struct sockaddr_in)) < 0)
	{
		ps_print_socket_error("failed to bind socket");
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

	//printf("Bound to %i\n", node->port);

	// set non-blocking i
#ifdef _WIN32
	DWORD nonBlocking = 1;
	if (ioctlsocket(node->socket, FIONBIO, &nonBlocking) != 0)
	{
		ps_print_socket_error("Failed to Set Socket as Non-Blocking");
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
#ifdef __unix__
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
	if (setsockopt(node->mc_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) < 0)
	{
		ps_print_socket_error("setsockopt reuse");
		exit(EXIT_FAILURE);
	}

	// Enable broadcast on all sockets just in case
	if (setsockopt(node->mc_socket, SOL_SOCKET, SO_BROADCAST, (char*)&opt, sizeof(opt)) < 0)
	{
		ps_print_socket_error("setsockopt broadcast mc");
		exit(EXIT_FAILURE);
	}
	if (setsockopt(node->socket, SOL_SOCKET, SO_BROADCAST, (char*)&opt, sizeof(opt)) < 0)
	{
		ps_print_socket_error("setsockopt broadcast");
		exit(EXIT_FAILURE);
	}

	// bind to port
	struct sockaddr_in mc_address;
	mc_address.sin_family = AF_INET;
	mc_address.sin_addr.s_addr = htonl(mc_bind_addr);// Linux seems to need to be bound to INADDR_ANY for broadcast or multicast
	mc_address.sin_port = htons(node->advertise_port);

	if (bind(node->mc_socket, (const struct sockaddr*)&mc_address, sizeof(struct sockaddr_in)) < 0)
	{
		ps_print_socket_error("Failed to bind mc socket");
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
		ps_print_socket_error("Failed to Set Socket as Non-Blocking");
		closesocket(node->mc_socket);
		return;
	}
#endif
#ifdef ARDUINO
	fcntl(node->mc_socket, F_SETFL, O_NONBLOCK);
#endif
#ifdef __unix__
	int flags2 = fcntl(node->mc_socket, F_GETFL);
	fcntl(node->mc_socket, F_SETFL, flags2 | O_NONBLOCK);
#endif

	// add sockets to events
#ifndef PUBSUB_REAL_TIME
	ps_event_set_create(&node->events);
	ps_event_set_add_socket(&node->events, node->socket);
	ps_event_set_add_socket(&node->events, node->mc_socket);
#endif

	if (broadcast)
	{
		return;
	}

	struct ip_mreq mreq;
	mreq.imr_multiaddr.s_addr = mc_addr;
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);// Linux seems to require INADDR_ANY here
	if (setsockopt(node->mc_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
		(char*)&mreq, sizeof(mreq)) < 0)
	{
		ps_print_socket_error("setsockopt multicast add membership");
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

	for (unsigned int i = 0; i < node->supported_transports; i++)
	{
		node->transports[i].destroy(&node->transports[i]);
	}

#ifndef PUBSUB_REAL_TIME
	ps_event_set_destroy(&node->events);
#endif

#ifdef _WIN32
	closesocket(node->socket);
	closesocket(node->mc_socket);
#else
	close(node->socket);
	close(node->mc_socket);
#endif

	if (node->transports)
	{
		free(node->transports);
	}

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

void ps_subscriber_options_init(struct ps_subscriber_options* options)
{
	options->queue_size = 1;
	options->ignore_local = true;
	options->allocator = 0;
	options->skip = 0;
	options->want_message_def = false;
	options->cb = 0;
	options->cb_data = 0;
	options->preferred_transport = PS_TRANSPORT_UDP;
}

void ps_node_create_subscriber_adv(struct ps_node_t* node, const char* topic, const struct ps_message_definition_t* type,
	struct ps_sub_t* sub,
	const struct ps_subscriber_options* options)
{
	node->num_subs++;
	struct ps_sub_t** old_subs = node->subs;
	node->subs = (struct ps_sub_t**)malloc(sizeof(struct ps_sub_t*) * node->num_subs);
	for (unsigned int i = 0; i < node->num_subs - 1; i++)
	{
		node->subs[i] = old_subs[i];
	}
	node->subs[node->num_subs - 1] = sub;
	free(old_subs);

	struct ps_allocator_t* allocator = options->allocator;
	if (allocator == 0)
	{
		allocator = &ps_default_allocator;
	}

	sub->node = node;
	sub->topic = topic;
	sub->type = type;
	sub->sub_id = node->sub_index++;
	sub->allocator = allocator;
	sub->ignore_local = options->ignore_local;
	sub->skip = options->skip;
	sub->preferred_transport = options->preferred_transport;

	sub->want_message_definition = type ? options->want_message_def : true;
	sub->received_message_def.fields = 0;
	sub->received_message_def.hash = 0;
	sub->received_message_def.num_fields = 0;

	// force queue size to be > 0
	unsigned int queue_size = options->queue_size;
	if (options->cb)
	{
		sub->cb = options->cb;
		sub->cb_data = options->cb_data;
		sub->queue_size = 0;
		sub->queue_len = 0;
		sub->queue_start = 0;
		sub->queue = 0;
	}
	else
	{
		if (queue_size <= 0)
		{
			queue_size = 1;
		}

		// allocate queue data
		sub->queue_len = 0;
		sub->queue_start = 0;
		sub->queue_size = queue_size;
		sub->queue = (void**)malloc(sizeof(void*) * queue_size);

		for (unsigned int i = 0; i < queue_size; i++)
		{
			sub->queue[i] = 0;
		}
	}

	// send out the subscription query while we are at it
	ps_node_subscribe_query(sub);
}

void ps_node_create_subscriber(struct ps_node_t* node, const char* topic, const struct ps_message_definition_t* type,
	struct ps_sub_t* sub,
	unsigned int queue_size,
	bool want_message_def,
	struct ps_allocator_t* allocator,
	bool ignore_local)
{
	struct ps_subscriber_options options;
	ps_subscriber_options_init(&options);

	options.queue_size = queue_size;
	options.want_message_def = want_message_def;
	options.allocator = allocator;
	options.ignore_local = ignore_local;

	ps_node_create_subscriber_adv(node, topic, type, sub, &options);
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
	struct ps_subscriber_options options;
	ps_subscriber_options_init(&options);

	options.queue_size = 0;
	options.cb = cb;
	options.cb_data = cb_data;
	options.want_message_def = want_message_def;
	options.allocator = allocator;
	options.ignore_local = ignore_local;

	ps_node_create_subscriber_adv(node, topic, type, sub, &options);
}

void ps_udp_publish_accept(struct ps_pub_t* pub, struct ps_client_t* client, const struct ps_message_definition_t* msg)
{
	//printf("Sending subscribe accept\n");
	// send da udp packet!
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(client->endpoint.address);
	address.sin_port = htons(client->endpoint.port);

	char data[1500];
	struct ps_subscribe_accept_t* p = (struct ps_subscribe_accept_t*)data;
	p->pid = PS_UDP_PROTOCOL_SUBSCRIBE_ACCEPT;
	p->sid = client->stream_id;

	//todo only send this to clients who request it
	int off = sizeof(struct ps_subscribe_accept_t);
	off += ps_serialize_message_definition(&data[off], msg);

	//also add other info...
	int sent_bytes = sendto(pub->node->socket, (const char*)data, off, 0, (struct sockaddr*)&address, sizeof(struct sockaddr_in));
}

#ifndef PUBSUB_REAL_TIME
#include <pubsub/Events.h>
int ps_node_wait(struct ps_node_t* node, unsigned int timeout_ms)
{
	ps_event_set_wait(&node->events, timeout_ms);

	return 0;
}

// returns the number added
int ps_node_create_events(struct ps_node_t* node, struct ps_event_set_t* events)
{
	ps_event_set_add_socket(events, node->socket);
	ps_event_set_add_socket(events, node->mc_socket);

	return 2;
}
#endif

void ps_node_add_transport(struct ps_node_t* node, struct ps_transport_t* transport)
{
	node->num_transports++;

	struct ps_transport_t* old_transports = node->transports;

	node->transports = (struct ps_transport_t*)malloc(sizeof(struct ps_transport_t) * node->num_transports);
	for (int i = 0; i < node->num_transports - 1; i++)
	{
		node->transports[i] = old_transports[i];
	}
	node->transports[node->num_transports - 1] = *transport;

	if (old_transports)
	{
		free(old_transports);
	}

	node->supported_transports |= transport->uuid;
}

// Update the node and checks if we got new messages
// Returns the number of UDP messages we got
int ps_node_spin(struct ps_node_t* node)
{
	// look for any subs/requests and process them
	int size = 1500;
	char data[1500];
#ifdef _WIN32
	typedef int socklen_t;
#endif

	//need to also send out our occasional advertisements
	//also send out keepalives which are still todo
#ifndef ARDUINO
	unsigned long long now = ps_get_tick_count();
#else
	unsigned long now = ps_get_tick_count();// todo have a typedef for this
#endif
	if (node->_last_advertise + 10 * 1000 < now)
	{
		node->_last_advertise = now;
#ifdef PUBSUB_VERBOSE
		//printf("Advertising\n");
#endif

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

	// Update any other transports
	for (unsigned int i = 0; i < node->num_transports; i++)
	{
		node->transports[i].spin(&node->transports[i], node);
	}

	// Update the main UDP protocol and discovery
	int packet_count = 0;
	while (true)
	{
		struct sockaddr_in from;
		socklen_t fromLength = sizeof(from);

		int received_bytes = recvfrom(node->socket, (char*)data, size, 0, (struct sockaddr*)&from, &fromLength);

		if (received_bytes <= 0)
			break;

#ifdef PUBSUB_VERBOSE
		//printf("got transport packet\n");
#endif

		struct ps_msg_info_t message_info;
		message_info.address = ntohl(from.sin_addr.s_addr);
		message_info.port = ntohs(from.sin_port);

		// look at the header for the id to see if we should do something with it
		// then keep doing this until we run out of packets or hit a max

		//we got message data or a specific request
		if (data[0] == PS_UDP_PROTOCOL_DATA)// actual message
		{
			const struct ps_msg_header* hdr = (struct ps_msg_header*)data;

#ifdef PUBSUB_VERBOSE
			//printf("Got message packet seq %i\n", hdr->seq);
#endif

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

			ps_sub_enqueue(sub, out_data, data_size, &message_info);

#ifdef PUBSUB_VERBOSE
			//printf("Got message, queue len %i\n", sub->queue_len);
#endif

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
					if (node->pubs[i]->clients[c].stream_id == stream_id && node->pubs[i]->clients[c].endpoint.port == message_info.port)
					{
						node->pubs[i]->clients[c].last_keepalive = ps_get_tick_count();
						break;
					}
				}
			}
		}
		else if (data[0] == PS_UDP_PROTOCOL_SUBSCRIBE_ACCEPT)
		{
			struct ps_subscribe_accept_t* p = (struct ps_subscribe_accept_t*)data;
#ifdef PUBSUB_VERBOSE
			printf("Got subscribe accept for stream %i\n", p->sid);
#endif

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
				printf("Error: Could not find sub matching stream id %i\n", p->sid);
				continue;
			}

			if (sub->want_message_definition)
			{
				ps_deserialize_message_definition(&data[sizeof(struct ps_subscribe_accept_t)], &sub->received_message_def);

				// call the callback as well
				if (node->def_cb)
				{
					node->def_cb(&sub->received_message_def);
				}
			}
		}
		else if (data[0] == PS_UDP_PROTOCOL_SUBSCRIBE_REQUEST)
		{
			//received subscribe request
			struct ps_sub_req_header_t* p = (struct ps_sub_req_header_t*)data;

			char* topic = (char*)&data[sizeof(struct ps_sub_req_header_t)];

#ifdef PUBSUB_VERBOSE
			//printf("Got subscribe request for %s\n", topic);
#endif

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
#ifdef PUBSUB_VERBOSE
				printf("Got subscribe request, but it was for a topic we don't have '%s'\n", topic);
#endif
				continue;
			}

			// now check that the type matches or none is given...
			char* type = (char*)&data[strlen(topic) + sizeof(struct ps_sub_req_header_t) + 1];
			if (strlen(type) != 0 && strcmp(type, pub->message_definition->name) != 0)
			{
#ifdef PUBSUB_VERBOSE
				// todo maybe make this a warning
				printf("The types didnt match! Ignoring\n");
#endif
				continue;
			}

			// send response and start publishing
			struct ps_client_t client;
			client.endpoint.address = p->addr;
			client.endpoint.port = p->port;
			client.last_keepalive = ps_get_tick_count();// use the current time stamp
			client.sequence_number = 0;
			client.stream_id = p->sub_id;
			client.modulo = p->skip > 0 ? p->skip + 1 : 0;
			client.transport = 0;

#ifdef PUBSUB_VERBOSE
			printf("Got subscribe request, adding client if we haven't already\n");
#endif

			ps_pub_add_client(pub, &client);

			//send out the data format to the client
			ps_udp_publish_accept(pub, &client, pub->message_definition);
		}
		else if (data[0] == PS_UDP_PROTOCOL_PARAM_ACK)
		{
			// We got a parameter change confirmation. What to do with it?
			double value = *(double*)&data[1];
			const char* name = &data[1 + 8];
			//printf("Got ack, new value: %f\n", value);

			if (node->param_confirm_cb)
			{
				node->param_confirm_cb(name, value);
			}
		}
		else if (data[0] == PS_UDP_PROTOCOL_MESSAGE_DEFINITION)
		{
			//printf("got message definition");
			if (node->def_cb)
			{
				char* type = (char*)&data[1];

				struct ps_message_definition_t def;
				ps_deserialize_message_definition(&data[1], &def);
				node->def_cb(&def);
				ps_free_message_definition(&def);
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

		//printf("Got discovery msg \n");

		unsigned int address = ntohl(from.sin_addr.s_addr);
		unsigned int port = ntohs(from.sin_port);

		// look at the header for the id to see if we should do something with it
		// then keep doing this until we run out of packets or hit a max

		//todo add enums for these
		if (data[0] == PS_DISCOVERY_PROTOCOL_SUBSCRIBE_QUERY)
		{
			//printf("Got subscribe msg \n");
			//subscribe query, from a node subscribing to a topic
			struct ps_subscribe_req_t* req = (struct ps_subscribe_req_t*)data;

			char* topic = (char*)&data[7];

			if (node->sub_cb)
			{
				char* type = (char*)&data[strlen(topic) + 8];
				char* node_name = type + 1 + strlen(type);
				node->sub_cb(topic, type, node_name, req);
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
#ifdef PUBSUB_VERBOSE
				printf("The types didnt match! Ignoring\n");
#endif
				continue;
			}

			char* node_name = type + 1 + strlen(type);

			//if we already have a sub for this, ignore
			for (unsigned int i = 0; i < pub->num_clients; i++)
			{
				if (pub->clients[i].endpoint.port == req->port && pub->clients[i].endpoint.address == req->addr)
				{
					//printf("Got subscribe query from a current client, not advertising\n");
					pub->clients[i].last_keepalive = ps_get_tick_count();
					break;
				}
				else if (i == pub->num_clients - 1)
				{
#ifdef PUBSUB_VERBOSE
					//printf("Got new subscribe query, advertising\n");
#endif
					ps_node_advertise(pub);
				}
			}

			if (pub->num_clients == 0)
			{
#ifdef PUBSUB_VERBOSE
				//printf("Got new subscribe query, advertising\n");
#endif
				ps_node_advertise(pub);
			}
		}
		else if (data[0] == PS_DISCOVERY_PROTOCOL_ADVERTISE)
		{
			//printf("Got advertise msg \n");
			struct ps_advertise_req_t* p = (struct ps_advertise_req_t*)data;

			char* topic = (char*)&data[sizeof(struct ps_advertise_req_t)];

			if (node->adv_cb)
			{
				char* type = (char*)&data[strlen(topic) + sizeof(struct ps_advertise_req_t) + 1];
				char* node_name = type + 1 + strlen(type);
				node->adv_cb(topic, type, node_name, p);
			}

			//printf("Got advertise notice\n");
			// is it something im subscribing to? if so send an actual subscribe request to that specific port
			//check if we have a sub matching that topic
			struct ps_sub_t* sub = 0;
			for (unsigned int i = 0; i < node->num_subs; i++)
			{
				//printf("%s\n", node->subs[i]->topic);
				if (strcmp(node->subs[i]->topic, topic) != 0)
				{
					sub = node->subs[i];
					continue;
				}

				sub = node->subs[i];

				if (sub == 0)
				{
					//printf("Got advertise notice, but it was for a topic we don't have %s\n", topic);
					continue;
				}

				if (sub->ignore_local && sub->node->port == p->port && sub->node->addr == p->addr)
				{
					printf("Got advertise notice, but it was for a local pub which we are set to ignore\n");
					continue;
				}

				// check group id
				if (sub->ignore_local && p->group_id != 0 && p->group_id == node->group_id)
				{
					printf("We are a member of this group, ignoring advertise");
					continue;
				}

				// now check that the type matches or we are dynamic...
				char* type = (char*)&data[strlen(topic) + sizeof(struct ps_advertise_req_t) + 1];
				if (sub->type != 0 && strcmp(type, sub->type->name) != 0)
				{
					printf("The types didnt match! Ignoring\n");
					continue;
				}

				//check hashes
				if (sub->type != 0 && p->type_hash != sub->type->hash)
				{
					// its an error for type mistmatch
					printf("ERROR: Type hash mismatch on %s", type);
					continue;
				}

				// todo subscribe to the most relevant protocol
				// check if we are already getting data from this person, if so lets not send another request to their advertise

				//printf("Got advertise notice for a topic we need\n");
				// pick which protocol to subscribe with
				struct ps_endpoint_t ep;
				ep.address = p->addr;
				ep.port = p->port;

				// first match udp if its what we want or all that is offered
				if (sub->preferred_transport == PS_TRANSPORT_UDP || p->transports == PS_TRANSPORT_UDP)
				{
					ps_udp_subscribe(sub, &ep);
				}
				else if (node->num_transports == 0)
				{
					printf("ERROR: Transport mismatch. Do not have desired transport.\n");
				}
				else
				{
					// Try and find a preferred transport
					bool found = false;
					for (int i = 0; i < node->num_transports; i++)
					{
						struct ps_transport_t* transport = &node->transports[i];
						if (transport->uuid & sub->preferred_transport != 0)
						{
							//printf("Found matching preferred transport\n");
							transport->subscribe(transport, sub, &ep);
							found = true;
							break;
						}
					}
					if (!found)
					{				
						// Otherwise fallback to udp
						ps_udp_subscribe(sub, &ep);
					}
				}
			}
		}
		else if (data[0] == PS_UDP_PROTOCOL_PARAM_CHANGE)
		{
			// We got a request to change a parameters
			// if we have a callback, call it
			double value = *(double*)&data[1];
			const char* name = &data[1 + 8];

			//printf("Got param change %f for %s\n", value, name);
			int send_ack = 0;
			double new_value = 0.0;
			if (node->param_cb)
			{
				new_value = node->param_cb(name, value);
				send_ack = (new_value == new_value);// nan check
			}

			if (send_ack)
			{
				printf("Change was confirmed. Replying with new value %f to port %i.\n", new_value, (int)port);
				// send a confirmation message, which is just the same as the one we sent
				char newdata[1500];
				memcpy(newdata, data, received_bytes);
				newdata[0] = PS_UDP_PROTOCOL_PARAM_ACK;
				*(double*)&newdata[1] = new_value;
				int sent_bytes = sendto(node->socket, (const char*)newdata, received_bytes, 0, (struct sockaddr*)&from, sizeof(struct sockaddr_in));
			}
		}
		else if (data[0] == PS_DISCOVERY_PROTOCOL_UNSUBSCRIBE)
		{
			//printf("Got unsubscribe request\n");

			int* addr = (int*)&data[1];
			unsigned short* port = (unsigned short*)&data[5];

			unsigned int* stream_id = (unsigned int*)&data[7];

			char* topic = (char*)&data[11];

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
#ifdef PUBSUB_VERBOSE
				printf("Got unsubscribe request, but it was for a topic we don't have '%s'\n", topic);
#endif
				continue;
			}

			// remove the client
			struct ps_client_t client;
			client.endpoint.address = *addr;
			client.endpoint.port = *port;
			client.stream_id = *stream_id;
			ps_pub_remove_client(pub, &client);
		}
		else if (data[0] == PS_DISCOVERY_PROTOCOL_QUERY_ALL)
		{
			//printf("Got query request\n");
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
			off += ps_serialize_message_definition(&data[off], def);

			//also add other info...
			int sent_bytes = sendto(node->socket, (const char*)data, off, 0, (struct sockaddr*)&address, sizeof(struct sockaddr_in));
		}
	}

	// perform time out checks on publishers
	if (node->_last_check + 2 * 1000 < now)
	{
		node->_last_check = now;

		for (unsigned int i = 0; i < node->num_pubs; i++)
		{
			for (unsigned int c = 0; c < node->pubs[i]->num_clients; c++)
			{
				struct ps_client_t* client = &node->pubs[i]->clients[c];
				// ignore clients with a last keepalive of 0, that signifies that this is a permanent fake client
				if (client->last_keepalive != 0 && client->last_keepalive + 120 * 1000 < now)
				{
#ifdef PUBSUB_VERBOSE
					printf("Client has timed out, unsubscribing...");
#endif
					// remove the client
					struct ps_client_t cl = *client;// make a copy since the old pointer will soon disappear
					ps_pub_remove_client(node->pubs[i], &cl);
				}
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

void ps_node_set_parameter(struct ps_node_t* node, const char* name, double value)
{
	// send a udp message to set the parameter via broadcast
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = node->advertise_addr;
	address.sin_port = htons(node->advertise_port);

	char data[1400];
	data[0] = PS_UDP_PROTOCOL_PARAM_CHANGE;
	*(double*)&data[1] = value;

	int off = sizeof(struct ps_advertise_req_t);
	int len = serialize_string(&data[1+8], name) + 9;

	//also add other info...
	int sent_bytes = sendto(node->socket, (const char*)data, len, 0, (struct sockaddr*)&address, sizeof(struct sockaddr_in));
}
