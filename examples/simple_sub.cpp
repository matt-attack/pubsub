#include <cstdlib>
#include <stdio.h>

#include <pubsub_cpp/Node.h>
#include <pubsub_cpp/Spinners.h>
#include <pubsub/System.h>

#include <pubsub/String.msg.h>

int main()
{
  // Create the node
  pubsub::Node node("simple_subscriber"/*node name*/);

  // Create the subscriber, the provided callback will be called each time a message comes in
  pubsub::Subscriber<pubsub::msg::String> subscriber(node, "data"/*topic name*/,
    [](const pubsub::msg::StringSharedPtr& msg) {
      printf("Got message %s\n", msg->value);
    }, 10/*maximum queue size, after this many messages build up the oldest will get dropped*/);

  // Create the "spinner" which executes callbacks and timers in a background thread
  pubsub::BlockingSpinner spinner;
  spinner.setNode(node);// Add the node to the spinner

  // Wait for the spinner to exit (on control-c)
  spinner.wait();

  return 0;
}

