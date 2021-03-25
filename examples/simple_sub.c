//#include <cstdlib>
#include <stdio.h>

#include <pubsub/Node.h>
#include <pubsub/Publisher.h>
#include <pubsub/Subscriber.h>
#include <pubsub/System.h>
#include <pubsub/TCPTransport.h>

#include <pubsub/String.msg.h>

int main()
{
	struct ps_node_t node;
	ps_node_init(&node, "simple_subscriber", "", false);

    // Adds TCP transport
    struct ps_transport_t tcp_transport;
    ps_tcp_transport_init(&tcp_transport, &node);
    ps_node_add_transport(&node, &tcp_transport);

	struct ps_sub_t string_sub;

    struct ps_subscriber_options options;
    ps_subscriber_options_init(&options);
    options.preferred_transport = 1;// tcp yo
    ps_node_create_subscriber_adv(&node, "/data", &pubsub__String_def, &string_sub, &options);

	//ps_node_create_subscriber(&node, "/data", &std_msgs__String_def, &string_sub, 10, false, 0, true);

	while (ps_okay())
	{
		ps_sleep(1);

		ps_node_spin(&node);

		// our sub has a message definition, so the queue contains real messages
		struct pubsub__String* data;
		while (data = (struct pubsub__String*)ps_sub_deque(&string_sub))
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

