#include <pubsub/Serialization.h>
#include <pubsub/Node.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#pragma pack(push)
#pragma pack(1)
struct def_header
{
	unsigned int hash;
	unsigned int num_fields;
};

struct field
{
	char type;
	unsigned int length;
	unsigned short content_length;//if we are an array
	char name[50];
	//string goes here
};
#pragma pack(pop)



int ps_serialize_message_definition(void* start, const struct ps_message_definition_t* definition)
{
	//ok, write out number of fields
	struct def_header* hdr = (struct def_header*)start;
	hdr->hash = definition->hash;
	hdr->num_fields = definition->num_fields;

	char* cur = ((char*)start) + sizeof(struct def_header);

	// now write in the name
	cur += serialize_string(cur, definition->name);

	for (unsigned int i = 0; i < definition->num_fields; i++)
	{
		struct field* f = (struct field*)cur;
		f->type = definition->fields[i].type;
		f->length = definition->fields[i].length;
		f->content_length = definition->fields[i].content_length;
		strcpy(f->name, definition->fields[i].name);

		cur += 1 + 4 + 2 + strlen(definition->fields[i].name)+ 1;
	}
	
	//note, we will run into issues with larger message definitions on udp based protocols, oh well
	// todo die if we make too big of a message

	return cur - (char*)start;
}

void ps_copy_message_definition(struct ps_message_definition_t* dst, const struct ps_message_definition_t* src)
{
	dst->num_fields = src->num_fields;
	dst->hash = src->hash;
	dst->fields = (struct ps_field_t*)malloc(sizeof(struct ps_field_t)*dst->num_fields);
	char* name = (char*)malloc(strlen(src->name)+1);
	strcpy(name, src->name);
	dst->name = name;
	for (unsigned int i = 0; i < dst->num_fields; i++)
	{
		dst->fields[i].type = src->fields[i].type;
		dst->fields[i].length = src->fields[i].length;
		dst->fields[i].content_length = src->fields[i].content_length;
		char* name = (char*)malloc(strlen(src->fields[i].name) + 1);
		strcpy(name, src->fields[i].name);
		dst->fields[i].name = name;
	}
}

void ps_deserialize_message_definition(const void * start, struct ps_message_definition_t * definition)
{
	//ok, write out number of fields
	struct def_header* hdr = (struct def_header*)start;
	definition->hash = hdr->hash;
	definition->num_fields = hdr->num_fields;
	definition->decode = 0;
	definition->encode = 0;
	definition->name = 0;

	definition->fields = (struct ps_field_t*)malloc(sizeof(struct ps_field_t)*definition->num_fields);

	char* cur = ((char*)start) + sizeof(struct def_header);

	// read in the name
	int len = strlen(cur);
	char* name = malloc(len + 1);
	strcpy(name, cur);
	definition->name = name;
	cur += len + 1;
	for (unsigned int i = 0; i < definition->num_fields; i++)
	{
		struct field* f = (struct field*)cur;
		definition->fields[i].type = (ps_field_types)f->type;
		definition->fields[i].length = f->length;
		definition->fields[i].content_length = f->content_length;
		//need to allocate the name
		int len = strlen(f->name);
		char* field_name = (char*)malloc(len + 1);
		strcpy(field_name, f->name);
		definition->fields[i].name = field_name;
		cur += 1 + 4 + 2 + len + 1;
	}
}

void ps_free_message_definition(struct ps_message_definition_t * definition)
{
	free((void*)definition->name);

	for (unsigned int i = 0; i < definition->num_fields; i++)
	{
		free((void*)definition->fields[i].name);
	}
	free(definition->fields);
}

const char* ps_deserialize_internal(const char* data, const struct ps_field_t* fields, int num_fields, int indentation)
{
	//ok, lets work our way through this
	for (int i = 0; i < num_fields; i++)
	{
		const struct ps_field_t* field = &fields[i];
		if (field->type == FT_String)
		{
			// for now lets just null terminate strings
			printf("%s: %s\n", field->name, data);
			data += strlen(data) + 1;
		}
		else if (field->type == FT_Array)
		{
			// RECURSE!
			data = ps_deserialize_internal(data, &fields[i + 1], field->content_length, indentation + 1);
		}
		else
		{
			//print fields
			if (field->length == 1)
			{
				// non dynamic types 
				switch (field->type)
				{
				case FT_Int8:
					printf("%i", (int)*(int8_t*)data);
					data += 1;
					break;
				case FT_Int16:
					printf("%s: %i\n", field->name, (int)*(int16_t*)data);
					data += 2;
					break;
				case FT_Int32:
					printf("%s: %i\n", field->name, (int)*(int32_t*)data);
					data += 4;
					break;
				case FT_Int64:
					printf("%s: %li\n", field->name, (long int)*(int64_t*)data);
					data += 8;
					break;
				case FT_UInt8:
					printf("%i", (int)*(uint8_t*)data);
					data += 1;
					break;
				case FT_UInt16:
					printf("%s: %i\n", field->name, (int)*(uint16_t*)data);
					data += 2;
					break;
				case FT_UInt32:
					printf("%s: %i\n", field->name, (unsigned int)*(uint32_t*)data);
					data += 4;
					break;
				case FT_UInt64:
					printf("%s: %li\n", field->name, (unsigned long int)*(uint64_t*)data);
					data += 8;
					break;
				case FT_Float32:
					printf("%s: %f\n", field->name, *(float*)data);
					data += 4;
					break;
				case FT_Float64:
					printf("%s: %lf\n", field->name, *(double*)data);
					data += 8;
					break;
				default:
					printf("ERROR: unhandled field type when parsing....\n");
				}
			}
			else if (field->length > 1 || field->length == 0)//print arrays
			{
				unsigned int length = field->length;
				if (length == 0)
				{
					//read in the length
					length = *(uint32_t*)data;
					data += 4;
				}
				printf("%s: [", field->name);

				for (unsigned int i = 0; i < length; i++)
				{
					// non dynamic types 
					switch (field->type)
					{
					case FT_Int8:
						printf("%i", (int)*(int8_t*)data);
						data += 1;
						break;
					case FT_Int16:
						printf("%i", (int)*(int16_t*)data);
						data += 2;
						break;
					case FT_Int32:
						printf("%i", (int)*(int32_t*)data);
						data += 4;
						break;
					case FT_Int64:
						printf("%li", (long int)*(int64_t*)data);
						data += 8;
						break;
					case FT_UInt8:
						printf("%i", (int)*(uint8_t*)data);
						data += 1;
						break;
					case FT_UInt16:
						printf("%i", (int)*(uint16_t*)data);
						data += 2;
						break;
					case FT_UInt32:
						printf("%i", (unsigned int)*(uint32_t*)data);
						data += 4;
						break;
					case FT_UInt64:
						printf("%li", (unsigned long int)*(uint64_t*)data);
						data += 8;
						break;
					case FT_Float32:
						printf("%f", *(float*)data);
						data += 4;
						break;
					case FT_Float64:
						printf("%lf", *(double*)data);
						data += 8;
						break;
					default:
						printf("ERROR: unhandled field type when parsing....\n");
					}

					if (i == length - 1)
					{
						printf("]\n");
					}
					else
					{
						printf(", ");
					}
				}
			}
		}
	}
	return data;
}

void ps_deserialize_print(const void * data, const struct ps_message_definition_t* definition)
{
	ps_deserialize_internal((char*)data, definition->fields, definition->num_fields, 0);
}

void* ps_get_msg_start(const void* data)
{
	return (void*)((char*)data + sizeof(struct ps_msg_header));
}

const char* TypeToString(ps_field_types type)
{
	switch (type)
	{
		// declarations
		// . . .
	case FT_Int8:
		return "int8";
	case FT_Int16:
		return "int16";
	case FT_Int32:
		return "int32";
	case FT_Int64:
		return "int64";
	case FT_UInt8:
		return "uint8";
	case FT_UInt16:
		return "uint16";
	case FT_UInt32:
		return "uint32";
	case FT_UInt64:
		return "uint64";
	case FT_Float32:
		return "float";
	case FT_Float64:
		return "double";
	case FT_String:
		// statements executed if the expression equals the
		// value of this constant_expression
		return "string";
	default:
		return "Unknown Type";
	}
}

void ps_print_definition(const struct ps_message_definition_t* definition)
{
	printf("%s\n\n", definition->name);
	for (unsigned int i = 0; i < definition->num_fields; i++)
	{
		const char* type_name = "";
		ps_field_types type = definition->fields[i].type;
		if (definition->fields[i].length > 1)
		{
			printf("%s %s[%i]\n", TypeToString(definition->fields[i].type), definition->fields[i].name, definition->fields[i].length);
		}
		else if (definition->fields[i].length == 0)
		{
			printf("%s[] %s\n", TypeToString(definition->fields[i].type), definition->fields[i].name);// dynamic array
		}
		else
		{
			printf("%s %s\n", TypeToString(definition->fields[i].type), definition->fields[i].name);
		}
	}
}

void ps_msg_alloc(unsigned int size, struct ps_msg_t* out_msg)
{
	out_msg->len = size;
	out_msg->data = (void*)((char*)malloc(size + sizeof(struct ps_msg_header)));
}


struct ps_msg_t ps_msg_cpy(const struct ps_msg_t* msg)
{
	struct ps_msg_t out;
	ps_msg_alloc(msg->len, &out);
	memcpy(ps_get_msg_start(out.data), ps_get_msg_start(msg->data), msg->len);
	return out;
}
