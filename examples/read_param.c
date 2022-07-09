//#include <cstdlib>
#include <stdio.h>

#include <pubsub/Node.h>
#include <pubsub/Publisher.h>
#include <pubsub/Subscriber.h>
#include <pubsub/System.h>

#include <pubsub/TCPTransport.h>

#include <pubsub/String.msg.h>
#include <pubsub/Parameters.msg.h>

#include <math.h>

double callback(const char* name, double value)
{
	if (strcmp("hi", name) == 0)
	{
		return value;
	}
	if (strcmp("okay", name) == 0)
	{
		return value;
	}
	return nan("");
}

int main()
{
	struct ps_node_t node;
	ps_node_init(&node, "simple_publisher", "", false);
	
	node.param_cb = callback;

	struct ps_transport_t tcp_transport;
	ps_tcp_transport_init(&tcp_transport, &node);
	ps_node_add_transport(&node, &tcp_transport);

	struct ps_pub_t string_pub;
	ps_node_create_publisher(&node, "/data", &pubsub__String_def, &string_pub, true);
	
	struct ps_pub_t param_pub;
	ps_node_create_publisher(&node, "/parameters", &pubsub__Parameters_def, &param_pub, true);
	
	struct pubsub__Parameters pmsg;
	pmsg.names_length = 2;
	pmsg.names = (char**)malloc(sizeof(char*)*2);
	pmsg.names[0] = "hi";
	pmsg.names[1] = "okay";
	pmsg.value_length = 0;
	pmsg.min_length = 0;
	pmsg.max_length = 0;
	struct ps_msg_t emsg = pubsub__Parameters_encode(&ps_default_allocator, (void*)&pmsg);
	struct pubsub__Parameters* dmsg = (struct pubsub__Parameters*)pubsub__Parameters_decode(ps_get_msg_start(emsg.data), &ps_default_allocator);
	ps_pub_publish_ez(&param_pub, &pmsg);

	// user is responsible for lifetime of the message they publish
	struct pubsub__String rmsg;
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
		
		//ps_pub_publish_ez(&param_pub, &pmsg);

		printf("Num subs: %i\n", ps_pub_get_subscriber_count(&string_pub));
		ps_sleep(40);
	}

	ps_pub_destroy(&string_pub);
	ps_node_destroy(&node);

    return 0;
}

