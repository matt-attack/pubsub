#include <cstdlib>
#include <stdio.h>

#include "../high_level_api/Node.h"
#include "../high_level_api/Logging.h"
#include "../high_level_api/Spinners.h"
#include "../src/System.h"

#include "../msg/std_msgs__String.msg.h"

int main()
{
	pubsub::Node node("simple_publisher", true);

	pubsub::Publisher<std_msgs::String> string_pub(node, "/data");

	pubsub::Logger logger(node);

	pubsub::Spinner spinner;
	spinner.addNode(node);

	int i = 0;
	while (ps_okay())
	{
		PUBSUB_INFO(logger, "Publishing...");
		std_msgs::String msg;
		char value[20];
		sprintf(value, "Hello %i", i++);
		msg.value = value;
		string_pub.publish(msg);
		
		// okay, since we are publishing with shared pointer we actually need to allocate the string properly
		auto shared = std_msgs::StringSharedPtr(new std_msgs::String);
		shared->value = new char[strlen(msg.value) + 1];
		strcpy(shared->value, msg.value);
		string_pub.publish(shared);

		msg.value = 0;// so it doesnt get freed by the destructor since we allocated it ourself

		ps_sleep(1000);
	}

    return 0;
}

