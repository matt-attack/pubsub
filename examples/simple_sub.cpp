#include <cstdlib>
#include <stdio.h>

#include "../high_level_api/Node.h"
#include "../high_level_api/Spinners.h"
#include <pubsub/System.h>

#include "../msg/std_msgs__String.msg.h"

int main()
{
	pubsub::Node node("simple_subscriber");

	pubsub::Subscriber<std_msgs::String> subscriber(node, "data", [](const std_msgs::StringSharedPtr& msg) {
		printf("Got message %s\n", msg->value);
	}, 10);

	pubsub::BlockingSpinner spinner;
	spinner.setNode(node);

	spinner.wait();

    return 0;
}

