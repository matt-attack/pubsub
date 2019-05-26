#include "Serialization.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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



int ps_serialize_message_definition(void* start, const ps_message_definition_t* definition)
{
	//ok, write out number of fields
	def_header* hdr = (def_header*)start;
	hdr->hash = definition->hash;
	hdr->num_fields = definition->num_fields;

	char* cur = ((char*)start) + sizeof(def_header);
	for (int i = 0; i < definition->num_fields; i++)
	{
		field* f = (field*)cur;
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

void ps_deserialize_message_definition(const void * start, ps_message_definition_t * definition)
{
	//ok, write out number of fields
	def_header* hdr = (def_header*)start;
	definition->hash = hdr->hash;
	definition->num_fields = hdr->num_fields;

	definition->fields = (ps_field_t*)malloc(sizeof(ps_field_t)*definition->num_fields);

	char* cur = ((char*)start) + sizeof(def_header);
	for (int i = 0; i < definition->num_fields; i++)
	{
		field* f = (field*)cur;
		definition->fields[i].type = (ps_field_types)f->type;
		definition->fields[i].length = f->length;
		definition->fields[i].content_length = f->content_length;
		//need to allocate the name
		definition->fields[i].name = (char*)malloc(strlen(f->name) + 1);
		strcpy(definition->fields[i].name, f->name);

		cur += 1 + 4 + 2 + strlen(f->name) + 1;
	}
}

const char* ps_deserialize_internal(const char* data, const ps_field_t* fields, int num_fields, int indentation = 0)
{
	//ok, lets work our way through this
	for (int i = 0; i < num_fields; i++)
	{
		const ps_field_t* field = &fields[i];
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
					printf("%i", (int)*(signed char*)data);
					data += 1;
					break;
				case FT_Int16:
					printf("%s: %i\n", field->name, (int)*(signed short*)data);
					data += 2;
					break;
				case FT_Int32:
					printf("%s: %i\n", field->name, (int)*(int*)data);
					data += 4;
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
			else if (field->length > 1)//print static arrrays
			{
				printf("%s: [", field->name);

				for (int i = 0; i < field->length; i++)
				{
					// non dynamic types 
					switch (field->type)
					{
					case FT_Int8:
						printf("%i", (int)*(signed char*)data);
						data += 1;
						break;
					case FT_Int16:
						printf("%i", (int)*(signed short*)data);
						data += 2;
						break;
					case FT_Int32:
						printf("%i", (int)*(int*)data);
						data += 4;
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

					if (i == field->length - 1)
					{
						printf("]\n");
					}
					else
					{
						printf(", ");
					}
				}
			}
			else
			{
				printf("ERROR: Dynamic arrays currently unhandled...");
			}
		}
	}
	return data;
}

void ps_deserialize_print(const void * data, const ps_message_definition_t* definition)
{
	ps_deserialize_internal((char*)data, definition->fields, definition->num_fields, 0);
}
