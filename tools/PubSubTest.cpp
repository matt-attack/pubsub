// PubSub.cpp : Defines the entry point for the console application.
//

#include <cstdlib>
#include <stdio.h>

#include "../src/Node.h"
#include "../src/Publisher.h"
#include "../src/Subscriber.h"

// simple pub sub library implementation in plain C, with lightweight version for embedded
// serialization is handled in a different api, this is just the protocol handling blobs

void ps_msg_alloc(unsigned int size, ps_msg_t* out_msg)
{
	out_msg->len = size;
	out_msg->data = (void*)((char*)malloc(size + sizeof(ps_msg_header)));
}

void* ps_get_msg_start(void* data)
{
	return (void*)((char*)data + sizeof(ps_msg_header));
}

#include <Windows.h>


int main()
{
	ps_node_t node;
	ps_node_init(&node, "pub", "192.168.0.104", false);

	ps_field_t field;
	field.type = FT_String;
	field.name = "data";
	field.content_length = field.length = 0;
	ps_message_definition_t def;
	def.name = "std_msgs/String";
	def.fields = &field;
	def.num_fields = 1;
	def.hash = 0;//todo do something with this

	ps_pub_t string_pub;
	ps_node_create_publisher(&node, "/data", &def, &string_pub);

	ps_sub_t string_sub;
	ps_node_create_subscriber(&node, "/data", "std_msgs/String", &string_sub);

	// wait until we get the subscription request
	while (ps_pub_get_subscriber_count(&string_pub) == 0) 
	{
		ps_node_spin(&node);
	}

	printf("Ok, stopping waiting as we got our subscriber\n");

	//ok, need to handle received messages now...

	ps_msg_t msg;
	ps_msg_alloc(6, &msg);

	while (true)
	{
		//need to make sure to reserve n bytes for the header
		memcpy(ps_get_msg_start(msg.data), "hello", 6);
		ps_pub_publish(&string_pub, &msg);

		Sleep(10);

		ps_node_spin(&node);
		//while (ps_node_spin(&node) == 0) {}

		char* data = (char*)ps_sub_deque(&string_sub);

		free(data);//todo use allocator free
		Sleep(1000);
	}

	ps_sub_destroy(&string_sub);
	ps_pub_destroy(&string_pub);
	//ps_node_destroy(&node);

    return 0;
}

