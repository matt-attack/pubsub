
#include "pubsub/TCPTransport.h"
#include "pubsub/Bindings.h"
#include "pubsub/Node.h"

#include "pubsub/Costmap.msg.h"
#include "pubsub/Marker.msg.h"
#include "pubsub/Pose.msg.h"
#include "pubsub/Path.msg.h"

#if defined(_WIN32) || defined(_WIN64)
/* We are on Windows */
# define strtok_r strtok_s
#endif

static bool node_initialized[10] = { 0 };
static struct ps_node_t nodes[10];
static struct ps_transport_t tcp_transports[10];
static bool pub_initialized[20] = { 0 };
static struct ps_pub_t publishers[20];

#if defined(_WIN32) || defined(_WIN64)
/* We are on Windows */
# define strtok_r strtok_s
#endif

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
    const char* name_copy = strdup(name);// copy since we dont control the lifetime
	ps_node_init_ex(&nodes[free_node], name_copy, ip, broadcast, false);

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
        char* name_str = (char*)nodes[node].name;
		ps_node_destroy(&nodes[node]);
		node_initialized[node] = 0;
        free(name_str);
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
	// find a spare pub, return -1 if none was found
	int free_pub = -1;
	for (int i = 0; i < 20; i++)
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
	

	struct ps_message_definition_t* cpy = (struct ps_message_definition_t*)malloc(sizeof(struct ps_message_definition_t));
	if (strcmp(definition, "marker") == 0)
	{
		ps_copy_message_definition(cpy, &pubsub__Marker_def);
	}
	else if (strcmp(definition, "costmap") == 0)
	{
		ps_copy_message_definition(cpy, &pubsub__Costmap_def);
	}
	else if (strcmp(definition, "pose") == 0)
	{
		ps_copy_message_definition(cpy, &pubsub__Pose_def);
	}
	else if (strcmp(definition, "path") == 0)
	{
		ps_copy_message_definition(cpy, &pubsub__Path_def);
	}
	else
	{
        struct ps_msg_field_t fields[50];
        struct ps_message_definition_t temp_def = { 3803012856, "pubsub__Int32", 1, fields, 0, 0, 0, 0 };
        // custom!
        char* def_copy = strdup(definition);
        char* saveptr1, *saveptr2;
        // the format will be "namehere;int32 field_one;float32 field_two;"
        // tokenize each line out
        char* line = strtok_r(def_copy, ";", &saveptr1);
        int num_fields = 0;
        while (line != NULL)
        {
            char* line_copy = strdup(line);
            const char* type = 0;
            const char* name = 0;
            
            char* word = strtok_r(line_copy, " ", &saveptr2);
            int word_cnt = 0;
            while (word != NULL)
            {
                if (word_cnt == 0)
                {
                    type = word;
                }
                else if (word_cnt == 1)
                {
                    name = word;
                }
                word = strtok_r(NULL, " ", &saveptr2);
                word_cnt++;
            }

            if (num_fields == 0)
            {
                temp_def.name = type;
            }
            else
            {
                struct ps_msg_field_t* field = &fields[num_fields-1];
                field->name = name;
                field->length = 1;
                field->content_length = 0;
                field->type = 0;// filled in below
                field->flags = 0;
                if (strcmp(type, "int8") == 0)
                {
                    field->type = FT_Int8;
                }
                if (strcmp(type, "uint8") == 0)
                {
                    field->type = FT_UInt8;
                }
                if (strcmp(type, "int16") == 0)
                {
                    field->type = FT_Int16;
                }
                if (strcmp(type, "uint16") == 0)
                {
                    field->type = FT_UInt16;
                }
                if (strcmp(type, "int32") == 0)
                {
                    field->type = FT_Int32;
                }
                if (strcmp(type, "uint32") == 0)
                {
                    field->type = FT_UInt32;
                }
                if (strcmp(type, "int64") == 0)
                {
                    field->type = FT_Int64;
                }
                if (strcmp(type, "uint64") == 0)
                {
                    field->type = FT_UInt64;
                }
                if (strcmp(type, "float32") == 0)
                {
                    field->type = FT_Float32;
                }
                if (strcmp(type, "float64") == 0)
                {
                    field->type = FT_Float64;
                }
                if (strcmp(type, "string") == 0)
                {
                    field->type = FT_String;
                }
            }
            line = strtok_r(NULL, ";", &saveptr1);
            num_fields++;
        }

        // finish filling out the definition
        temp_def.num_fields = num_fields-1;
        temp_def.hash = 10010101;// todo actually calculate
		ps_copy_message_definition(cpy, &temp_def);
	}

	ps_node_create_publisher(&nodes[node], strdup(topic), cpy, &publishers[free_pub], latched);
	pub_initialized[free_pub] = 1;
	return free_pub;// todo support other pubs
}

EXPORT void ps_publish(int pub, const void* msg, int len)
{
    // publish the message simply since it is already encoded
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
