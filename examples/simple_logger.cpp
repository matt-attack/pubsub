#include <cstdlib>
#include <stdio.h>

#include <pubsub_cpp/Node.h>
#include <pubsub_cpp/Logging.h>
#include <pubsub_cpp/Spinners.h>
#include <pubsub/System.h>

#include <pubsub/String.msg.h>

int main()
{
	pubsub::Node node("simple_publisher", true);

	pubsub::Publisher<pubsub::msg::String> string_pub(node, "/data");

	pubsub::Logger logger(node);

	pubsub::Spinner spinner;
	spinner.addNode(node);
	spinner.start();

	int i = 0;
	while (ps_okay())
	{
		PUBSUB_INFO(logger, "Publishing...");
		pubsub::msg::String msg;
		char value[20];
		sprintf(value, "Hello %i", i++);
		msg.value = value;
		string_pub.publish(msg);
		
		// okay, since we are publishing with shared pointer we actually need to allocate the string properly
		auto shared = pubsub::msg::StringSharedPtr(new pubsub::msg::String);
		shared->value = new char[strlen(msg.value) + 1];
		strcpy(shared->value, msg.value);
		string_pub.publish(shared);

		msg.value = 0;// so it doesnt get freed by the destructor since we allocated it ourself

		ps_sleep(1000);
	}

    return 0;
}

