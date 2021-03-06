#ifndef _PUBSUB_TCP_TRANSPORT_HEADER
#define _PUBSUB_TCP_TRANSPORT_HEADER


#include <pubsub/Node.h>
#include <pubsub/Publisher.h>
#include <pubsub/Subscriber.h>
#include <pubsub/System.h>
#include <pubsub/Net.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef __unix__
#include <signal.h>
#endif

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

    struct ps_sub_t* subscriber;

	bool waiting_for_header;
	int packet_size;

    char packet_type;
    int current_size;
	char* packet_data;
};

struct ps_tcp_client_t
{
  int socket;
  bool needs_removal;

  struct ps_pub_t* publisher;
};

struct ps_tcp_transport_impl
{
  int socket;

  struct ps_tcp_client_t* clients;
  int num_clients;

  struct ps_tcp_transport_connection* connections;
  int num_connections;
};

void remove_client_socket(struct ps_tcp_transport_impl* transport, int socket, struct ps_node_t* node)
{
  // find the index
  int i = 0;
  for (; i < transport->num_clients; i++)
  {
    if (transport->clients[i].socket == socket)// socket packed in address
    {
      break;
    }
  }

#ifdef _WIN32
  closesocket(socket);
#else
  close(socket);
#endif

  struct ps_tcp_client_t* old_clients = transport->clients;
  transport->num_clients -= 1;

  // close the socket and dont wait on it anymore
  ps_event_set_remove_socket(&node->events, transport->clients[i].socket);

  if (transport->num_clients)
  {
    transport->clients = (struct ps_tcp_client_t*)malloc(sizeof(struct ps_tcp_client_t)*transport->num_clients);
    for (int j = 0; j < i; j++)
    {
      transport->clients[j] = old_clients[j];
    }

    for (int j = i + 1; j < transport->num_clients; j++)
    {
      transport->clients[j - 1] = old_clients[j];
    }
  }
  free(old_clients);
}

void ps_tcp_transport_spin(struct ps_transport_t* transport, struct ps_node_t* node)
{
  struct ps_tcp_transport_impl* impl = (struct ps_tcp_transport_impl*)transport->impl;
  int socket = accept(impl->socket, 0, 0);
  if (socket > 0)
  {
    printf("Got new socket connection!\n");

    // add it to the list yo
    impl->num_clients++;
    struct ps_tcp_client_t* old_sockets = impl->clients;
    impl->clients = (struct ps_tcp_client_t*)malloc(sizeof(struct ps_tcp_client_t)*impl->num_clients);
    for (int i = 0; i < impl->num_clients-1; i++)
    {
      impl->clients[i] = old_sockets[i];
    }
    impl->clients[impl->num_clients-1].socket = socket;
    impl->clients[impl->num_clients - 1].needs_removal = false;

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

    ps_event_set_add_socket(&node->events, socket);

	if (impl->num_clients - 1)
	{
	  free(old_sockets);
	}
  }
  //printf("polled\n");

  // remove any old sockets
  for (int i = 0; i < impl->num_clients; i++)
  {
    if (impl->clients[i].needs_removal)
    {
      // add it to the list
      struct ps_client_t client;
      client.endpoint.address = impl->clients[i].socket;
      client.endpoint.port = 255;// p->port;
      ps_pub_remove_client(impl->clients[i].publisher, &client);// todo this is probably unsafe....

      remove_client_socket(impl, impl->clients[i].socket, impl->clients[i].publisher->node);

      i = i - 1;
      return;
    }
  }

  // update our sockets yo
  for (int i = 0; i < impl->num_clients; i++)
  {
    // check for new data
    char buf[1500];
    int len = recv(impl->clients[i].socket, buf, 1500, 0);
    if (len > 0)
    {
      //printf("Read %i bytes from client '%s'\n", len, &buf[5]);

      const char* topic = &buf[5+4];
      // check if this matches any of our publishers
      for (unsigned int pi = 0; pi < node->num_pubs; pi++)
      {
        struct ps_pub_t* pub = node->pubs[pi];
        if (strcmp(topic, pub->topic) == 0)
        {
          unsigned int skip = *(unsigned int*)&buf[5];
          // send response and start publishing
          struct ps_client_t client;
          client.endpoint.address = impl->clients[i].socket;
          client.endpoint.port = 255;// p->port;
          client.last_keepalive = 10000000000000;//GetTickCount64();// use the current time stamp
          client.sequence_number = 0;
          client.stream_id = 0;
          client.modulo = skip > 0 ? skip + 1 : 0;
          client.transport = transport;

          impl->clients[i].publisher = pub;

          //printf("Got subscribe request, adding client if we haven't already\n");
          ps_pub_add_client(pub, &client);

          // send the client the acknowledgement and message definition
          int8_t packet_type = 0x03;//message definition
          send(impl->clients[i].socket, (char*)&packet_type, 1, 0);

          char buf[1500];
          int length = ps_serialize_message_definition((void*)buf, pub->message_definition);
          send(impl->clients[i].socket, (char*)&length, 4, 0);
          send(impl->clients[i].socket, buf, length, 0);
          break;
        }
      }
    }
  }

  for (int i = 0; i < impl->num_connections; i++)
  {
    struct ps_tcp_transport_connection* connection = &impl->connections[i];
    char buf[1500];
    // if we havent gotten a header yet, just check for that
    if (connection->waiting_for_header)
    {
      const int header_size = 5;
      int len = recv(connection->socket, buf, header_size, MSG_PEEK);
      if (len < header_size)
      {
        continue;// no header yet
      }

      char message_type = buf[0];

      // we actually got the header! start looking for the message
      len = recv(connection->socket, buf, header_size, 0);
      connection->packet_type = message_type;
      connection->waiting_for_header = false;
      connection->packet_size = *(int*)&buf[1];
      //printf("Incoming message with %i bytes\n", impl->connections[i].packet_size);
      connection->packet_data = (char*)malloc(connection->packet_size);

      connection->current_size = 0;
	}
    else // read in the message
    {
      int remaining_size = connection->packet_size - connection->current_size;
      // check for new messages and read until we hit packet size
      int len = recv(connection->socket, &connection->packet_data[connection->current_size], remaining_size, 0);
      if (len > 0)
      {
        //printf("Read %i bytes of message\n", len);
        connection->current_size += len;

        if (connection->current_size == connection->packet_size)
        {
          //printf("message finished\n");

          if (connection->packet_type == 0x3)
          {
            //printf("Was message definition");
            if (connection->subscriber->want_message_definition)
            {
              ps_deserialize_message_definition(connection->packet_data, &connection->subscriber->received_message_def);
            }
            
            free(connection->packet_data);
            connection->waiting_for_header = true;
            return;
          }

          // decode and add it to the queue
          struct ps_msg_info_t message_info;
          message_info.address = connection->endpoint.address;
          message_info.port = connection->endpoint.port;

          void* out_data;
          if (connection->subscriber->type)
          {
            out_data = connection->subscriber->type->decode(connection->packet_data, connection->subscriber->allocator);
            free(connection->packet_data);
          }
          else
          {
            out_data = connection->packet_data;
          }

          ps_sub_enqueue(connection->subscriber,
            out_data,
            connection->packet_size,
            &message_info);

          connection->waiting_for_header = true;
        }
      }
    }
  }
}

void ps_tcp_transport_pub(struct ps_transport_t* transport, struct ps_pub_t* publisher, struct ps_client_t* client, const void* message, uint32_t length)
{
  struct ps_tcp_transport_impl* impl = (struct ps_tcp_transport_impl*)transport->impl;

  // the client packs the socket id in the addr
  int socket = client->endpoint.address;

  printf("publishing tcp\n");
  // a packet is an int length followed by data
  int8_t packet_type = 0x02;//message
  if (send(socket, (char*)&packet_type, 1, 0) < 0)
  {
    //perror("send error, removing socket");
    // remove this socket
    // add this to the list of clients to remove if it doesnt exist
    for (int i = 0; i < impl->num_clients; i++)
    {
      if (impl->clients[i].socket == socket)
      {
        impl->clients[i].needs_removal = true;
        return;// we already have it in the list
      }
    }

    return;
  }

  send(socket, (char*)&length, 4, 0);
  send(socket, (char*)ps_get_msg_start(message), length, 0);
}

void ps_tcp_transport_subscribe(struct ps_transport_t* transport, struct ps_sub_t* subscriber, struct ps_endpoint_t* ep)
{
    struct ps_tcp_transport_impl* impl = (struct ps_tcp_transport_impl*)transport->impl;

    // check if we already have a sub with this endpoint
    // if so, ignore it
    for (int i = 0; i < impl->num_connections; i++)
    {
      if (impl->connections[i].endpoint.port == ep->port && impl->connections[i].endpoint.address == ep->address)
      {
        return;
      }
    }

#ifdef _WIN32
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
#else
    int sock = socket(AF_INET, SOCK_STREAM, 0);
#endif

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(ep->address);
    server_addr.sin_port = htons(ep->port);
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0 )
    {
      perror("error connecting tcp socket");
    }

	//printf("%i %i %i %i\n", (ep->address & 0xFF000000) >> 24, (ep->address & 0xFF0000) >> 16, (ep->address & 0xFF00) >> 8, (ep->address & 0xFF));

    // make the subscribe request in a "packet"
    // a packet is an int length followed by data
    int8_t packet_type = 0x01;//subscribe
    send(sock, (char*)&packet_type, 1, 0);

    // make the request
    int32_t length = 0;
    char buffer[500];
    length += strlen(subscriber->topic)+1 + 4;
    strcpy(buffer, subscriber->topic);
    send(sock, (char*)&length, 4, 0);
    unsigned int skip = subscriber->skip;
    send(sock, (char*)&skip, 4, 0);
    send(sock, buffer, length, 0);

    // add the socket to the list of connections
    impl->num_connections++;
    struct ps_tcp_transport_connection* old_connections = impl->connections;
    impl->connections = (struct ps_tcp_transport_connection*)malloc(sizeof(struct ps_tcp_transport_connection)*impl->num_connections);
    for (int i = 0; i < impl->num_connections-1; i++)
    {
	  impl->connections[i] = old_connections[i];
    }
    impl->connections[impl->num_connections-1].socket = sock;
    impl->connections[impl->num_connections-1].endpoint = *ep;
	impl->connections[impl->num_connections-1].waiting_for_header = true;
    impl->connections[impl->num_connections-1].subscriber = subscriber;

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

    ps_event_set_add_socket(&subscriber->node->events, sock);

	if (impl->num_connections - 1)
	{
       free(old_connections);
	}
}


void ps_tcp_transport_destroy(struct ps_transport_t* transport)
{
  struct ps_tcp_transport_impl* impl = (struct ps_tcp_transport_impl*)transport->impl;

  // Free our subscribers and any buffers
  for (int i = 0; i < impl->num_connections; i++)
  {
    if (!impl->connections[i].waiting_for_header)
    {
      free(impl->connections[i].packet_data);
    }
    ps_event_set_remove_socket(&impl->connections[i].subscriber->node->events, impl->connections[i].socket);
#ifdef _WIN32
    closesocket(impl->connections[i].socket);
#else
    close(impl->connections[i].socket);
#endif
  }

  for (int i = 0; i < impl->num_clients; i++)
  {
    ps_event_set_remove_socket(&impl->clients[i].publisher->node->events, impl->clients[i].socket);
#ifdef _WIN32
    closesocket(impl->clients[i].socket);
#else
    close(impl->clients[i].socket);
#endif
  }

#ifdef _WIN32
  closesocket(impl->socket);
#else
  close(impl->socket);
#endif

  if (impl->num_clients)
  {
    free(impl->clients);
  }

  if (impl->num_connections)
  {
    free(impl->connections);
  }

  free(impl);
}

void ps_tcp_transport_init(struct ps_transport_t* transport, struct ps_node_t* node)
{
#ifdef __unix__
    signal(SIGPIPE, SIG_IGN);
#endif

    transport->spin = ps_tcp_transport_spin;
    transport->subscribe = ps_tcp_transport_subscribe;
    transport->destroy = ps_tcp_transport_destroy;
    transport->pub = ps_tcp_transport_pub;
    transport->uuid = 1;

    struct ps_tcp_transport_impl* impl = (struct ps_tcp_transport_impl*)malloc(sizeof(struct ps_tcp_transport_impl));

    impl->num_clients = 0;
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

    ps_event_set_add_socket(&node->events, impl->socket);

    transport->impl = (void*) impl;
}

#endif
