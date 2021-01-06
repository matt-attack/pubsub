#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

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
		FT_MaxInteger,//for comparisons
		FT_Float32 = 9,
		FT_Float64 = 10,
		FT_MaxFloat,// all floats and ints are less than this
		FT_String,
		FT_WString,
		FT_Array, //indicates the number of fields following contained in it
				  // recurses down if we hit another array type without advancing
	};
	typedef enum ps_field_types ps_field_types;



	struct ps_field_t
	{
		ps_field_types type;
		const char* name;
		unsigned int length;//length of the array, 0 if dynamic
		unsigned short content_length;//number of fields inside this array
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
		struct ps_field_t* fields;
		ps_fn_encode_t encode;
		ps_fn_decode_t decode;
	};

	int ps_serialize_message_definition(void* start, const struct ps_message_definition_t* definition);

	void ps_deserialize_message_definition(const void* start, struct ps_message_definition_t* definition);

	// print out the deserialized contents of the message to console, for rostopic echo like implementations
	// in yaml format
	void ps_deserialize_print(const void* data, const struct ps_message_definition_t* definition);

	void ps_free_message_definition(struct ps_message_definition_t* definition);

	void ps_copy_message_definition(struct ps_message_definition_t* dst, const struct ps_message_definition_t* src);

	void ps_print_definition(const struct ps_message_definition_t* definition);

	void ps_msg_alloc(unsigned int size, struct ps_msg_t* out_msg);

	void* ps_get_msg_start(const void* data);

	struct ps_msg_t ps_msg_cpy(const struct ps_msg_t* msg);

#ifdef __cplusplus
}
#endif
