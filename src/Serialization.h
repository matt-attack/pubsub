#pragma once

enum ps_field_types
{
	FT_Int8,
	FT_Int16,
	FT_Int32,
	FT_Int64,
	FT_UInt8,
	FT_UInt16,
	FT_UInt32,
	FT_UInt64,
	FT_Float32,
	FT_Float64,
	FT_String,
	FT_WString,
	FT_Array, //indicates the number of fields following contained in it
	          // recurses down if we hit another array type without advancing
};

struct ps_field_t
{
	ps_field_types type;
	char* name;
	unsigned int length;//length of the array, 0 if dynamic
	unsigned short content_length;//number of fields inside this array
};

struct ps_message_definition_t
{
	unsigned int hash;
	unsigned int num_fields;
	ps_field_t* fields;
};

int ps_serialize_message_definition(void* start, const ps_message_definition_t* definition);

void ps_deserialize_message_definition(const void* start, ps_message_definition_t* definition);

// print out the deserialized contents of the message to console, for rostopic echo like implementations
// in yaml format
void ps_deserialize_print(const void* data, const ps_message_definition_t* definition);