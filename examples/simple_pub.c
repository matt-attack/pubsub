//#include <cstdlib>
#include <stdio.h>

#include "../src/Node.h"
#include "../src/Publisher.h"
#include "../src/Subscriber.h"
#include "../src/System.h"

#include "../msg/std_msgs__String.msg.h"

int main()
{
	struct ps_node_t node;
	ps_node_init(&node, "simple_publisher", "", true);

	struct ps_pub_t string_pub;
	ps_node_create_publisher(&node, "/data", &std_msgs__String_def, &string_pub, true);

	// user is responsible for lifetime of the message they publish
	struct std_msgs__String rmsg;
	rmsg.value = "Hello";
	//ps_msg_t msg = std_msgs__String_encode(&rmsg);
	//ps_pub_publish(&string_pub, &msg);
	ps_pub_publish_ez(&string_pub, &rmsg);

	int i = 0;
	while (ps_okay())
	{
		char value[20];
		sprintf(value, "Hello %i", i++);
		rmsg.value = value;
		ps_pub_publish_ez(&string_pub, &rmsg);

		ps_node_spin(&node);

		printf("Num subs: %i\n", ps_pub_get_subscriber_count(&string_pub));
		ps_sleep(333);
	}

	ps_pub_destroy(&string_pub);
	ps_node_destroy(&node);

    return 0;
}
