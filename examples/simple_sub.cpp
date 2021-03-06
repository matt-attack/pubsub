#include <cstdlib>
#include <stdio.h>

#include <pubsub_cpp/Node.h>
#include <pubsub_cpp/Spinners.h>
#include <pubsub/System.h>

#include <pubsub/String.msg.h>

int main()
{
	pubsub::Node node("simple_subscriber");

	pubsub::Subscriber<pubsub::msg::String> subscriber(node, "data", [](const pubsub::msg::StringSharedPtr& msg) {
		printf("Got message %s\n", msg->value);
	}, 10);

	pubsub::BlockingSpinner spinner;
	spinner.setNode(node);

	spinner.wait();

    return 0;
}

