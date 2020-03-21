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



struct ps_tcp_transport_impl
{
  int socket;
  int* client_sockets;
  int num_client_sockets;

  int* connection_sockets;// for subscriptions
  struct ps_endpoint_t* connection_eps;
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

    free(old_sockets);
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
      ps_pub_add_client(node->pubs[0], &client);
    }
  }

  for (int i = 0; i < impl->num_connections; i++)
  {
    // check for new messages
    char buf[1500];
    int len = recv(impl->connection_sockets[i], buf, 1500, 0);
    if (len > 0)
    {
      printf("Read %i bytes from connection '%s'\n", len, &buf[5]);
    }
  }
}

void ps_tcp_transport_pub(struct ps_transport_t* transport, struct ps_pub_t* publisher, const void* message, uint32_t length)
{
  struct ps_tcp_transport_impl* impl = (struct ps_tcp_transport_impl*)transport->impl;
  // just send it to all of the clients for the moment
  for (int i = 0; i < impl->num_client_sockets; i++)
  {
    printf("publishing real\n");
    // a packet is an int length followed by data
    int8_t packet_type = 0x02;//message
    write(impl->client_sockets[i], &packet_type, 1);

    // make the request
    write(impl->client_sockets[i], &length, 4);
    write(impl->client_sockets[i], ps_get_msg_start(message), length);
  }
}

void ps_tcp_transport_subscribe(struct ps_transport_t* transport, struct ps_sub_t* subscriber, struct ps_endpoint_t* ep)
{
    struct ps_tcp_transport_impl* impl = (struct ps_tcp_transport_impl*)transport->impl;

    // check if we already have a sub with this endpoint
    for (int i = 0; i < impl->num_connections-1; i++)
    {
      if (impl->connection_eps[i].port == ep->port && impl->connection_eps[i].address == ep->address)
      {
        printf("found duplicate\n");
        return;
      }
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(ep->address);
    server_addr.sin_port = htons(ep->port);
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0 )
    {
      perror("error connecting tcp socket");
    }

    // make the subscribe request in a "packet"
    // a packet is an int length followed by data
    int8_t packet_type = 0x01;//subscribe
    write(sock, &packet_type, 1);

    // make the request
    int32_t length = 0;
    char buffer[500];
    length += strlen(subscriber->topic)+1;
    printf("topic %s\n", subscriber->topic);
    strcpy(buffer, subscriber->topic);
    write(sock, &length, 4);
    write(sock, buffer, length);

    // add the socket to the list of connections
    impl->num_connections++;
    int* old_sockets = impl->connection_sockets;
    impl->connection_sockets = (int*)malloc(sizeof(int)*impl->num_connections);
    for (int i = 0; i < impl->num_connections-1; i++)
    {
      impl->connection_sockets[i] = old_sockets[i];
    }
    impl->connection_sockets[impl->num_connections-1] = sock;

    struct ps_endpoint_t* old_eps = impl->connection_eps;
    impl->connection_eps = (struct ps_endpoint_t*)malloc(sizeof(struct ps_endpoint_t)*impl->num_connections);
    for (int i = 0; i < impl->num_connections-1; i++)
    {
      impl->connection_eps[i] = old_eps[i];
    }
    impl->connection_eps[impl->num_connections-1] = *ep;

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

    free(old_sockets);
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
