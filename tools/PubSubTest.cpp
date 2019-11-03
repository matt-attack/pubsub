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
ps_field_t std_msgs_String_fields[] = { { FT_String, "value", 1, 0 } };
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

/*ps_field_t joy_msgs_Joy_fields[] = { { FT_Float32, "axes", 4, 0 }, { FT_Int32, "buttons", 1, 0 } };
ps_message_definition_t joy_msgs_Joy_def = { 12345678910, "joy_msgs/Joy", 2, joy_msgs_Joy_fields };

struct joy_msgs_Joy
{
	float axes[4];
	int buttons;//bitmap of buttons
};

void joy_msgs_Joy_decode(const void* data, joy_msgs_Joy* out)
{
	*out = *(joy_msgs_Joy*)data;
}

ps_msg_t joy_msgs_Joy_encode(void* allocator, const joy_msgs_Joy* msg)
{
	int len = sizeof(joy_msgs_Joy);
	ps_msg_t omsg;
	ps_msg_alloc(len, &omsg);
	memcpy(ps_get_msg_start(omsg.data), msg, len);

	return omsg;
}*/

#include <stdint.h>

struct joy_msgs__Joy
{
	int32_t buttons;
	float axes[4];
};

ps_field_t joy_msgs__Joy_fields[] = {
	{ FT_Int32, "buttons", 1, 0 },
{ FT_Float32, "axes", 4, 0 },
};

ps_message_definition_t joy_msgs__Joy_def = { 123456789, "joy_msgs/Joy", 2, joy_msgs__Joy_fields };
void joy_msgs__Joy_decode(const void* data, joy_msgs__Joy* out)
{
	*out = *(joy_msgs__Joy*)data;
}

ps_msg_t joy_msgs__Joy_encode(void* allocator, const joy_msgs__Joy* msg)
{
	int len = sizeof(joy_msgs__Joy);
	ps_msg_t omsg;
	ps_msg_alloc(len, &omsg);
	memcpy(ps_get_msg_start(omsg.data), msg, len);
	return omsg;
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
	ps_node_init(&node, "pub", "", true);

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

	ps_pub_t adv_pub;
	ps_node_create_publisher(&node, "/joy", &joy_msgs__Joy_def, &adv_pub);

	ps_sub_t string_sub;
	ps_node_create_subscriber(&node, "/data", 0/*&std_msgs_String_def*/, &string_sub);

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

		joy_msgs__Joy jmsg;
		jmsg.axes[0] = 0.0;
		jmsg.axes[1] = 0.25;
		jmsg.axes[2] = 0.5;
		jmsg.axes[3] = 0.75;
		jmsg.buttons = 1234;

		ps_msg_t msg2 = joy_msgs__Joy_encode(0, &jmsg);
		ps_pub_publish(&adv_pub, &msg2);

		//ok, so lets add timeouts, make pubsub unsubscribe before it dies and figure out why the wires are getting crossed
		Sleep(10);

		ps_node_spin(&node);
		//while (ps_node_spin(&node) == 0) {}
		//todo now need to add decoding to the sub
		while (char* data = (char*)ps_sub_deque(&string_sub))
		{
			printf("Got message: %s\n", data);
			free(data);//todo use allocator free
		}

		printf("Num subs: %i %i\n", ps_pub_get_subscriber_count(&string_pub), ps_pub_get_subscriber_count(&adv_pub));
		Sleep(1000);
	}

	ps_sub_destroy(&string_sub);
	ps_pub_destroy(&string_pub);
	ps_node_destroy(&node);

    return 0;
}

