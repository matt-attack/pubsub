#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    struct ps_allocator_t
    {
	    void*(*alloc)(unsigned int size, void* context);
	    void(*free)(void*);
	    void* context;
    };

    extern struct ps_allocator_t ps_default_allocator;

	enum ps_field_types
	{
		FT_Int8 = 0,
		FT_Int16 = 1,
		FT_Int32 = 2,
		FT_Int64 = 3,
		FT_UInt8 = 4,
		FT_UInt16 = 5,
		FT_UInt32 = 6,
		FT_UInt64 = 7,
		FT_MaxInteger,//for comparisons, not present in messages
		FT_Float32 = 9,
		FT_Float64 = 10,
		FT_MaxFloat,// all floats and ints are less than this, not present in messages
		FT_String,
		FT_Array, //indicates the number of fields following contained in it
				  // recurses down if we hit another array type without advancing
	};
	typedef enum ps_field_types ps_field_types;

	enum ps_field_flags
	{
		FF_NONE = 0,
		FF_BITMASK = 1,
		FF_ENUM = 2
	};
	typedef enum ps_field_flags ps_field_flags;

	struct ps_msg_field_t
	{
		ps_field_types type;
		ps_field_flags flags;// packed in upper bits of type, but broken out here
		const char* name;
		unsigned int length;//length of the array, 0 if dynamic
		unsigned short content_length;//number of fields inside this array
	};
	
	struct ps_msg_enum_t
	{
		const char* name;// name of the enumeration
		int value;// value of the enumeration
		int field;// the field this is associated with in the message
	};


	// encoded message
	struct ps_msg_t
	{
		void* data;
		unsigned int len;
	};

	struct ps_allocator_t;
	typedef struct ps_msg_t(*ps_fn_encode_t)(struct ps_allocator_t* allocator, const void* msg);
	typedef void*(*ps_fn_decode_t)(const void* data, struct ps_allocator_t* allocator);// allocates the message
	struct ps_message_definition_t
	{
		unsigned int hash;
		const char* name;
		unsigned int num_fields;
		struct ps_msg_field_t* fields;
		ps_fn_encode_t encode;
		ps_fn_decode_t decode;
		unsigned int num_enums;
		struct ps_msg_enum_t* enums;
	};

	// Serializes a given message definition to a buffer.
	// Returns: Number of bytes written
	int ps_serialize_message_definition(void* start, const struct ps_message_definition_t* definition);

	// Deserializes a message definition from the specified buffer.
	void ps_deserialize_message_definition(const void* start, struct ps_message_definition_t* definition);

	// print out the deserialized contents of the message to console, for rostopic echo like implementations
	// in yaml format
	// if field is non-null only print out the content of that field
	void ps_deserialize_print(const void* data, const struct ps_message_definition_t* definition, unsigned int max_array_size, const char* field);
	
	struct ps_deserialize_iterator
	{
		const char* next_position;
		int next_field_index;
	
		int num_fields;
		const struct ps_msg_field_t* fields;
	};
	
	// Create an iteratator to iterate through the fields of a serialized message
	// Returns: The iterator
	struct ps_deserialize_iterator ps_deserialize_start(const char* msg, const struct ps_message_definition_t* definition);
	
	// Iterate through a serialized message one field at a time
	// Returns: Start pointer in the message for the current field or zero when at the end
	const char* ps_deserialize_iterate(struct ps_deserialize_iterator* iter, const struct ps_msg_field_t** f, uint32_t* l);

	// Frees a dynamically allocated message definition
	void ps_free_message_definition(struct ps_message_definition_t* definition);

	// Makes a dynamically allocated copy of the given message definition into the destination
	void ps_copy_message_definition(struct ps_message_definition_t* dst, const struct ps_message_definition_t* src);
	
	// Prints out a message definition to console.
	void ps_print_definition(const struct ps_message_definition_t* definition, bool print_name);

	// Allocates a new message, including space for the header, with the given allocator and size
	void ps_msg_alloc(unsigned int size, struct ps_allocator_t* allocator, struct ps_msg_t* out_msg);

	// Get the start position, after the header, for the data portion of a serialized message
	// Returns: Pointer to start position 
	void* ps_get_msg_start(const void* data);

	// Makes a copy of a given serialized message
	// Returns: The new copy
	struct ps_msg_t ps_msg_cpy(const struct ps_msg_t* msg);

#ifdef __cplusplus
}
#endif
