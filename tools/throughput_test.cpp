#include <cstdlib>
#include <stdio.h>

#include "../high_level_api/Node.h"
#include "../high_level_api/Spinners.h"
#include <pubsub/System.h>

#include <pubsub/String.msg.h>

int main()
{
	pubsub::Node node("simple_publisher", true);

	pubsub::Publisher<pubsub::msg::String> string_pub(node, "/data");

	pubsub::Publisher<pubsub::msg::String> string_pub2(node, "/data_copy");

	/*pubsub::Subscriber<std_msgs::String> subscriber(node, "/data", [&](const std_msgs::StringSharedPtr& msg) {
		//printf("Got message %s\n", msg->value);
		//string_pub2.publish(msg);
	}, 10);*/

	pubsub::Spinner spinner;
	spinner.addNode(node);

	pubsub::msg::String msg;
	char value[20];
	int i = 0;
	sprintf(value, "Hello %i", i++);
	msg.value = value;

	// okay, since we are publishing with shared pointer we actually need to allocate the string properly
	auto shared = pubsub::msg::StringSharedPtr(new pubsub::msg::String);
	shared->value = new char[strlen(msg.value) + 1];
	strcpy(shared->value, msg.value);

	while (ps_okay())
	{
		string_pub.publish(shared);
	}

    return 0;
}

