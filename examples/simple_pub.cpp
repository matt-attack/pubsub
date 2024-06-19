#include <cstdlib>
#include <stdio.h>
#include <cmath>

#include <pubsub_cpp/Node.h>
#include <pubsub_cpp/Spinners.h>
#include <pubsub/System.h>

#include <pubsub/String.msg.h>

#include <pubsub/TCPTransport.h>

int main()
{
	pubsub::Node node("simple_publisher");
	
	struct ps_transport_t tcp_transport;
	ps_tcp_transport_init(&tcp_transport, node.getNode());
	ps_node_add_transport(node.getNode(), &tcp_transport);

	pubsub::Publisher<pubsub::msg::String> string_pub(node, "/data");

	pubsub::BlockingSpinnerWithTimers spinner;
	spinner.setNode(node);

	auto start = pubsub::Time::now();
	spinner.addTimer(1.0, [&]()
	{
		auto now = pubsub::Time::now();
		pubsub::msg::String msg;
		char value[20];
		sprintf(value, "Hello %f", (now-start).toSec());
		msg.value = value;
		string_pub.publish(msg);
	});

	spinner.wait();

    return 0;
}

