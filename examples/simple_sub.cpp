#include <cstdlib>
#include <stdio.h>

#include "../src/Node.h"
#include "../src/Publisher.h"
#include "../src/Subscriber.h"


#include <Windows.h>

void ps_msg_alloc(unsigned int size, ps_msg_t* out_msg)
{
	out_msg->len = size;
	out_msg->data = (void*)((char*)malloc(size + sizeof(ps_msg_header)));
}

#include "../msg/std_msgs__String.msg.h"

int main()
{
	ps_node_t node;
	ps_node_init(&node, "simple_subscriber", "", true);

	ps_sub_t string_sub;
	ps_node_create_subscriber(&node, "/data", &std_msgs__String_def, &string_sub, 10);

	while (ps_okay())
	{
		Sleep(1);

		ps_node_spin(&node);

		// our sub has a message definition, so the queue contains real messages
		while (std_msgs__String* data = (std_msgs__String*)ps_sub_deque(&string_sub))
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

