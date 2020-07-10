#ifndef _PUBSUB_TCP_TRANSPORT_HEADER
#define _PUBSUB_TCP_TRANSPORT_HEADER


#include <pubsub/Node.h>
#include <pubsub/Publisher.h>
#include <pubsub/Subscriber.h>
#include <pubsub/System.h>
#include <pubsub/Net.h>

#include <stdlib.h>
#include <string.h>

/*

typedef void(*ps_transport_fn_pub_t)(struct ps_transport_t* transport, struct ps_pub_t* publisher, void* message);
typedef void(*ps_transport_fn_spin_t)(struct ps_transport_t* transport, struct ps_node_t* node);
typedef void(*ps_transport_fn_add_publisher_t)(struct ps_transport_t* transport, struct ps_pub_t* publisher);
typedef void(*ps_transport_fn_remove_publisher_t)(struct ps_transport_t* transport, struct ps_pub_t* publisher);
typedef void(*ps_transport_fn_subscribe_t)(struct ps_transport_t* transport, struct ps_sub_t* subscriber, struct ps_endpoint_t* ep);
typedef void(*ps_transport_fn_unsubscribe_t)(struct ps_transport_t* transport, struct ps_sub_t* subscriber);
typedef unsigned int(*ps_transport_fn_num_subscribers_t)(struct ps_transport_t* transport, struct ps_pub_t* publisher);
typedef unsigned int(*ps_transport_fn_add_wait_set_t)(struct ps_transport_t* transport, struct ps_event_set_t* events);
struct ps_transport_t
{
	unsigned short uuid;// unique id for this transport type, listed in advertisements for it
	ps_transport_fn_pub_t pub;
	ps_transport_fn_spin_t spin;
	ps_transport_fn_num_subscribers_t subscriber_count;
	ps_transport_fn_subscribe_t subscribe;
	ps_transport_fn_unsubscribe_t unsubscribe;
	ps_transport_fn_add_publisher_t add_pub;
	ps_transport_fn_remove_publisher_t remove_pub;
    ps_transport_fn_add_wait_set_t add_wait_set;
    void* impl;
};*/

struct ps_tcp_transport_connection
{
	int socket;
	struct ps_endpoint_t endpoint;

	bool waiting_for_header;
	int packet_size;

    int current_size;
	char* packet_data;
};

struct ps_tcp_transport_impl
{
  int socket;
  int* client_sockets;
  int num_client_sockets;

  struct ps_tcp_transport_connection* connections;
  int num_connections;
};

void ps_tcp_transport_spin(struct ps_transport_t* transport, struct ps_node_t* node)
{
  struct ps_tcp_transport_impl* impl = (struct ps_tcp_transport_impl*)transport->impl;
  int socket = accept(impl->socket, 0, 0);
  if (socket > 0)
  {
    printf("Got new socket connection!\n");

    // add it to the list yo
    impl->num_client_sockets++;
    int* old_sockets = impl->client_sockets;
    impl->client_sockets = (int*)malloc(sizeof(int)*impl->num_client_sockets);
    for (int i = 0; i < impl->num_client_sockets-1; i++)
    {
      impl->client_sockets[i] = old_sockets[i];
    }
    impl->client_sockets[impl->num_client_sockets-1] = socket;

    // set non-blocking
#ifdef _WIN32
	DWORD nonBlocking = 1;
	if (ioctlsocket(socket, FIONBIO, &nonBlocking) != 0)
	{
		printf("Failed to Set Socket as Non-Blocking!\n");
		closesocket(socket);
		return;
	}
#endif
#ifdef ARDUINO
    fcntl(socket, F_SETFL, O_NONBLOCK);
#endif
#ifdef __unix__
	int flags = fcntl(socket, F_GETFL);
	fcntl(socket, F_SETFL, flags | O_NONBLOCK);
#endif

	if (impl->num_client_sockets - 1)
	{
	  free(old_sockets);
	}
  }
  //printf("polled\n");

  // update our sockets yo
  for (int i = 0; i < impl->num_client_sockets; i++)
  {
    // check for new data
    char buf[1500];
    int len = recv(impl->client_sockets[i], buf, 1500, 0);
    if (len > 0)
    {
      printf("Read %i bytes '%s'\n", len, &buf[5]);

      // send response and start publishing
      struct ps_client_t client;
      //client.endpoint.address = p->addr;
      //client.endpoint.port = p->port;
      client.last_keepalive = 10000000000000;//GetTickCount64();// use the current time stamp
      client.sequence_number = 0;
      client.stream_id = 0;
      client.modulo = 0;
      client.transport = transport;

      //printf("Got subscribe request, adding client if we haven't already\n");
       todo store something about this client so we can remove it on unsub/exit
      ps_pub_add_client(node->pubs[0], &client);
    }
  }

  for (int i = 0; i < impl->num_connections; i++)
  {
    char buf[1500];
    // if we havent gotten a header yet, just check for that
    if (impl->connections[i].waiting_for_header)
    {
      const int header_size = 5;
      int len = recv(impl->connections[i].socket, buf, header_size, MSG_PEEK);
      if (len < header_size)
      {
        continue;// no header yet
      }

      // we actually got the header! start looking for the message
      len = recv(impl->connections[i].socket, buf, header_size, 0);
      impl->connections[i].waiting_for_header = false;
      impl->connections[i].packet_size = *(int*)&buf[1];
      printf("Incoming message with %i bytes\n", impl->connections[i].packet_size);
      impl->connections[i].packet_data = malloc(impl->connections[i].packet_size);

      impl->connections[i].current_size = 0;
	}
    else // read in the message
    {
      int remaining_size = impl->connections[i].packet_size - impl->connections[i].current_size;
      // check for new messages and read until we hit packet size
      int len = recv(impl->connections[i].socket, &impl->connections[i].packet_data[impl->connections[i].current_size], remaining_size, 0);
      if (len > 0)
      {
        //printf("Read %i bytes of message\n", len);
        impl->connections[i].current_size += len;

        if (impl->connections[i].current_size == impl->connections[i].packet_size)
        {
          printf("message finished %s\n", impl->connections[i].packet_data);
          // todo add it to the queue
          free(impl->connections[i].packet_data);
          impl->connections[i].waiting_for_header = true;
        }
      }
    }
  }
}

void remove_client_socket(struct ps_tcp_transport_impl* transport, int i)
{
  int* old_sockets = transport->client_sockets;
  transport->num_client_sockets -= 1;

  if (transport->num_client_sockets)
  {
    transport->client_sockets = (int*)malloc(sizeof(int)*transport->num_client_sockets);
    for (int j = 0; j < i; j++)
    {
      transport->client_sockets[j] = old_sockets[j];
    }

    for (int j = i+1; j < transport->num_client_sockets; j++)
    {
      transport->client_sockets[j-1] = old_sockets[j];
    }
    free(old_sockets);
  }
  else
  {
    free(transport->client_sockets);
  }
}

void ps_tcp_transport_pub(struct ps_transport_t* transport, struct ps_pub_t* publisher, const void* message, uint32_t length)
{
  struct ps_tcp_transport_impl* impl = (struct ps_tcp_transport_impl*)transport->impl;
  // just send it to all of the clients for the moment
  for (int i = 0; i < impl->num_client_sockets; i++)
  {
    printf("publishing tcp\n");
    // a packet is an int length followed by data
    int8_t packet_type = 0x02;//message
    if (send(impl->client_sockets[i], &packet_type, 1, 0) < 0)
    {
      perror("send error, removing socket");
      // remove this socket

      remove_client_socket(impl, i);
      todo actually unsub
      i = i - 1;

      continue;
    }

    // make the request
    send(impl->client_sockets[i], &length, 4, 0);
    send(impl->client_sockets[i], ps_get_msg_start(message), length, 0);
  }
}

void ps_tcp_transport_subscribe(struct ps_transport_t* transport, struct ps_sub_t* subscriber, struct ps_endpoint_t* ep)
{
    struct ps_tcp_transport_impl* impl = (struct ps_tcp_transport_impl*)transport->impl;

    // check if we already have a sub with this endpoint
    for (int i = 0; i < impl->num_connections; i++)
    {
      if (impl->connections[i].endpoint.port == ep->port && impl->connections[i].endpoint.address == ep->address)
      {
        printf("found duplicate\n");
        return;
      }
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(ep->address);
    server_addr.sin_port = htons(ep->port);
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0 )
    {
      perror("error connecting tcp socket");
    }

	printf("%i %i %i %i\n", (ep->address & 0xFF000000) >> 24, (ep->address & 0xFF0000) >> 16, (ep->address & 0xFF00) >> 8, (ep->address & 0xFF));

    // make the subscribe request in a "packet"
    // a packet is an int length followed by data
    int8_t packet_type = 0x01;//subscribe
    send(sock, &packet_type, 1, 0);

    // make the request
    int32_t length = 0;
    char buffer[500];
    length += strlen(subscriber->topic)+1;
    printf("topic %s\n", subscriber->topic);
    strcpy(buffer, subscriber->topic);
    send(sock, &length, 4, 0);
    send(sock, buffer, length, 0);

    // add the socket to the list of connections
    impl->num_connections++;
    struct ps_tcp_transport_connection* old_connections = impl->connections;
    impl->connections = (struct tcp_transport_connection*)malloc(sizeof(struct ps_tcp_transport_connection)*impl->num_connections);
    for (int i = 0; i < impl->num_connections-1; i++)
    {
	  impl->connections[i] = old_connections[i];
    }
    impl->connections[impl->num_connections-1].socket = sock;
    impl->connections[impl->num_connections-1].endpoint = *ep;
	impl->connections[impl->num_connections-1].waiting_for_header = true;

    // set non-blocking
#ifdef _WIN32
	DWORD nonBlocking = 1;
	if (ioctlsocket(sock, FIONBIO, &nonBlocking) != 0)
	{
		printf("Failed to Set Socket as Non-Blocking!\n");
		closesocket(sock);
		return;
	}
#endif
#ifdef ARDUINO
    fcntl(sock, F_SETFL, O_NONBLOCK);
#endif
#ifdef __unix__
	int flags = fcntl(sock, F_GETFL);
	fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif

	if (impl->num_connections - 1)
	{
       free(old_connections);
	}
}

void ps_tcp_transport_init(struct ps_transport_t* transport, struct ps_node_t* node)
{
    transport->spin = ps_tcp_transport_spin;
    transport->subscribe = ps_tcp_transport_subscribe;
    //transport->add_publisher = ;
    transport->pub = ps_tcp_transport_pub;
    transport->uuid = 1;

    struct ps_tcp_transport_impl* impl = (struct ps_tcp_transport_impl*)malloc(sizeof(struct ps_tcp_transport_impl));

    impl->num_client_sockets = 0;
    impl->num_connections = 0;

    impl->socket = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(node->port);
    if (bind(impl->socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0 )
    {
      perror("error binding tcp socket");
    }

    // set non-blocking
#ifdef _WIN32
	DWORD nonBlocking = 1;
	if (ioctlsocket(impl->socket, FIONBIO, &nonBlocking) != 0)
	{
		printf("Failed to Set Socket as Non-Blocking!\n");
		closesocket(impl->socket);
		return;
	}
#endif
#ifdef ARDUINO
    fcntl(impl->socket, F_SETFL, O_NONBLOCK);
#endif
#ifdef __unix__
	int flags = fcntl(impl->socket, F_GETFL);
	fcntl(impl->socket, F_SETFL, flags | O_NONBLOCK);
#endif

    listen(impl->socket, 5);

    transport->impl = (void*) impl;
}

#endif
