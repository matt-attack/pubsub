
#include "pubsub/TCPTransport.h"
#include "pubsub/Bindings.h"
#include "pubsub/Node.h"

#include "pubsub/Costmap.msg.h"
#include "pubsub/Marker.msg.h"

static bool node_initialized[10] = { 0 };
static struct ps_node_t nodes[10];
static struct ps_transport_t tcp_transports[10];
static bool pub_initialized[20] = { 0 };
static struct ps_pub_t publishers[20];

EXPORT int ps_create_node(const char* name, const char* ip, bool broadcast)
{
	// find a spare node, return -1 if none was found
	int free_node = -1;
	for (int i = 0; i < 10; i++)
	{
		if (node_initialized[i] == 0)
		{
			free_node = i;
			break;
		}
	}

	if (free_node < 0)
	{
		return -1;
	}

	node_initialized[free_node] = 1;
	ps_node_init_ex(&nodes[free_node], name, ip, broadcast, false);

	ps_tcp_transport_init(&tcp_transports[free_node], &nodes[free_node]);
	ps_node_add_transport(&nodes[free_node], &tcp_transports[free_node]);

	return free_node;
}

EXPORT void ps_destroy_node(int node)
{
	if (node >= 10 || node < 0)
	{
		return;
	}

	if (node_initialized[node])
	{
		ps_node_destroy(&nodes[node]);
		node_initialized[node] = 0;
	}
}

EXPORT void ps_spin_node(int node)
{
	if (node_initialized[node])
	{
		ps_node_spin(&nodes[node]);
	}
}

EXPORT int ps_create_publisher(int node, const char* topic, const char* definition, bool latched)
{
	// find a spare node, return -1 if none was found
	int free_pub = -1;
	for (int i = 0; i < 10; i++)
	{
		if (pub_initialized[i] == 0)
		{
			free_pub = i;
			break;
		}
	}

	if (free_pub < 0)
	{
		return -1;
	}

	// generate a definition from the definition string given
	struct ps_msg_field_t pubsub__Image_fields[] = {
		{ FT_Int32, FF_NONE, "x", 1, 0 },
	};

	// note it has no encode/decode functions
	struct ps_message_definition_t pubsub__Image_def = { 3803012856, "pubsub__Int32", 1, pubsub__Image_fields, 0, 0, 0, 0 };

	struct ps_message_definition_t* cpy = (struct ps_message_definition_t*)malloc(sizeof(struct ps_message_definition_t));
	if (strcmp(definition, "marker") == 0)
	{
		ps_copy_message_definition(cpy, &pubsub__Marker_def);
	}
	else if (strcmp(definition, "costmap") == 0)
	{
		ps_copy_message_definition(cpy, &pubsub__Costmap_def);
	}
	else
	{
		ps_copy_message_definition(cpy, &pubsub__Image_def);
	}

	ps_node_create_publisher(&nodes[node], strdup(topic), cpy, &publishers[free_pub], latched);
	pub_initialized[free_pub] = 1;
	return free_pub;// todo support other pubs
}

EXPORT void ps_publish(int pub, const void* msg, int len)
{
	struct ps_msg_t omsg;
	ps_msg_alloc(len, 0, &omsg);
	memcpy(ps_get_msg_start(omsg.data), msg, len);
	ps_pub_publish(&publishers[pub], &omsg);
}

EXPORT bool ps_is_okay()
{
	return ps_okay();
}

EXPORT void ps_set_parameter_cb(int node, void* cb)
{
	if (node >= 10 || node < 0)
	{
		return;
	}

	if (node_initialized[node])
	{
		nodes[node].param_cb = cb;
	}
}