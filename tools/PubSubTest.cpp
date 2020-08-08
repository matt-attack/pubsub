
#include <cstdlib>
#include <stdio.h>

#include <pubsub/Node.h>
#include <pubsub/Publisher.h>
#include <pubsub/Subscriber.h>
#include <pubsub/System.h>


#include "../msg/std_msgs__Joy.msg.h"
#include "../msg/std_msgs__String.msg.h"

int main()
{
	ps_node_t node;
	ps_node_init(&node, "pub", "", true);

	ps_pub_t string_pub;
	ps_node_create_publisher(&node, "/data", &std_msgs__String_def, &string_pub, true);

	ps_pub_t adv_pub;
	ps_node_create_publisher(&node, "/joy", &std_msgs__Joy_def, &adv_pub, false);

	ps_sub_t string_sub;
	ps_node_create_subscriber(&node, "/data", &std_msgs__String_def, &string_sub, 10, false, 0, false);

	// wait until we get the subscription request
	while (ps_pub_get_subscriber_count(&string_pub) == 0) 
	{
		ps_node_spin(&node);
	}

	printf("Ok, stopping waiting as we got our subscriber\n");

	// user is responsible for lifetime of the message they publish
	std_msgs__String rmsg;
	rmsg.value = "Hello";
	//where does the allocation happen? probably should be in teh encode function which should return the ps_msg_t
	//ps_msg_t msg = std_msgs__String_encode(&rmsg);
	//ps_pub_publish(&string_pub, &msg);
	ps_pub_publish_ez(&string_pub, &rmsg);

	while (ps_okay())
	{
		rmsg.value = "Woah 2";
		ps_pub_publish_ez(&string_pub, &rmsg);

		std_msgs__Joy jmsg;
		jmsg.axes[0] = 0.0;
		jmsg.axes[1] = 0.25;
		jmsg.axes[2] = 0.5;
		jmsg.axes[3] = 0.75;
		jmsg.buttons = 1234;

		ps_pub_publish_ez(&adv_pub, &jmsg);

		//ok, so lets add timeouts, make pubsub unsubscribe before it dies and figure out why the wires are getting crossed
		ps_sleep(10);

		ps_node_spin(&node);
		//while (ps_node_spin(&node) == 0) { Sleep(1); }

		// our sub has a message definition, so the queue contains real messages
		while (std_msgs__String* data = (std_msgs__String*)ps_sub_deque(&string_sub))
		{
			printf("Got message: %s\n", data->value);
			free(data->value);
			free(data);//todo use allocator free
		}

		printf("Num subs: %i %i\n", ps_pub_get_subscriber_count(&string_pub), ps_pub_get_subscriber_count(&adv_pub));
		ps_sleep(1000);
	}

	ps_sub_destroy(&string_sub);
	ps_pub_destroy(&string_pub);
	ps_node_destroy(&node);

    return 0;
}

