#include <cstdlib>
#include <stdio.h>
#include <cmath>

#include <pubsub_cpp/Node.h>
#include <pubsub_cpp/Spinners.h>
#include <pubsub/System.h>

#include <pubsub/String.msg.h>

#include <pubsub/TCPTransport.h>

int main()
{
  // Create the node
  pubsub::Node node("simple_publisher"/*node name*/);

  // Adds TCP transport (optional)
  struct ps_transport_t tcp_transport;
  ps_tcp_transport_init(&tcp_transport, node.getNode());
  ps_node_add_transport(node.getNode(), &tcp_transport);

  // Create the publisher
  pubsub::Publisher<pubsub::msg::String> string_pub(node, "/data"/*topic name*/,
                                                    false/*true to "latch" the topic*/);

  // Create the "spinner" which executes callbacks and timers in a background thread
  pubsub::BlockingSpinnerWithTimers spinner;
  spinner.setNode(node);// Add the node to the spinner

  auto start = pubsub::Time::now();// Gets the current time

  // Create a timer which will run at a prescribed interval
  spinner.addTimer(1.0/*timer is run every this many seconds*/, [&]()
  {
    auto now = pubsub::Time::now();

	// Build and publish the message
    pubsub::msg::String msg;
    char value[20];
    sprintf(value, "Hello %f", (now-start).toSec());
    msg.value = value;
    string_pub.publish(msg);
  });

  // Wait for the spinner to exit (on control-c)
  spinner.wait();

  return 0;
}

