#include <string>

#include <pubsub_cpp/Node.h>
#include <pubsub_cpp/Spinners.h>

#include <pubsub/TCPTransport.h>

int main()
{
  // Create the node
  pubsub::Node node("simple_parameters"/*node name*/);

  // Adds TCP transport (optional)
  struct ps_transport_t tcp_transport;
  ps_tcp_transport_init(&tcp_transport, node.getNode());
  ps_node_add_transport(node.getNode(), &tcp_transport);
                                                    
  // Create the "spinner" which executes callbacks and timers in a background thread
  pubsub::BlockingSpinnerWithTimers spinner;
  spinner.setNode(node);// Add the node to the spinner

  auto start = pubsub::Time::now();// Gets the current time
  
  auto parameter = node.parameter("test_1", 1.0, "Description.");
  auto parameter2 = node.parameter("test_2", 3.0, "Description 2.");

  // Create a timer which will run at a prescribed interval
  spinner.addTimer(1.0/*timer is run every this many seconds*/, [&]()
  {
    printf("Value: %f %f\n", (float)(double)parameter, (float)(double)parameter2);
  });

  // Wait for the spinner to exit (on control-c)
  spinner.run();

  return 0;
}

