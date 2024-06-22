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
  ps_node_init(&node, "simple_publisher"/*node name*/, 
               NULL/*IP to bind to, empty string or null to autodetect*/,
               false/*set true to use broadcast instead of multicast advertising*/);

  // Adds TCP transport (optional)
  struct ps_transport_t tcp_transport;
  ps_tcp_transport_init(&tcp_transport, &node);
  ps_node_add_transport(&node, &tcp_transport);

  // Create the publisher
  struct ps_pub_t string_pub;
  ps_node_create_publisher(&node, "/data"/*topic name*/,
                           &pubsub__String_def/*message definition*/,
                           &string_pub,
                           true/*true to "latch" the topic*/);

  // User is responsible for lifetime of the message they publish
  // Publish does a copy internally if necessary
  struct pubsub__String rmsg;
  rmsg.value = "Hello";
  ps_pub_publish_ez(&string_pub, &rmsg);

  int i = 0;
  while (ps_okay())
  {
    // Build and publish the message
    char value[20];
    sprintf(value, "Hello %i", i++);
    rmsg.value = value;
    ps_pub_publish_ez(&string_pub, &rmsg);

    printf("Num subs: %i\n", ps_pub_get_subscriber_count(&string_pub));

    // Waits until we have a message or other event to respond to (optional)
    // Used to prevent this from using 100% CPU, but you can do that through other means
    ps_node_wait(&node, 333/*maximum wait time in ms*/);

    // Updates the node, which will queue up any received messages
    ps_node_spin(&node);
  }

  // Shutdown the node to free resources
  ps_pub_destroy(&string_pub);// not necessary, destroyed below otherwise
  ps_node_destroy(&node);

  return 0;
}

