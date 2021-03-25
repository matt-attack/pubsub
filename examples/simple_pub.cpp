#include <cstdlib>
#include <stdio.h>

#include "../high_level_api/Node.h"
#include "../high_level_api/Spinners.h"
#include <pubsub/System.h>

#include <pubsub/String.msg.h>
#include <pubsub/Image.msg.h>

int main()
{
	pubsub::Node node("simple_publisher");

	pubsub::Publisher<pubsub::msg::String> string_pub(node, "/data");

	pubsub::Publisher<pubsub::msg::Image> image_pub(node, "/image");

	pubsub::BlockingSpinnerWithTimers spinner;
	spinner.setNode(node);

	int i = 0;
	spinner.addTimer(0.3333, [&]()
	{
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

		auto img = pubsub::msg::ImageSharedPtr(new pubsub::msg::Image);
		int len = rand() % 10 + 2;
		img->width = 1;
		img->height = len;
		img->data = new uint8_t[len];
		img->data_length = len;
		for (int i = 0; i < len; i++)
		{
			img->data[i] = rand() % 10;
		}
		image_pub.publish(img);
	});

	spinner.wait();

    return 0;
}

