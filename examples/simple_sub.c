//#include <cstdlib>
#include <stdio.h>

#include <pubsub/Node.h>
#include <pubsub/Publisher.h>
#include <pubsub/Subscriber.h>
#include <pubsub/System.h>
#include <pubsub/TCPTransport.h>

#include <pubsub/String.msg.h>

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
  struct ps_sub_t string_sub;
  struct ps_subscriber_options options;
  ps_subscriber_options_init(&options);
  options.preferred_transport = PUBSUB_TCP_TRANSPORT;// sets preferred transport to TCP
  ps_node_create_subscriber_adv(&node, "/data", &pubsub__String_def, &string_sub, &options);

  // Loop and spin
  while (ps_okay())
  {
    // Waits until we have a message or other event to respond to (optional)
    // Used to prevent this from using 100% CPU, but you can do that through other means
    ps_node_wait(&node, 1000/*maximum wait time in ms*/);

    // Updates the node, which will queue up any received messages
    ps_node_spin(&node);

    // our sub has a message definition, so the queue contains real messages
    struct pubsub__String* data;
    while (data = (struct pubsub__String*)ps_sub_deque(&string_sub))
    {
      // user is responsible for freeing the message and its arrays
      printf("Got message: %s\n", data->value);
      free(data->value);
      free(data);
    }
  }

  // Shutdown the node to free resources
  ps_sub_destroy(&string_sub);// not necessary, destroyed below otherwise
  ps_node_destroy(&node);

  return 0;
}

