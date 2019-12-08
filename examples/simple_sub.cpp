#include <cstdlib>
#include <stdio.h>

#include "../high_level_api/Node.h"
#include "../src/System.h"

#include "../msg/std_msgs__String.msg.h"

int main()
{
	pubsub::Node node("simple_subscriber");

	pubsub::Subscriber<std_msgs::String> subscriber(node, "/data", [](std_msgs::String* msg) {
		printf("Got message %s\n", msg->value);
		free(msg);
	});

	pubsub::Spinner spinner;
	spinner.addNode(node);

	spinner.wait();

    return 0;
}

