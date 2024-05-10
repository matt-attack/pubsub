
#include <pubsub_cpp/Node.h>
#include <pubsub_cpp/Spinners.h>

#include <pubsub/String.msg.h>

#include "mini_mock.hpp"

TEST(test_publish_subscribe_latched_cpp, []() {
	// test that latched topics make it through the local message passing
	pubsub::Node node("simple_publisher");

	pubsub::Publisher<pubsub::msg::String> string_pub(node, "/data", true);

	pubsub::msg::String omsg;
	omsg.value = "Hello";
	string_pub.publish(omsg);

	
	pubsub::BlockingSpinnerWithTimers spinner;
	spinner.setNode(node);

	bool got_message = false;
	pubsub::Subscriber<pubsub::msg::String> subscriber(node, "/data", [&](const pubsub::msg::StringSharedPtr& msg) {
		printf("Got message %s in sub1\n", msg->value);
		EXPECT(strcmp(omsg.value, msg->value) == 0);
		got_message = true;
		spinner.stop();
	}, 10);

	spinner.wait();
	EXPECT(got_message);
});

TEST(test_publish_subscribe_cpp, []() {
	// test that normal messages make it through message passing
	pubsub::Node node("simple_publisher");

	pubsub::Publisher<pubsub::msg::String> string_pub(node, "/data");

	pubsub::msg::String omsg;
	omsg.value = "Hello";
	
	pubsub::BlockingSpinnerWithTimers spinner;
	spinner.setNode(node);

	bool got_message = false;
	pubsub::Subscriber<pubsub::msg::String> subscriber(node, "/data", [&](const pubsub::msg::StringSharedPtr& msg) {
		printf("Got message %s in sub1\n", msg->value);
		EXPECT(strcmp(omsg.value, msg->value) == 0);
		spinner.stop();
		got_message = true;
	}, 10);

	spinner.addTimer(0.1, [&]()
	{
		string_pub.publish(omsg);
	});

	spinner.wait();
	EXPECT(got_message);
});

CREATE_MAIN_ENTRY_POINT();
