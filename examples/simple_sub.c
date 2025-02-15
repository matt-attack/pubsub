//#include <cstdlib>
#include <stdio.h>

#include <pubsub/Node.h>
#include <pubsub/Publisher.h>
#include <pubsub/Subscriber.h>
#include <pubsub/System.h>
#include <pubsub/TCPTransport.h>

#include <pubsub/String.msg.h>

struct ps_sub_t string_sub;
void callback(void* message, unsigned int size, void* cbdata, const struct ps_msg_info_t* info)
{
  // user is responsible for freeing the message and its arrays
  struct pubsub__String* data = (struct pubsub__String*)message;
  printf("Got message: %s\n", data->value);
  pubsub__String_free(string_sub.allocator, data);
}

int main()
{
  // Create the node
  struct ps_node_t node;
  ps_node_init(&node, "simple_subscriber"/*node name*/, 
               NULL/*IP to bind to, empty string or null to autodetect*/,
               false/*set true to use broadcast instead of multicast advertising*/);

  // Adds TCP transport (optional)
  struct ps_transport_t tcp_transport;
  ps_tcp_transport_init(&tcp_transport, &node);
  ps_node_add_transport(&node, &tcp_transport);

  // Create the subscriber
  struct ps_subscriber_options options;
  ps_subscriber_options_init(&options);
  options.preferred_transport = PUBSUB_TCP_TRANSPORT;// sets preferred transport to TCP
  options.cb = callback;
  ps_node_create_subscriber_adv(&node, "/data", &pubsub__String_def, &string_sub, &options);

  // Loop and spin
  while (ps_okay())
  {
    // Waits until we have a message or other event to respond to (optional)
    // Used to prevent this from using 100% CPU, but you can do that through other means
    ps_node_wait(&node, 1000/*maximum wait time in ms*/);

    // Updates the node, which will receive messages and call any callbacks as they come in
    ps_node_spin(&node);
  }

  // Shutdown the node to free resources
  ps_sub_destroy(&string_sub);// not necessary, destroyed below otherwise
  ps_node_destroy(&node);

  return 0;
}

