#include <pubsub/Serialization.h>
#include <pubsub/Node.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#pragma pack(push)
#pragma pack(1)
struct msg_definition_header
{
	uint32_t hash;
	uint32_t num_fields;
	uint32_t num_enums;
};

struct field
{
	uint8_t type;
	uint32_t length;
	uint16_t content_length;//if we are an array
	char name[50];
	//string goes here
};

struct enumeration
{
	int32_t value;
	uint8_t field;
	char name[50];
};
#pragma pack(pop)



int ps_serialize_message_definition(void* start, const struct ps_message_definition_t* definition)
{
	//ok, write out number of fields
	struct msg_definition_header* hdr = (struct msg_definition_header*)start;
	hdr->hash = definition->hash;
	hdr->num_fields = definition->num_fields;
	hdr->num_enums = definition->num_enums;

	char* cur = ((char*)start) + sizeof(struct msg_definition_header);

	// now write in the name
	cur += serialize_string(cur, definition->name);

	for (unsigned int i = 0; i < definition->num_fields; i++)
	{
		struct field* f = (struct field*)cur;
		f->type = definition->fields[i].type | (definition->fields[i].flags << 5);
		f->length = definition->fields[i].length;
		f->content_length = definition->fields[i].content_length;
		strcpy(f->name, definition->fields[i].name);

		cur += 1 + 4 + 2 + strlen(definition->fields[i].name)+ 1;
	}
	
	// serialize any enums
	for (unsigned int i = 0; i < definition->num_enums; i++)
	{
		struct enumeration* f = (struct enumeration*)cur;
		f->value = definition->enums[i].value;
		f->field = definition->enums[i].field;
		strcpy(f->name, definition->enums[i].name);

		cur += 1 + 4 + strlen(definition->enums[i].name) + 1;
	}
	
	//note, we will run into issues with larger message definitions on udp based protocols, oh well
	// todo die if we make too big of a message

	return cur - (char*)start;
}

void ps_copy_message_definition(struct ps_message_definition_t* dst, const struct ps_message_definition_t* src)
{
	dst->num_fields = src->num_fields;
	dst->hash = src->hash;
	dst->fields = (struct ps_msg_field_t*)malloc(sizeof(struct ps_msg_field_t)*dst->num_fields);
	char* name = (char*)malloc(strlen(src->name)+1);
	strcpy(name, src->name);
	dst->name = name;
	for (unsigned int i = 0; i < dst->num_fields; i++)
	{
		dst->fields[i].type = src->fields[i].type;
		dst->fields[i].flags = src->fields[i].flags;
		dst->fields[i].length = src->fields[i].length;
		dst->fields[i].content_length = src->fields[i].content_length;
		char* name = (char*)malloc(strlen(src->fields[i].name) + 1);
		strcpy(name, src->fields[i].name);
		dst->fields[i].name = name;
	}
	dst->num_enums = src->num_enums;
	dst->enums = (struct ps_msg_enum_t*)malloc(sizeof(struct ps_msg_enum_t)*dst->num_enums);
	for (unsigned int i = 0; i < dst->num_enums; i++)
	{
		dst->enums[i].value = src->enums[i].value;
		dst->enums[i].field = src->enums[i].field;
		char* name = (char*)malloc(strlen(src->enums[i].name) + 1);
		strcpy(name, src->enums[i].name);
		dst->enums[i].name = name;
	}
}

void ps_deserialize_message_definition(const void * start, struct ps_message_definition_t * definition)
{
	//ok, write out number of fields
	struct msg_definition_header* hdr = (struct msg_definition_header*)start;
	definition->hash = hdr->hash;
	definition->num_fields = hdr->num_fields;
	definition->num_enums = hdr->num_enums;
	definition->decode = 0;
	definition->encode = 0;
	definition->name = 0;

	definition->fields = (struct ps_msg_field_t*)malloc(sizeof(struct ps_msg_field_t)*definition->num_fields);

	char* cur = ((char*)start) + sizeof(struct msg_definition_header);

	// read in the name
	int len = strlen(cur);
	char* name = malloc(len + 1);
	strcpy(name, cur);
	definition->name = name;
	cur += len + 1;
	for (unsigned int i = 0; i < definition->num_fields; i++)
	{
		struct field* f = (struct field*)cur;
		definition->fields[i].type = (ps_field_types)f->type & 0x1F;
		definition->fields[i].flags = f->type >> 5;
		definition->fields[i].length = f->length;
		definition->fields[i].content_length = f->content_length;
		//need to allocate the name
		int len = strlen(f->name);
		char* field_name = (char*)malloc(len + 1);
		strcpy(field_name, f->name);
		definition->fields[i].name = field_name;
		cur += 1 + 4 + 2 + len + 1;
	}
	
	definition->enums = (struct ps_msg_enum_t*)malloc(sizeof(struct ps_msg_enum_t)*definition->num_enums);
	
	for (unsigned int i = 0; i < definition->num_enums; i++)
	{
		struct enumeration* f = (struct enumeration*)cur;
		definition->enums[i].value = f->value;
		definition->enums[i].field = f->field;
		//need to allocate the name
		int len = strlen(f->name);
		char* enum_name = (char*)malloc(len + 1);
		strcpy(enum_name, f->name);
		definition->enums[i].name = enum_name;
		cur += 1 + 4 + len + 1;
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
	
	for (unsigned int i = 0; i < definition->num_enums; i++)
	{
		free((void*)definition->enums[i].name);
	}
	free(definition->enums);
}

struct ps_deserialize_iterator ps_deserialize_start(const char* msg, const struct ps_message_definition_t* definition)
{
	struct ps_deserialize_iterator iter;
	iter.next_field_index = 0;
	iter.next_position = msg;
	iter.fields = definition->fields;
	iter.num_fields = definition->num_fields;
	return iter;
}

// takes in a deserialize iterator and returns pointer to the data and the current field
const char* ps_deserialize_iterate(struct ps_deserialize_iterator* iter, const struct ps_msg_field_t** f, uint32_t* l)
{
	if (iter->next_field_index == iter->num_fields)
	{
		return 0;
	}
	
	const struct ps_msg_field_t* field = &iter->fields[iter->next_field_index++];
	*f = field;
	
	const char* position = iter->next_position;
	
	// now advance next position by the field size
	if (field->type == FT_String)
	{
		if (field->length == 1)
		{
			uint32_t len = strlen(position);
			*l = len;
			iter->next_position += len + 1;
		}
		else
		{
			int len = field->length;
			if (field->length == 0)
			{
				len = (int)*(uint32_t*)position;
				position += 4;
				*l = len;
				iter->next_position += 4;
			}
			
			for (int i = 0; i < len; i++)
			{
				int len = (int)*(uint32_t*)iter->next_position;
				iter->next_position += 4;
				iter->next_position += len;
			}
		}
	}
	else if (field->type == FT_Array)
	{
		// iterate again, todo show this recursion somehow in the iter
		// okay, lets just not allow struct arrays in arrays for now?
		// TODO, i dont think this is even implemented in code gen
	}
	else
	{
		int field_size = 0;
		if (field->type == FT_Int8 || field->type == FT_UInt8)
		{
			field_size = 1;
		}
		else if (field->type == FT_Int16 || field->type == FT_UInt16)
		{
			field_size = 2;
		}
		else if (field->type == FT_Int32 || field->type == FT_UInt32 || field->type == FT_Float32)
		{
			field_size = 4;
		}
		else if (field->type == FT_Int64 || field->type == FT_UInt64 || field->type == FT_Float64)
		{
			field_size = 8;
		}
		
		// now handle length
		if (field->length > 0)
		{
			iter->next_position += field_size*field->length;
			*l = field->length;
		}
		else
		{
			// dynamic array
			uint32_t length = *(uint32_t*)position;
			iter->next_position += 4;
			*l = length;
			position += 4;// skip over the length for the end user
			iter->next_position += length*field_size;
		}
	}
	return position;
}

void ps_deserialize_print(const void * data, const struct ps_message_definition_t* definition, unsigned int max_array_size, const char* field_name)
{
	struct ps_deserialize_iterator iter = ps_deserialize_start(data, definition);
	const struct ps_msg_field_t* field; uint32_t length; const char* ptr;
	int it = 0;
	while (ptr = ps_deserialize_iterate(&iter, &field, &length))
	{
		if (field_name && strcmp(field_name, field->name) != 0)
		{
			continue;
		}
		if (field->type == FT_String)
		{
			// strings are already null terminated
			if (field->length == 1)
			{
				printf("%s: %s\n", field->name, ptr);
			}
			else
			{
				printf("%s: [", field->name);
				for (int i = 0; i < length; i++)
				{
					int len = (int)*(uint32_t*)ptr;
					ptr += 4;
					const char* str = ptr;
					printf("%s", str);
					
					if (i != length - 1)
					{
						printf(", ");
					}
					ptr += len;
				}
				printf("]\n");
			}
		}
		else
		{
			if (field->length == 1)
			{
				printf("%s: ", field->name);
			}
			else
			{
				printf("%s: [", field->name);
			}
			
			if (length == 0)
			{
				printf("]\n");
			}

			if (length > max_array_size && max_array_size != 0)
			{
				printf("%i elements hidden...]\n", length);
				length = 0;
			}

			for (unsigned int i = 0; i < length; i++)
			{
				uint64_t value = 0;
				// non dynamic types 
				switch (field->type)
				{
				case FT_Int8:
					printf("%i", (int)*(int8_t*)ptr);
					value = (uint64_t)*(int8_t*)ptr;
					ptr += 1;
					break;
				case FT_Int16:
					printf("%i", (int)*(int16_t*)ptr);
					value = (uint64_t)*(int16_t*)ptr;
					ptr += 2;
					break;
				case FT_Int32:
					printf("%i", (int)*(int32_t*)ptr);
					value = (uint64_t)*(int32_t*)ptr;
					ptr += 4;
					break;
				case FT_Int64:
					printf("%li", (long int)*(int64_t*)ptr);
					value = (uint64_t)*(int64_t*)ptr;
					ptr += 8;
					break;
				case FT_UInt8:
					printf("%i", (int)*(uint8_t*)ptr);
					value = (uint64_t)*(uint8_t*)ptr;
					ptr += 1;
					break;
				case FT_UInt16:
					printf("%i", (int)*(uint16_t*)ptr);
					value = (uint64_t)*(uint16_t*)ptr;
					ptr += 2;
					break;
				case FT_UInt32:
					printf("%i", (unsigned int)*(uint32_t*)ptr);
					value = (uint64_t)*(uint32_t*)ptr;
					ptr += 4;
					break;
				case FT_UInt64:
					printf("%li", (unsigned long int)*(uint64_t*)ptr);
					value = (uint64_t)*(uint64_t*)ptr;
					ptr += 8;
					break;
				case FT_Float32:
					printf("%f", *(float*)ptr);
					ptr += 4;
					break;
				case FT_Float64:
					printf("%lf", *(double*)ptr);
					ptr += 8;
					break;
				default:
					printf("ERROR: unhandled field type when parsing....\n");
				}
				
				if (field->flags > 0)
				{
					if (field->flags == FF_ENUM)
					{
						const char* name = "Enum Not Found";
						for (unsigned int i = 0; i < definition->num_enums; i++)
						{
							if (definition->enums[i].field == it && value == definition->enums[i].value)
							{
								name = definition->enums[i].name;
							}
						}
						printf(" (%s)", name);
					}
					else if (field->flags == FF_BITMASK)
					{
						const char* name = "Enum Not Found";
						printf(" (");
						for (unsigned int i = 0; i < definition->num_enums; i++)
						{
							if (definition->enums[i].field == it && (definition->enums[i].value & value) != 0)
							{
								name = definition->enums[i].name;
								printf("%s, ", name);
							}
						}
						printf(")");
					}
				}

				if (field->length == 1)
				{
					printf("\n");
				}
				else if (i == length - 1)
				{
					printf("]\n");
				}
				else
				{
					printf(", ");
				}
			}
		}
		it++;
	}
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

void ps_print_definition(const struct ps_message_definition_t* definition, bool print_name)
{
    if (print_name)
    {
	    printf("%s\n\n", definition->name);
    }
	for (unsigned int i = 0; i < definition->num_fields; i++)
	{
		//print out any relevant enums
		for (unsigned int j = 0; j < definition->num_enums; j++)
		{
			if (definition->enums[j].field == i)
			{
				printf("%s %i\n", definition->enums[j].name, definition->enums[j].value);
			}
		}
	
		const char* type_name = "";
		ps_field_types type = definition->fields[i].type;
		if (definition->fields[i].flags == FF_ENUM)
		{
			printf("enum ");
		}
		else if (definition->fields[i].flags == FF_BITMASK)
		{
			printf("bitmask ");
		}
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

void ps_msg_alloc(unsigned int size, struct ps_allocator_t* allocator, struct ps_msg_t* out_msg)
{
	out_msg->len = size;
    if (allocator)
    {
	    out_msg->data = (void*)allocator->alloc(size + sizeof(struct ps_msg_header), allocator->context);
    }
    else
    {
        out_msg->data = (void*)((char*)malloc(size + sizeof(struct ps_msg_header)));
    }
}


struct ps_msg_t ps_msg_cpy(const struct ps_msg_t* msg)
{
	struct ps_msg_t out;
	ps_msg_alloc(msg->len, 0, &out);
	memcpy(ps_get_msg_start(out.data), ps_get_msg_start(msg->data), msg->len);
	return out;
}
