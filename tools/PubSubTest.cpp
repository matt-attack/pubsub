// PubSub.cpp : Defines the entry point for the console application.
//

#include <cstdlib>
#include <stdio.h>

#include <string.h>

#include "../src/Node.h"
#include "../src/Publisher.h"
#include "../src/Subscriber.h"
#include "../src/System.h"

void ps_msg_alloc(unsigned int size, ps_msg_t* out_msg)
{
	out_msg->len = size;
	out_msg->data = (void*)((char*)malloc(size + sizeof(ps_msg_header)));
}

#include "../msg/std_msgs__Joy.msg.h"
#include "../msg/std_msgs__String.msg.h"

// simple pub sub library implementation in plain C, with lightweight version for embedded
// serialization is handled in a different api, this is just the protocol handling blobs

/*void* ps_get_msg_start(void* data)
{
	return (void*)((char*)data + sizeof(ps_msg_header));
}*/


// generation test
/*ps_field_t std_msgs__String_fields[] = { { FT_String, "value", 1, 0 } };
struct std_msgs__String
{
	char* value;
};

// user needs to properly free this message with the allocator
void* std_msgs__String_decode(const void* data, ps_allocator_t* allocator)
{
	void* out = allocator->alloc(sizeof(std_msgs__String), allocator->context);
	std_msgs__String* msg = (std_msgs__String*)out;
	int len = strlen((const char*)data) + 1;
	msg->value = (char*)malloc(len);
	memcpy(msg->value, data, len);
	return out;
}

ps_msg_t std_msgs__String_encode(ps_allocator_t* allocator, const void* imsg)
{
	std_msgs__String* msg = (std_msgs__String*)imsg;
	int len = strlen(msg->value) + 1;
	ps_msg_t omsg;
	ps_msg_alloc(len, &omsg);
	memcpy(ps_get_msg_start(omsg.data), msg->value, len);
	return omsg;
}


ps_message_definition_t std_msgs__String_def = { 123456789, "std_msgs/String", 1, std_msgs__String_fields, std_msgs__String_encode, std_msgs__String_decode };*/

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

/*struct joy_msgs__Joy
{
	int32_t buttons;
	float axes[4];
};

ps_field_t joy_msgs__Joy_fields[] = {
	{ FT_Int32, "buttons", 1, 0 },
	{ FT_Float32, "axes", 4, 0 },
};

void* joy_msgs__Joy_decode(const void* data, ps_allocator_t* allocator)
{
	void* out = allocator->alloc(sizeof(joy_msgs__Joy), allocator->context);
	*(joy_msgs__Joy*)out = *(joy_msgs__Joy*)data;
	return out;
}

ps_msg_t joy_msgs__Joy_encode(ps_allocator_t* allocator, const void* msg)
{
	int len = sizeof(joy_msgs__Joy);
	ps_msg_t omsg;
	ps_msg_alloc(len, &omsg);
	memcpy(ps_get_msg_start(omsg.data), msg, len);
	return omsg;
}

ps_message_definition_t joy_msgs__Joy_def = { 123456789, "joy_msgs/Joy", 2, joy_msgs__Joy_fields, joy_msgs__Joy_encode };*/

/*struct joy_msgs__Joy
{
	int32_t buttons;
	float axes[4];
};

ps_field_t joy_msgs__Joy_fields[] = {
	{ FT_Int32, "buttons", 1, 0 },
{ FT_Float32, "axes", 4, 0 },
};

void* joy_msgs__Joy_decode(const void* data, ps_allocator_t* allocator)
{
	joy_msgs__Joy* out = (joy_msgs__Joy*)allocator->alloc(sizeof(joy_msgs__Joy), allocator->context);
	*out = *(joy_msgs__Joy*)data;
	return out;

}

ps_msg_t joy_msgs__Joy_encode(ps_allocator_t* allocator, const void* msg)
{
	int len = sizeof(joy_msgs__Joy);
	ps_msg_t omsg;
	ps_msg_alloc(len, &omsg);
	memcpy(ps_get_msg_start(omsg.data), msg, len);
	return omsg;
}
ps_message_definition_t joy_msgs__Joy_def = { 123456789, "joy_msgs/Joy", 2, joy_msgs__Joy_fields, joy_msgs__Joy_encode, joy_msgs__Joy_decode };
*/

int main()
{
	ps_node_t node;
	ps_node_init(&node, "pub", "", true);

	ps_pub_t string_pub;
	ps_node_create_publisher(&node, "/data", &std_msgs__String_def, &string_pub, true);

	ps_pub_t adv_pub;
	ps_node_create_publisher(&node, "/joy", &std_msgs__Joy_def, &adv_pub);

	ps_sub_t string_sub;
	ps_node_create_subscriber(&node, "/data", &std_msgs__String_def, &string_sub, 10);

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

