
#include <pubsub_cpp/Node.h>
#include <pubsub_cpp/Spinners.h>

#include <pubsub/Int.msg.h>
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
		
		// make sure subscriber count is correct
		EXPECT(string_pub.getNumSubscribers() == 1);
	}, 10);

	spinner.run();
	EXPECT(got_message);
});

TEST(test_publish_subscribe_zero_copy, []() {
	// test that data gets passed through without copying within a single node
	pubsub::Node node("simple_publisher");

	pubsub::Publisher<pubsub::msg::String> string_pub(node, "/data", true);

	pubsub::msg::StringSharedPtr omsg(new pubsub::msg::String());
	omsg->value = "Hello";
	string_pub.publish(omsg);
	
	pubsub::BlockingSpinnerWithTimers spinner;
	spinner.setNode(node);

	bool got_message = false;
	pubsub::Subscriber<pubsub::msg::String> subscriber(node, "/data", [&](const pubsub::msg::StringSharedPtr& msg) {
		printf("Got message %s in sub1\n", msg->value);
		EXPECT(strcmp(omsg->value, msg->value) == 0);
		got_message = true;
		EXPECT(msg.get() == omsg.get());
		spinner.stop();
		
		// make sure subscriber count is correct
		EXPECT(string_pub.getNumSubscribers() == 1);
	}, 10);

	spinner.run();
	EXPECT(got_message);
});

TEST(test_publish_subscribe_queue_behavior, []() {
	// test that the queue drops messages as expected and we only get the newest
	pubsub::Node node("simple_publisher");

	pubsub::Publisher<pubsub::msg::Int> int_pub(node, "/data");
	
	pubsub::BlockingSpinnerWithTimers spinner;
	spinner.setNode(node);

	std::vector<int> received;
	pubsub::Subscriber<pubsub::msg::Int> subscriber(node, "/data", [&](const pubsub::msg::IntSharedPtr& msg) {
		printf("Got message %i in sub1\n", msg->value);
		received.push_back(msg->value);
		spinner.stop();
	}, 10);
	
	for (int i = 0; i < 100; i++)
	{
	  pubsub::msg::Int omsg;
	  omsg.value = i;
	  int_pub.publish(omsg);
	}

  // spin after publishing so the queue fills up
	spinner.run();
	EXPECT(received.size() == 10);
	for (int i = 0; i < 10; i++)
	{
	  EXPECT(received[i] == 90 + i);
	}
});

TEST(test_publish_subscribe_nodelets, []() {
	// test that data gets passed through without copying between multiple nodes
	pubsub::Node nodep("simple_publisher");
	pubsub::Node nodes("simple_subscriber");

	pubsub::Publisher<pubsub::msg::String> string_pub(nodep, "/data", true);

	pubsub::msg::StringSharedPtr omsg(new pubsub::msg::String());
	omsg->value = "Hello";
	string_pub.publish(omsg);
	
	pubsub::BlockingSpinnerWithTimers spinner;
	spinner.setNode(nodep);
	
	pubsub::BlockingSpinnerWithTimers spinner2;
	spinner2.setNode(nodes);

	bool got_message = false;
	pubsub::Subscriber<pubsub::msg::String> subscriber(nodes, "/data", [&](const pubsub::msg::StringSharedPtr& msg) {
		printf("Got message %s in sub1\n", msg->value);
		EXPECT(strcmp(omsg->value, msg->value) == 0);
		got_message = true;
		EXPECT(msg.get() == omsg.get());
		spinner2.stop();
		spinner.stop();
	}, 10);

  spinner2.start();
	spinner.run();
	spinner2.wait();
	EXPECT(got_message);
});

// Make sure close works on publishers/subscribers and doesnt result in them getting closed multiple times
TEST(test_publisher_subscriber_close_cpp, []() {
	pubsub::Node node("simple_publisher");

	pubsub::Publisher<pubsub::msg::String> pub(node, "/data", true);
	pubsub::Subscriber<pubsub::msg::String> sub(node, "/data", [&](const pubsub::msg::StringSharedPtr& msg) {});

	pub.close();
	pub.close();

	sub.close();
	sub.close();
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

	spinner.run();
	EXPECT(got_message);
});

CREATE_MAIN_ENTRY_POINT();
