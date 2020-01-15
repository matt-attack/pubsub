//#include <cstdlib>
#include <stdio.h>

#include <pubsub/Node.h>
#include <pubsub/Publisher.h>
#include <pubsub/Subscriber.h>
#include <pubsub/System.h>

#include "../msg/std_msgs__String.msg.h"

int main()
{
	struct ps_node_t node;
	ps_node_init(&node, "simple_subscriber", "", true);

	struct ps_sub_t string_sub;
	ps_node_create_subscriber(&node, "/data", &std_msgs__String_def, &string_sub, 10, false, 0, true);

	while (ps_okay())
	{
		ps_sleep(1);

		ps_node_spin(&node);

		// our sub has a message definition, so the queue contains real messages
		struct std_msgs__String* data;
		while (data = (struct std_msgs__String*)ps_sub_deque(&string_sub))
		{
			// user is responsible for freeing the message and its arrays
			printf("Got message: %s\n", data->value);
			free(data->value);
			free(data);//todo use allocator free
		}
	}

	ps_sub_destroy(&string_sub);
	ps_node_destroy(&node);

    return 0;
}

