
#include <pubsub/Node.h>
#include <pubsub/Publisher.h>
#include <pubsub/Subscriber.h>
#include <pubsub/System.h>
#include <pubsub/TCPTransport.h>

#include <pubsub/String.msg.h>

#include "mini_mock.hpp"

TEST(test_publish_subscribe_latched, []() {
	struct ps_node_t node;
	ps_node_init(&node, "simple_publisher", "", false);

    struct ps_transport_t tcp_transport;
    ps_tcp_transport_init(&tcp_transport, &node);
    ps_node_add_transport(&node, &tcp_transport);

	struct ps_pub_t string_pub;
	ps_node_create_publisher(&node, "/data", &pubsub__String_def, &string_pub, true);

	struct ps_sub_t string_sub;

    struct ps_subscriber_options options;
    ps_subscriber_options_init(&options);
    //options.preferred_transport = 1;// tcp yo
    ps_node_create_subscriber_adv(&node, "/data", &pubsub__String_def, &string_sub, &options);

	// come up with the latched topic
	struct pubsub__String rmsg;
	rmsg.value = "Hello";
	ps_pub_publish_ez(&string_pub, &rmsg);

    // now spin and wait for us to get the published message
	int i = 0;
	while (ps_okay())
	{
		ps_node_spin(&node);// todo blocking wait first

		struct pubsub__String* data;
		while (data = (struct pubsub__String*)ps_sub_deque(&string_sub))
		{
			// user is responsible for freeing the message and its arrays
			printf("Got message: %s\n", data->value);
			EXPECT(strcmp(data->value, rmsg.value) == 0);
			free(data->value);
			free(data);//todo use allocator free
			goto done;
		}
		ps_sleep(10);
	}

done:
	ps_pub_destroy(&string_pub);
	ps_node_destroy(&node);
});

CREATE_MAIN_ENTRY_POINT();
