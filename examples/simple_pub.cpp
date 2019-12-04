#include <cstdlib>
#include <stdio.h>

#include "../src/Node.h"
#include "../src/Publisher.h"
#include "../src/Subscriber.h"
#include "../src/System.h"

#include "../msg/std_msgs__String.msg.h"

int main()
{
	ps_node_t node;
	ps_node_init(&node, "simple_publisher", "", true);

	ps_pub_t string_pub;
	ps_node_create_publisher(&node, "/data", &std_msgs__String_def, &string_pub, true);

	// user is responsible for lifetime of the message they publish
	std_msgs__String rmsg;
	rmsg.value = "Hello";
	//ps_msg_t msg = std_msgs__String_encode(&rmsg);
	//ps_pub_publish(&string_pub, &msg);
	ps_pub_publish_ez(&string_pub, &rmsg);

	while (ps_okay())
	{
		ps_pub_publish_ez(&string_pub, &rmsg);

		ps_node_spin(&node);

		printf("Num subs: %i\n", ps_pub_get_subscriber_count(&string_pub));
		ps_sleep(333);
	}

	ps_pub_destroy(&string_pub);
	ps_node_destroy(&node);

    return 0;
}

