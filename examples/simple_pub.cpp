#include <cstdlib>
#include <stdio.h>

#include "../high_level_api/Node.h"
#include "../src/System.h"

#include "../msg/std_msgs__String.msg.h"

int main()
{
	pubsub::Node node("simple_publisher", true);

	pubsub::Publisher<std_msgs::String> string_pub(node, "/data");

	int i = 0;
	while (ps_okay())
	{
		std_msgs::String msg;
		char value[20];
		sprintf(value, "Hello %i", i++);
		msg.value = value;
		string_pub.publish(msg);

		node.spin();

		ps_sleep(333);
	}

    return 0;
}

