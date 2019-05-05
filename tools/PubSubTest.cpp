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

// generation test
ps_field_t std_msgs_String_fields[] = { { FT_String, "value", 0, 0 } };
ps_message_definition_t std_msgs_String_def = { 123456789/*hash*/, "std_msgs/String", 1, std_msgs_String_fields};
struct std_msgs_String
{
	char* value;
};

// todo, this might have a data lifetime issue
void std_msgs_String_decode(const void* data, std_msgs_String* out)
{
	out->value = (char*)data;
}

ps_msg_t std_msgs_String_encode(const std_msgs_String* msg)
{
	int len = strlen(msg->value) + 1;
	ps_msg_t omsg;
	ps_msg_alloc(len, &omsg);
	memcpy(ps_get_msg_start(omsg.data), msg->value, len);
	return omsg;
}

struct joy_msgs_Joy
{
	float axes[4];
	int buttons;//bitmap of buttons
};

void joy_msgs_Joy_decode(const void* data, joy_msgs_Joy* msg)
{

}

void* joy_msgs_Joy_encode(void* allocator, const joy_msgs_Joy* msg)
{
	return 0;
}

struct std_msgs_Int32
{
	int value;
};

void std_msgs_Int32_decode()
{

}

void std_msgs_Int32_encode()
{

}

struct std_msgs_Float32
{
	float value;
};

int main()
{
	ps_node_t node;
	ps_node_init(&node, "pub", "192.168.0.102", true);

	/*ps_field_t field;
	field.type = FT_String;
	field.name = "data";
	field.content_length = field.length = 0;
	ps_message_definition_t def;
	def.name = "std_msgs/String";
	def.fields = &field;
	def.num_fields = 1;
	def.hash = 0;//todo do something with this*/

	ps_pub_t string_pub;
	ps_node_create_publisher(&node, "/data", &std_msgs_String_def, &string_pub);

	ps_sub_t string_sub;
	ps_node_create_subscriber(&node, "/data", "std_msgs/String", &string_sub);

	// wait until we get the subscription request
	while (ps_pub_get_subscriber_count(&string_pub) == 0) 
	{
		ps_node_spin(&node);
	}

	printf("Ok, stopping waiting as we got our subscriber\n");

	//ok, need to handle received messages now...

	//ps_msg_t msg;
	//ps_msg_alloc(6, &msg);

	while (true)
	{
		//need to make sure to reserve n bytes for the header
		std_msgs_String rmsg;
		rmsg.value = "Hello";
		//where does the allocation happen? probably should be in teh encode function which should return the ps_msg_t
		ps_msg_t msg = std_msgs_String_encode(&rmsg);
		ps_pub_publish(&string_pub, &msg);

		Sleep(10);

		ps_node_spin(&node);
		//while (ps_node_spin(&node) == 0) {}
		todo now need to add decoding to the sub
		char* data = (char*)ps_sub_deque(&string_sub);
		printf("Got message: %s\n", data);
		free(data);//todo use allocator free
		Sleep(1000);
	}

	ps_sub_destroy(&string_sub);
	ps_pub_destroy(&string_pub);
	//ps_node_destroy(&node);

    return 0;
}

