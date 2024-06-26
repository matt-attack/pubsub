#ifndef _PUBSUB_TCP_TRANSPORT_HEADER
#define _PUBSUB_TCP_TRANSPORT_HEADER

// Must be included first on windows
#include <pubsub/Net.h>

#include <pubsub/Node.h>
#include <pubsub/Publisher.h>
#include <pubsub/Subscriber.h>
#include <pubsub/System.h>
//#include <pubsub/Net.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef __unix__
#include <signal.h>
#endif

#define PUBSUB_TCP_TRANSPORT 1

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

  bool connecting;
  bool waiting_for_header;
  int packet_size;

  char packet_type;
  int current_size;
  char* packet_data;
};

struct ps_tcp_client_queued_message_t
{
  char* data;
  int32_t length;
};

struct ps_tcp_client_t
{
  int socket;
  bool needs_removal;

  struct ps_pub_t* publisher;

  int32_t current_packet_size;
  int32_t desired_packet_size;
  char* packet_data;

  char* queued_message;
  int32_t queued_message_length;
  int32_t queued_message_written;

  // this stores queued messages > 1
  int32_t num_queued_messages;
  struct ps_tcp_client_queued_message_t* queued_messages;
};

struct ps_tcp_transport_impl
{
  int socket;

  struct ps_node_t* node;

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

  if (transport->clients[i].packet_data)
  {
    free(transport->clients[i].packet_data);
  }

  if (transport->clients[i].queued_message)
  {
    free(transport->clients[i].queued_message);
  }

  // free queued messages
  if (transport->clients[i].num_queued_messages)
  {
    for (int j = 0; j < transport->clients[i].num_queued_messages; j++)
    {
      free(transport->clients[i].queued_messages[j].data);
    }
    free(transport->clients[i].queued_messages);
  }

  struct ps_tcp_client_t* old_clients = transport->clients;
  transport->num_clients -= 1;

  // close the socket and dont wait on it anymore
  ps_event_set_remove_socket(&node->events, transport->clients[i].socket);

  if (transport->num_clients)
  {
    transport->clients = (struct ps_tcp_client_t*)malloc(sizeof(struct ps_tcp_client_t) * transport->num_clients);
    for (int j = 0; j < i; j++)
    {
      transport->clients[j] = old_clients[j];
    }

    for (int j = i + 1; j <= transport->num_clients; j++)
    {
      transport->clients[j - 1] = old_clients[j];
    }
  }
  free(old_clients);
}

void ps_tcp_remove_connection(struct ps_tcp_transport_impl* impl, int index)
{
    // Free our subscribers and any buffers
    int iter = 0;
    int new_size = impl->num_connections - 1;
    struct ps_tcp_transport_connection* new_connections = new_size == 0 ? 0 : (struct ps_tcp_transport_connection*)malloc(sizeof(struct ps_tcp_transport_connection) * new_size);
    for (int i = 0; i < impl->num_connections; i++)
    {
      if (i != index)
      {
        new_connections[iter++] = impl->connections[i];
        continue;
      }

      if (!impl->connections[i].waiting_for_header)
      {
        free(impl->connections[i].packet_data);
      }
      ps_event_set_remove_socket(&impl->node->events, impl->connections[i].socket);
#ifdef _WIN32
      closesocket(impl->connections[i].socket);
#else
      close(impl->connections[i].socket);
#endif
    }
    impl->num_connections = new_size;
    free(impl->connections);
    impl->connections = new_connections;
}

int ps_tcp_transport_spin(struct ps_transport_t* transport, struct ps_node_t* node)
{
  struct ps_tcp_transport_impl* impl = (struct ps_tcp_transport_impl*)transport->impl;
  int socket = accept(impl->socket, 0, 0);
  if (socket > 0)
  {
#ifdef PUBSUB_VERBOSE
    printf("Got new socket connection!\n");
#endif

    // add it to the list yo
    impl->num_clients++;
    struct ps_tcp_client_t* old_sockets = impl->clients;
    impl->clients = (struct ps_tcp_client_t*)malloc(sizeof(struct ps_tcp_client_t) * impl->num_clients);
    for (int i = 0; i < impl->num_clients - 1; i++)
    {
      impl->clients[i] = old_sockets[i];
    }
    struct ps_tcp_client_t* new_client = &impl->clients[impl->num_clients - 1];
    new_client->socket = socket;
    new_client->socket = socket;
    new_client->needs_removal = false;
    new_client->current_packet_size = 0;
    new_client->desired_packet_size = 0;
    new_client->packet_data = 0;
    new_client->queued_message = 0;
    new_client->queued_message_length = 0;
    new_client->queued_message_written = 0;
    new_client->queued_messages = 0;
    new_client->num_queued_messages = 0;

    // set non-blocking
#ifdef _WIN32
    DWORD nonBlocking = 1;
    if (ioctlsocket(socket, FIONBIO, &nonBlocking) != 0)
    {
      printf("Failed to Set Socket as Non-Blocking!\n");
      closesocket(socket);
      return 0;
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
      client.stream_id = 0;
      ps_pub_remove_client(impl->clients[i].publisher, &client);// todo this is probably unsafe....

      remove_client_socket(impl, impl->clients[i].socket, impl->clients[i].publisher->node);

      i = i - 1;
      break;
    }
  }

  // update our sockets yo
  for (int i = 0; i < impl->num_clients; i++)
  {
    struct ps_tcp_client_t* client = &impl->clients[i];

    // send queued messages until we block or cant send anymore
    while (client->queued_message != 0)
    {
      int to_send = client->queued_message_length - client->queued_message_written;
      int sent = send(client->socket, &client->queued_message[client->queued_message_written], to_send, 0);
      if (sent > 0)
      {
        client->queued_message_written += sent;
      }
#ifdef WIN32
      else if (sent < 0 && WSAGetLastError() != WSAEWOULDBLOCK)
#else
      else if (sent < 0 && errno != EAGAIN)
#endif
      {
        client->needs_removal = true;
      }

      if (client->queued_message_written == client->queued_message_length)
      {
        //printf("Message sent.\n");
        free(client->queued_message);
        client->queued_message = 0;

        // we finished! check if there are more to send
        if (client->num_queued_messages > 0)
        {
          // grab a message from the front of our message queue
          client->queued_message = client->queued_messages[0].data;
          client->queued_message_written = 0;
          client->queued_message_length = client->queued_messages[0].length;

          client->num_queued_messages -= 1;
          if (client->num_queued_messages == 0)
          {
            free(client->queued_messages);
            client->queued_messages = 0;
            continue;
          }

          struct ps_tcp_client_queued_message_t* msgs = (struct ps_tcp_client_queued_message_t*)malloc(client->num_queued_messages * sizeof(struct ps_tcp_client_queued_message_t));
          for (int i = 0; i < client->num_queued_messages; i++)
          {
            msgs[i] = client->queued_messages[i + 1];// take from the front
          }
          free(client->queued_messages);
          client->queued_messages = msgs;

          // continue so we can attempt to send again
        }
        else
        {
          ps_event_set_remove_socket_write(&node->events, client->socket);
          break;// no more to send
        }
      }
      else
      {
        break;// we couldnt send anymore atm
      }
    }

    // check for new data and add it to the packet if present
    char buf[1500];
    // if we havent gotten a header yet, just check for that
    if (client->desired_packet_size == 0)
    {
      const int header_size = 5;
      int len = recv(client->socket, buf, header_size, MSG_PEEK);
      if (len == 0)
      {
        client->needs_removal = true;
        continue;
      }
      if (len < header_size)
      {
        continue;// no header yet
      }

      char message_type = buf[0];// not used atm

      // we actually got the header! start looking for the message
      len = recv(client->socket, buf, header_size, 0);
      //connection->packet_type = message_type;
      //client->waiting_for_header = false;
      client->desired_packet_size = *(uint32_t*)&buf[1];
      //printf("Incoming message with %i bytes\n", client->desired_packet_size);
      client->packet_data = (char*)malloc(client->desired_packet_size);

      client->current_packet_size = 0;
    }
    // read in the message
    if (client->desired_packet_size != 0)
    {
      int remaining_size = client->desired_packet_size - client->current_packet_size;
      // check for new messages and read until we hit packet size
      int len = recv(client->socket, &client->packet_data[client->current_packet_size], remaining_size, 0);
      if (len > 0)
      {
        //printf("Read %i bytes of message\n", len);
        client->current_packet_size += len;

        if (client->current_packet_size == client->desired_packet_size)
        {
#ifdef PUBSUB_VERBOSE
          printf("message finished\n");
#endif

          if (true)// todo look at message id
          {
            // its a subscribe
            const char* topic = &client->packet_data[4];
            // check if this matches any of our publishers
            for (unsigned int pi = 0; pi < node->num_pubs; pi++)
            {
              struct ps_pub_t* pub = node->pubs[pi];
              if (strcmp(topic, pub->topic) == 0)
              {
                uint32_t skip = *(uint32_t*)&client->packet_data[0];
                // send response and start publishing
                struct ps_client_t sub_client;
                sub_client.endpoint.address = client->socket;
                sub_client.endpoint.port = 255;// p->port;
                sub_client.last_keepalive = 10000000000000;//GetTickCount64();// use the current time stamp
                sub_client.sequence_number = 0;
                sub_client.stream_id = 0;
                sub_client.modulo = skip > 0 ? skip + 1 : 0;
                sub_client.transport = transport;

                impl->clients[i].publisher = pub;

                // send the client the acknowledgement and message definition
                int8_t packet_type = 0x03;//message definition
                send(impl->clients[i].socket, (char*)&packet_type, 1, 0);

                char buf[1500];
                int32_t length = ps_serialize_message_definition((void*)buf, pub->message_definition);
                send(impl->clients[i].socket, (char*)&length, 4, 0);
                send(impl->clients[i].socket, buf, length, 0);

#ifdef PUBSUB_VERBOSE
                printf("TCPTransport: Got subscribe request, adding client if we haven't already\n");
#endif
                ps_pub_add_client(pub, &sub_client);

                break;
              }
            }
          }

          free(client->packet_data);
          client->packet_data = 0;
          client->desired_packet_size = 0;
        }
      }
    }
  }

  int message_count = 0;
  for (int i = 0; i < impl->num_connections; i++)
  {
    struct ps_tcp_transport_connection* connection = &impl->connections[i];
    char buf[1500];
    if (connection->connecting)
    {
      //printf("checking for connected\n");
      // select to check for writability
      fd_set wfds;
      struct timeval tv;
      int retval;

      FD_ZERO(&wfds);
      FD_SET(connection->socket, &wfds);

      tv.tv_sec = 0;
      tv.tv_usec = 0;
      retval = select(connection->socket + 1, NULL, &wfds, NULL, &tv);
      if (retval == -1)
      {
        // error?
        printf("socket errored while connecting\n");
      }
      else if (retval)
      {
        // socket is writable
        //printf("socket writable\n");

        // make the subscribe request in a "packet"
        // a packet is an int length followed by data
        int8_t packet_type = 0x01;//subscribe
        send(connection->socket, (char*)&packet_type, 1, 0);

        int32_t length = strlen(connection->subscriber->topic) + 1 + 4;
        send(connection->socket, (char*)&length, 4, 0);

        // make the request
        char buffer[500];
        strcpy(buffer, connection->subscriber->topic);
        uint32_t skip = connection->subscriber->skip;
        send(connection->socket, (char*)&skip, 4, 0);
        send(connection->socket, buffer, length - 4, 0);
	
        connection->connecting = false;
      }
    }
    // if we havent gotten a header yet, just check for that
    else if (connection->waiting_for_header)
    {
      const int header_size = 5;
      int len = recv(connection->socket, buf, header_size, MSG_PEEK);
      //printf("peek got: %i\n", len);
      if (len == 0)
      {
        // we got disconnected
        ps_tcp_remove_connection(impl, i);
        i--;
        continue;
      }
      else if (len < header_size)
      {
        continue;// no header yet
      }

      char message_type = buf[0];

      // we actually got the header! start looking for the message
      len = recv(connection->socket, buf, header_size, 0);
      connection->packet_type = message_type;
      connection->waiting_for_header = false;
      connection->packet_size = *(uint32_t*)&buf[1];
      //printf("Incoming message with %i bytes\n", impl->connections[i].packet_size);
      connection->packet_data = (char*)malloc(connection->packet_size);

      connection->current_size = 0;
    }
    else // read in the message
    {
      int remaining_size = connection->packet_size - connection->current_size;

      // check for new messages and read until we hit packet size
      int len = recv(connection->socket, &connection->packet_data[connection->current_size], remaining_size, 0);
      if (len == 0)
      {
        // we got disconnected
        ps_tcp_remove_connection(impl, i);
        i--;
        continue;
      }
      else if (len > 0)
      {
        //printf("Read %i bytes of message\n", len);
        connection->current_size += len;

        if (connection->current_size == connection->packet_size)
        {
          //printf("message finished type %x\n", connection->packet_type);
          if (connection->packet_type == 0x3)
          {
            //printf("Was message definition\n");
            if (connection->subscriber->type == 0)
            {
              // todo put this in a function so we cant accidentally forget it
              if (connection->subscriber->received_message_def.fields == 0)
              {
                ps_deserialize_message_definition(connection->packet_data, &connection->subscriber->received_message_def);
              }

              // call the callback as well
              if (node->def_cb)
              {
                node->def_cb(&connection->subscriber->received_message_def, node->def_cb_data);
              }
            }

            free(connection->packet_data);
          }
          else if (connection->packet_type == 0x2)
          {
            //printf("added to queue\n");
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

            // remove the reference to packet data so we dont try and double free it on destroy
            // it is the queue's responsibility now
            connection->packet_data = 0;
            ps_sub_enqueue(connection->subscriber,
              out_data,
              connection->packet_size,
              &message_info);

            message_count++;
          }
          else
          {
            // unhandled packet id
            free(connection->packet_data);
          }
          connection->waiting_for_header = true;
        }
      }
    }
  }
  return message_count;
}

void ps_tcp_transport_pub(struct ps_transport_t* transport, struct ps_pub_t* publisher, struct ps_client_t* client, const void* message, uint32_t length)
{
  struct ps_tcp_transport_impl* impl = (struct ps_tcp_transport_impl*)transport->impl;

  // the client packs the socket id in the addr
  int socket = client->endpoint.address;

  // okay, so new version, if any write fails (EAGAIN or < expected size)
  // we make a copy of the entire message and store it on that client to try and send again in our update loop
  // if we get into this function and this client already has a queued message, just drop this one
  struct ps_tcp_client_t* tclient = 0;
  for (int i = 0; i < impl->num_clients; i++)
  {
    if (impl->clients[i].socket == socket)
    {
      tclient = &impl->clients[i];
      break;
    }
  }

  if (tclient->queued_message != 0)
  {
    // check if we have queue space left

        // for now hardcode max queue size
    const int max_queue_size = 10;

    // copy the message to put it in the queue
    // todo remove this copy
    char* data = (char*)malloc(length + 4 + 1);
    data[0] = 0x02;
    *((uint32_t*)&data[1]) = length;
    memcpy(&data[5], ps_get_msg_start(message), length);

    // this if statement is unnecessary, but I added it for the sake of testing/completeness
    if (tclient->queued_message == 0)
    {
      tclient->queued_message = data;
      tclient->queued_message_length = length + 5;
      tclient->queued_message_written = 0;
    }
    else if (tclient->num_queued_messages >= max_queue_size)
    {
      // todo use a deque lol
      // swap everything down, freeing the first
      for (int i = tclient->num_queued_messages - 1; i >= 1; i--)
      {
        tclient->queued_messages[i] = tclient->queued_messages[i - 1];
      }
      tclient->queued_messages[0].data = data;
      tclient->queued_messages[0].length = length + 5;
      //printf("dropped message on topic '%s'\n", publisher->topic);
      return;// drop it, we are out of queue space
    }
    else
    {
      //printf("queuing up message %i on topic '%s'\n", tclient->num_queued_messages, publisher->topic);

      // add the message to the front of the queue
      tclient->num_queued_messages += 1;
      struct ps_tcp_client_queued_message_t* msgs = (struct ps_tcp_client_queued_message_t*)malloc(tclient->num_queued_messages * sizeof(struct ps_tcp_client_queued_message_t));

      msgs[0].data = data;
      msgs[0].length = length + 5;
      for (int i = 0; i < tclient->num_queued_messages - 1; i++)
      {
        msgs[i + 1] = tclient->queued_messages[i];
      }
      free(tclient->queued_messages);
      tclient->queued_messages = msgs;

      return;
    }
  }
  //printf("started writing\n");
  // try and write, if any of these fail, make a copy
  uint8_t packet_type = 0x02;
  int c = send(socket, (char*)&packet_type, 1, 0);
  if (c == 0)
  {
    tclient->queued_message_written = 0;
    goto FAILCOPY;
  }
  if (c < 0)
  {
#ifdef WIN32
    int error = WSAGetLastError();
    if (error == WSAEWOULDBLOCK)
#else
    if (errno == EAGAIN || errno == EWOULDBLOCK)
#endif
    {
      tclient->queued_message_written = 0;
      goto FAILCOPY;
    }
    goto FAILDISCONNECT;
  }

  c = send(socket, (char*)&length, 4, 0);
  if (c < 4 && c >= 0)
  {
    tclient->queued_message_written = c + 1;
    goto FAILCOPY;
  }
  if (c < 0)
  {
#ifdef WIN32
    int error = WSAGetLastError();
    if (error == WSAEWOULDBLOCK)
#else
    if (errno == EAGAIN || errno == EWOULDBLOCK)
#endif
    {
      tclient->queued_message_written = 1;
      goto FAILCOPY;
    }
    goto FAILDISCONNECT;
  }

  //printf("sending %i bytes\n", length + 4 + 1);
  c = send(socket, (char*)ps_get_msg_start(message), length, 0);
  if (c < length && c >= 0)
  {
    tclient->queued_message_written = c + 5;
    goto FAILCOPY;
  }
  if (c < 0)
  {
#ifdef WIN32
    int error = WSAGetLastError();
    if (error == WSAEWOULDBLOCK)
#else
    if (errno == EAGAIN || errno == EWOULDBLOCK)
#endif
    {
      tclient->queued_message_written = 5;
      goto FAILCOPY;
    }
    goto FAILDISCONNECT;
  }
  //printf("wrote all\n");
  return;

  char* data;
FAILDISCONNECT:
  //printf("Disconnected: %s\n", strerror(err));
  tclient->needs_removal = true;
  return;

FAILCOPY:
  // todo remove this copy
  data = (char*)malloc(length + 4 + 1);
  data[0] = 0x02;
  *((uint32_t*)&data[1]) = length;
  memcpy(&data[5], ps_get_msg_start(message), length);

  tclient->queued_message = data;
  tclient->queued_message_length = length + 5;
  ps_event_set_add_socket_write(&publisher->node->events, socket);
  return;
}

void ps_tcp_transport_subscribe(struct ps_transport_t* transport, struct ps_sub_t* subscriber, struct ps_endpoint_t* ep, uint32_t transport_info)
{
  struct ps_tcp_transport_impl* impl = (struct ps_tcp_transport_impl*)transport->impl;

  // check if we already have a sub for this subscriber with this endpoint
  // if so, ignore it
  for (int i = 0; i < impl->num_connections; i++)
  {
    if (impl->connections[i].endpoint.port == ep->port &&
      impl->connections[i].endpoint.address == ep->address &&
      impl->connections[i].subscriber->sub_id == subscriber->sub_id)
    {
      return;
    }
  }

#ifdef _WIN32
  SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
#else
  int sock = socket(AF_INET, SOCK_STREAM, 0);
#endif

  // set non-blocking
#ifdef _WIN32
  DWORD nonBlocking = 1;
  if (ioctlsocket(sock, FIONBIO, &nonBlocking) != 0)
  {
    ps_print_socket_error("Failed to Set Socket as Non-Blocking");
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

  // Actually connect
  //printf("connecting\n");
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(ep->address);
  server_addr.sin_port = htons(transport_info);
  int connect_result = connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
  if (connect_result != 0)
  {
#ifdef _WIN32
    if (WSAGetLastError() != WSAEWOULDBLOCK)//WSAEINPROGRESS
#else
    if (errno != EINPROGRESS)
#endif
    {
      ps_print_socket_error("error connecting tcp socket");
	  return;
    }
  }

  //printf("%i %i %i %i\n", (ep->address & 0xFF000000) >> 24, (ep->address & 0xFF0000) >> 16, (ep->address & 0xFF00) >> 8, (ep->address & 0xFF));

  // make the subscribe request in a "packet"
  // a packet is an int length followed by data
  /*int8_t packet_type = 0x01;//subscribe
  send(sock, (char*)&packet_type, 1, 0);

  int32_t length = strlen(subscriber->topic) + 1 + 4;
  send(sock, (char*)&length, 4, 0);

  // make the request
  char buffer[500];
  strcpy(buffer, subscriber->topic);
  uint32_t skip = subscriber->skip;
  send(sock, (char*)&skip, 4, 0);
  send(sock, buffer, length - 4, 0);*/

  // add the socket to the list of connections
  impl->num_connections++;
  struct ps_tcp_transport_connection* old_connections = impl->connections;
  impl->connections = (struct ps_tcp_transport_connection*)malloc(sizeof(struct ps_tcp_transport_connection) * impl->num_connections);
  for (int i = 0; i < impl->num_connections - 1; i++)
  {
    impl->connections[i] = old_connections[i];
  }
  
  struct ps_tcp_transport_connection* new_connection = &impl->connections[impl->num_connections - 1];
  new_connection->socket = sock;
  new_connection->endpoint = *ep;
  new_connection->waiting_for_header = true;
  new_connection->subscriber = subscriber;
  new_connection->connecting = true;

  ps_event_set_add_socket(&subscriber->node->events, sock);

  if (impl->num_connections - 1)
  {
    free(old_connections);
  }
}

void ps_tcp_transport_unsubscribe(struct ps_transport_t* transport, struct ps_sub_t* subscriber)
{
  struct ps_tcp_transport_impl* impl = (struct ps_tcp_transport_impl*)transport->impl;

  // remove all transports which reference this subscriber
  int num_to_remove = 0;
  for (int i = 0; i < impl->num_connections; i++)
  {
    if (impl->connections[i].subscriber == subscriber)
    {
      num_to_remove++;
    }
  }

#ifdef PUBSUB_VERBOSE
  printf("Removing %i tcp subs\n", num_to_remove);
#endif

  if (num_to_remove > 0)
  {
    // Free our subscribers and any buffers
    int iter = 0;
    int new_size = impl->num_connections - num_to_remove;
    struct ps_tcp_transport_connection* new_connections = new_size == 0 ? 0 : (struct ps_tcp_transport_connection*)malloc(sizeof(struct ps_tcp_transport_connection) * new_size);
    for (int i = 0; i < impl->num_connections; i++)
    {
      if (impl->connections[i].subscriber != subscriber)
      {
        new_connections[iter++] = impl->connections[i];
        continue;
      }

      if (!impl->connections[i].waiting_for_header)
      {
        free(impl->connections[i].packet_data);
      }
      ps_event_set_remove_socket(&impl->node->events, impl->connections[i].socket);
#ifdef _WIN32
      closesocket(impl->connections[i].socket);
#else
      close(impl->connections[i].socket);
#endif
    }
    impl->num_connections = new_size;
    free(impl->connections);
    impl->connections = new_connections;
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
    ps_event_set_remove_socket(&impl->node->events, impl->connections[i].socket);
#ifdef _WIN32
    closesocket(impl->connections[i].socket);
#else
    close(impl->connections[i].socket);
#endif
  }

  for (int i = 0; i < impl->num_clients; i++)
  {
    ps_event_set_remove_socket(&impl->node->events, impl->clients[i].socket);
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
  transport->unsubscribe = ps_tcp_transport_unsubscribe;
  transport->destroy = ps_tcp_transport_destroy;
  transport->pub = ps_tcp_transport_pub;
  transport->uuid = 1;

  struct ps_tcp_transport_impl* impl = (struct ps_tcp_transport_impl*)malloc(sizeof(struct ps_tcp_transport_impl));

  impl->num_clients = 0;
  impl->num_connections = 0;

  impl->node = node;

  impl->socket = socket(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = 0;// we want an ephemeral port
  if (bind(impl->socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
  {
    ps_print_socket_error("error binding tcp transport socket");
  }

  socklen_t outlen = sizeof(struct sockaddr_in);
  struct sockaddr_in outaddr;
  getsockname(impl->socket, (struct sockaddr*)&outaddr, &outlen);
  transport->transport_info = ntohs(outaddr.sin_port);

  printf("Bound tcp to %i\n", transport->transport_info);

  // set non-blocking
#ifdef _WIN32
  DWORD nonBlocking = 1;
  if (ioctlsocket(impl->socket, FIONBIO, &nonBlocking) != 0)
  {
    ps_print_socket_error("Failed to Set Socket as Non-Blocking");
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

  transport->impl = (void*)impl;
}

#endif
