#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <map>

#include <vector>
#include <string>
#include <sstream>
#include <algorithm>

std::vector<std::string> split(const std::string& data, char c)
{
	std::stringstream ss(data);
	std::string item;
	std::vector<std::string> lines;
	while (std::getline(ss, item, c))
	{
		lines.push_back(item);
	}

	return lines;
}

struct enumeration
{
	std::string name;
	std::string value;
	int field_num;
};

struct Type
{
	std::string name;
	std::string base_type;
	std::string type_enum;
	// if zero size, a basic type
	std::vector<std::pair<Type*, std::string>> fields;
};

// stupid hack
static std::string _current_file;

struct field
{
	std::string name;
	Type* type;

	int array_size;
	
	std::string flag;

	uint32_t line_number;

	std::string getBaseType()
	{
		return type->base_type;
	}
	
	std::string getFlags()
	{
		if (flag == "enum")
			return "FF_ENUM";
		if (flag == "bitmask")
			return "FF_BITMASK";
		if (flag == "")
			return "FF_NONE";
		
		printf("%s:%i ERROR: Invalid flag '%s' for field '%s'\n", _current_file.c_str(), line_number, flag.c_str(), name.c_str());
		throw 7;
		return "invalid";
	}

	std::string getTypeEnum()
	{
		return type->type_enum;
	}
	
	void GenerateFree(std::string& output)
	{
		if (type->name == "string" && array_size != 1)
		{
			if (array_size == 0)
			{
				output += "    for (int i = 0; i < this->" + name + "_length; i++)\n    {\n";
			}
			else
			{
				output += "    for (int i = 0; i < " + std::to_string(array_size) + "; i++)\n    {\n";
			}
			
			output += "      free(this->" + name + "[i]);\n";
			output += "    }\n";
		}
		
		if (type->name == "string" || array_size != 1)
		{
			output += "    if (this->" + name + ")\n";
			output += "      free(this->" + name + ");\n";
		}
	}
	
	void GenerateCopy(std::string& output, const std::string& source)
	{
		if (type->name == "string")
		{
			if (array_size == 1)
			{
				output += "    " + name + " = new char[strlen(" + source + "." + name + ") + 1];\n";
				output += "    strcpy(" + name + ", " + source + "." + name + ");\n";
			}
			else
			{
				if (array_size == 0)
				{
					// encode the array legnth
					output += "    " + name + "_length = " + source + "." + name + "_length;\n";
					output += "    " + name + " = (char**)malloc(sizeof(char**)*" + name + "_length);\n";
					output += "    for (int i = 0; i < " + name + "_length; i++)\n    {\n";
				}
				else
				{
					output += "    for (int i = 0; i < " + std::to_string(array_size) + "; i++)\n    {\n";
				}
					
				output += "      " + name + "[i] = strdup(" + source + "." + name + "[i]);\n";
				output += "    }\n";
			}
		}
		else if (array_size > 1)
		{
			//todo probably should finish this
		}
		else if (array_size == 0)
		{
			output += "    " + name + " = (" + getBaseType() + "*)malloc(" + source + "." + name + "_length*sizeof(" + getBaseType() + "));\n";
			output += "    " + name + "_length = " + source + "." + name + "_length;\n";
			output += "    memcpy(" + name + ", " + source + "." + name + ", " + source + "." + name + "_length*sizeof(" + getBaseType() + "));\n";
		}
		else
		{
			output += "    " + name + " = " + source + "." + name + "; \n";
		}
	}
};

std::string strip_whitespace(const std::string& in)
{
	std::string out;
	for (int i = 0; i < in.length(); i++)
	{
		char c = in[i];
		if (c != ' ' && c != '\t')
		{
			out += c;
		}
	}
	return out;
}

std::string generate(const char* definition, const char* name)
{
	std::string output;

	// first parse out the fields and enums
	std::vector<field> fields;
	std::vector<enumeration> enumerations;

	// remove any windows line extensions
	std::string cleaned;
	size_t len = strlen(definition);
	for (size_t i = 0; i < len; i++)
	{
		if (definition[i] != '\r')
		{
			cleaned += definition[i];
		}
	}
	
	auto lines = split(cleaned.c_str(), '\n');

	// now lets remove comments from lines so we can generate a hash from it
	// todo, also remove extra spacing so it doesn't effect hash
	for (auto& line : lines)
	{
		// go through and remove everything after a #
		size_t p = line.find_first_of('#');
		if (p != -1)
		{
			line = line.substr(0, p);
		}
		
		// now strip any leading whitespace
		std::string cp;
		for (int i = 0; i < line.length(); i++)
		{
			if (line[i] != ' ' && line[i] != '\t')
			{
				for (; i < line.length(); i++)
				{
					cp += line[i];
				}
				break;
			}
		}
		line = cp;
	}

	// A table of supported types and their information
	Type* current_struct = 0;
	std::map<std::string, Type*> types;// todo fix this memory leak

	// add default types
	auto string_type = new Type{"string", "char*", "FT_String", {}};
	types["string"] = string_type;
	types["uint32"] = new Type{"uint32", "uint32_t", "FT_UInt32", {}};
	types["int32"] = new Type{"int32", "int32_t", "FT_Int32", {}};
	types["uint16"] = new Type{"uint16", "uint16_t", "FT_UInt16", {}};
	types["int16"] = new Type{"int16", "int16_t", "FT_Int16", {}};
	types["uint8"] = new Type{"uint8", "uint8_t", "FT_UInt8", {}};
	types["int8"] = new Type{"int8", "int8_t", "FT_Int8", {}};
	types["float"] = new Type{"float", "float", "FT_Float32", {}};
	types["double"] = new Type{"double", "double", "FT_Float64", {}};

	// also generate the hash while we are at it
	uint32_t hash = 0;
	uint32_t line_number = 0;
	for (auto& line : lines)
	{
		line_number++;
		for (unsigned int i = 0; i < line.length(); i++)
		{
			hash *= std::abs(line[i]);
			hash += i;
		}

		// okay so each of these should contain a type, then a field
		// todo refactor me to actually tokenize
		bool has_equal = (line.find('=') != std::string::npos);
		auto words = split(line, ' ');
		if (words.size() == 0)
		{
			continue;
		}
		else if (words.size() == 1 && words[0] == "end_struct")
		{
			if (current_struct)
			{
				types[current_struct->name] = current_struct;
				current_struct = 0;
			}
			else
			{
				printf("%s:%i ERROR: Invalid syntax. Unexpected end_struct without a start.\n", _current_file.c_str(), line_number);
				throw 7;
			}			
		}
		else if (words.size() == 2 && !has_equal)
		{
			std::string name = words[1];
			std::string type = words[0];

			int size = 1;
			if (name.length() > 2 && name.substr(name.size() - 2) == "[]")
			{
				size = 0;
				name = name.substr(0, name.length() - 2);
			}
			else if (name.find('[') != -1)
			{
				int index = name.find('[');
				size = std::stoi(name.substr(index + 1));
				name = name.substr(0, index);
			}

			if (type == "struct")
			{
				if (current_struct)
				{
					printf("%s:%i ERROR: Invalid syntax. Cannot have a struct inside of a struct.\n", _current_file.c_str(), line_number);
					throw 7;
				}

				// make sure no type with this name exists
				if (types.find(name) != types.end())
				{
					printf("%s:%i ERROR: A type with the name '%s' already exists.\n", _current_file.c_str(), line_number, name.c_str());
					throw 7;
				}

				current_struct = new Type();
				current_struct->name = name;
				current_struct->base_type = name;
				current_struct->type_enum = "FT_Struct";
				continue;
			}

			// lookup the type
			Type* real_type = 0;
			auto res = types.find(type);
			if (res != types.end())
			{
				real_type = res->second;
			}
			else
			{
				printf("%s:%i ERROR: Invalid type '%s'\n", _current_file.c_str(), line_number, type.c_str());
				throw 7;
			}
			
			if (current_struct)
			{
				// this belongs in the struct
				current_struct->fields.push_back({real_type, name});
				continue;
			}
			// also fill in array size
			fields.push_back({ name, real_type, size, "", line_number });
		}
		// a line with flags maybe?
		else if (words.size() == 3 && !has_equal)
		{
			std::string flag = words[0];
			std::string type = words[1];
			std::string name = words[2];
			
			int size = 1;
			if (name.substr(name.size() - 2) == "[]")
			{
				size = 0;
				name = name.substr(0, name.length() - 2);
			}
			else if (name.find('[') != -1)
			{
				int index = name.find('[');
				size = std::stoi(name.substr(index + 1));
				name = name.substr(0, index);
			}

			// lookup the type
			Type* real_type = 0;
			auto res = types.find(type);
			if (res != types.end())
			{
				real_type = res->second;
			}
			else
			{
				printf("%s:%i ERROR: Invalid type '%s'\n", _current_file.c_str(), line_number, type.c_str());
				throw 7;
			}

			// also fill in array size
			fields.push_back({ name, real_type, size, flag, line_number});
		}
		else
		{
			// this is either an enum or an error
			auto equals = split(line, '=');
			if (equals.size() == 2)
			{
				// its an enum!
				// remove whitespace from each side
				std::string name = strip_whitespace(equals[0]);
				std::string value = strip_whitespace(equals[1]);
				//printf("it was an enum: %s=%s\n", equals[0].c_str(), equals[1].c_str());
				enumerations.push_back({ name, value, (int)fields.size()});
			}
			else
			{
				printf("%s:%i ERROR: Invalid syntax. Expected either a name or a value.\n", _current_file.c_str(), line_number);
				throw 7;
			}
		}
	}

	std::string raw_name = split(name, '_').back();
    std::string ns = std::string(name).substr(0, std::string(name).find_last_of('_')-1);

	// convert the name into a type
	std::string type_name;
	const char* c = name;
	while (*c != 0)
	{
		if (*c == '/')
		{
			type_name += "__";
		}
		else
		{
			type_name += *c;
		}
		c++;
	}

	// add header guard
	output += "#pragma once\n\n";

	output += "#include <pubsub/Serialization.h>\n";
	output += "#include <stdint.h>\n";
	output += "#include <string.h>\n\n";

	// now that we have these, generate stuff!, start with the struct
	output += "#pragma pack(push, 1)\n";

	// first generate any internal structs
	for (auto& type: types)
	{
		if (type.second->type_enum != "FT_Struct")
		{
			continue;
		}

		std::string struct_name = type_name + "_" + type.second->name;
		type.second->base_type = struct_name;
		output += "struct " + struct_name + "\n{\n";
		for (auto& field: type.second->fields)
		{
			// dont allow strings yet
			if (field.first->base_type == "char*")
			{
				printf("%s:%i ERROR: Strings not yet allowed in structs.\n", _current_file.c_str(), line_number);
				throw 7;
			}

			if (field.first->type_enum == "FT_Struct")
			{
				printf("%s:%i ERROR: Structs not yet allowed in structs.\n", _current_file.c_str(), line_number);
				throw 7;
			}

			output += "  " + field.first->base_type + " " + field.second + ";\n";
		}
		output += "};\n\n";
	}

	// finally generate our own struct
	output += "struct " + type_name + "\n{\n";
	for (auto& field : fields)
	{
		if (field.array_size > 1)
		{
			output += "  " + field.getBaseType() + " " + field.name + "[" + std::to_string(field.array_size) + "];\n";
		}
		else if (field.array_size == 0)
		{
			output += "  " + field.getBaseType() + "* " + field.name + ";\n";
			output += "  uint32_t " + field.name + "_length;\n";
		}
		else
		{
			output += "  " + field.getBaseType() + " " + field.name + ";\n";
		}
	}
	output += "};\n\n";
	output += "#pragma pack(pop)\n";

	// then generate the message definition data
	//ps_field_t std_msgs_String_fields[] = { { FT_String, "value", 1, 0 } };
	//ps_message_definition_t std_msgs_String_def = { 123456789, "std_msgs/String", 1, std_msgs_String_fields };

	// generate the fields
	output += "struct ps_msg_field_t " + type_name + "_fields[] = {\n";
	for (auto& field : fields)
	{
		if (field.getTypeEnum() == "FT_Struct")
		{
			// struct
			auto& members = field.type->fields;
			output += "  { " + field.getTypeEnum() + ", " + field.getFlags() + ", \"" + field.name + "\", ";
			output += std::to_string(field.array_size) + ", " + std::to_string(members.size()) + " }, \n";// todo use for array types

			// now add struct fields
			for (auto& m : members)
			{
				output += "  { " + m.first->type_enum + ", " + "FF_NONE" + ", \"" + m.second + "\", ";
				output += std::to_string(1) + ", 0 }, \n";// todo support array members
			}
		}
		else
		{
			output += "  { " + field.getTypeEnum() + ", " + field.getFlags() + ", \"" + field.name + "\", ";
			output += std::to_string(field.array_size) + ", 0 }, \n";// todo use for array types
		}
	}
	output += "};\n\n";
	
	// generate enum metadata
	if (enumerations.size())
	{
		output += "struct ps_msg_enum_t " + type_name + "_enums[] = {\n";
		for (auto& e: enumerations)
		{
			output += "  {\"" + e.name + "\", " + e.value + ", " + std::to_string(e.field_num) + "},\n";
		}
		output += "};\n\n";
	}

	// okay, generate encoding/decoding
	// for the moment, we can just copy it to decode, if its just a packed struct
	/*void joy_msgs_Joy_decode(const void* data, joy_msgs_Joy* out)
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

	// okay, lets determine if it is pure data, if so just do a memcpy

	bool is_pure = true;
	// determine if we are a pure structure (nothing dynamic)
	for (auto& field : fields)
	{
		if (field.type == string_type)
		{
			is_pure = false;
			break;
		}
		else if (field.array_size == 0)
		{
			is_pure = false;
			break;
		}
	}

	if (is_pure)
	{
		//generate simple de/serializaton
		output += "void* " + type_name + "_decode(const void* data, struct ps_allocator_t* allocator)\n{\n";
		output += "  struct " + type_name + "* out = (struct " + type_name + "*)allocator->alloc(sizeof(struct " + type_name + "), allocator->context);\n";
		output += "  *out = *(struct " + type_name + "*)data;\n";
		output += "  return out;\n";
		output += "}\n\n";

		// now for encode
		output += "struct ps_msg_t " + type_name + "_encode(struct ps_allocator_t* allocator, const void* msg)\n{\n";
		output += "  int len = sizeof(struct " + type_name + ");\n";
		output += "  struct ps_msg_t omsg;\n";
		output += "  ps_msg_alloc(len, allocator, &omsg);\n";
		output += "  memcpy(ps_get_msg_start(omsg.data), msg, len);\n";
		output += "  return omsg;\n}\n";
	}
	else
	{
		//need to split it in sections between the strings
		output += "void* " + type_name + "_decode(const void* data, struct ps_allocator_t* allocator)\n{\n";
		output += "  char* p = (char*)data;\n";
		output += "  int len = sizeof(struct "+type_name+");\n";
		output += "  struct "+type_name+"* out = (struct " + type_name + "*)allocator->alloc(len, allocator->context);\n";
		// for now lets just decode non strings
		for (size_t i = 0; i < fields.size(); i++)
		{
			if (fields[i].type == string_type)
			{
				if (fields[i].array_size == 1)
				{
					output += "  int len_" + fields[i].name + " = strlen(p) + 1; \n";
					output += "  out->" + fields[i].name + " = (char*)allocator->alloc(len_" + fields[i].name + ", allocator->context);\n";
					output += "  memcpy(out->" + fields[i].name + ", p, len_" + fields[i].name + ");\n";// is memcpy faster in this case?
					output += "  p += len_" + fields[i].name + ";\n";
				}
				else
				{
					if (fields[i].array_size == 0)
					{
						output += "  int num_" + fields[i].name + " = *(uint32_t*)p;\n";
						output += "  p += 4;\n";// add size of length
					}
					else
					{
						output += "  int num_" + fields[i].name + " = " + std::to_string(fields[i].array_size) + ";\n";
					}
					
					output += "  out->" + fields[i].name + "_length = num_" + fields[i].name + ";\n";
					output += "  out->" + fields[i].name + " = (char**)malloc(sizeof(char*)*num_" + fields[i].name + ");\n";
					
					// allocate the array
					// need to do it!
					// first, read the number of strings, then read each string
					output += "  for (int i = 0; i < num_" + fields[i].name + "; i++) {\n";
					output += "    int len = *(uint32_t*)p;\n";
					output += "    p += 4;\n";// add size of length
					// now read and allocate each string
					output += "    out->" + fields[i].name + "[i] = (char*)malloc(len);\n";
					output += "    memcpy(out->" + fields[i].name + "[i], p, len);\n";
					output += "    p += len;\n";
					output += "  }\n";
				}
			}
			else if (fields[i].array_size == 0)
			{
				output += "  int len_" + fields[i].name + " = *(uint32_t*)p;\n";
				//output += "  p += len_" + fields[i].name + "*sizeof(" + fields[i].getBaseType() + ");\n";
				output += "  p += 4;\n";// add size of length
				
				output += "  out->" + fields[i].name + "_length = len_" + fields[i].name + ";\n";
				output += "  out->" + fields[i].name + " = ("+fields[i].getBaseType()+"*)allocator->alloc(len_" + fields[i].name + "*sizeof(" + fields[i].getBaseType() + "), allocator->context);\n";
				//output += "  p += 4;\n";// move past the length
				output += "  memcpy(out->" + fields[i].name + ", p, len_" + fields[i].name + "*sizeof(" + fields[i].getBaseType() + "));\n";// is memcpy faster in this case?
				output += "  p += len_" + fields[i].name + "*sizeof(" + fields[i].getBaseType() + ");\n";
			}
			else
			{
				if (fields[i].array_size > 1)
				{
					output += "  memcpy(out->" + fields[i].name + ", p, sizeof(" + fields[i].getBaseType() + ")*" + std::to_string(fields[i].array_size) + ");\n";
					output += "  p += sizeof("+ fields[i].getBaseType() + ")*" + std::to_string(fields[i].array_size) + ";\n";
				}
				else
				{
					output += "  out->" + fields[i].name + " = *((" + fields[i].getBaseType() + "*)p);\n";
					output += "  p += sizeof(" + fields[i].getBaseType() + ");\n";
				}
			}
		}
		output += "  return (void*)out;\n";
		output += "}\n\n";

		//typedef ps_msg_t(*ps_fn_encode_t)(ps_allocator_t* allocator, const void* msg);
		output += "struct ps_msg_t " + type_name + "_encode(struct ps_allocator_t* allocator, const void* data)\n{\n";
		output += "  const struct " + type_name + "* msg = (const struct " + type_name + "*)data;\n";
		output += "  int len = sizeof(struct " + type_name + ");\n";
		output += "  // calculate the encoded length of the message\n";
		int n_arr = 0;
		for (size_t i = 0; i < fields.size(); i++)
		{
			if (fields[i].type == string_type)
			{
				if (fields[i].array_size == 1)
				{
					output += "  int len_" + fields[i].name + " = strlen(msg->" + fields[i].name + ") + 1; \n";
					output += "  len += len_" + fields[i].name + ";\n";
				}
				else
				{
					if (fields[i].array_size == 0)
					{
						output += "  int num_" + fields[i].name + " = msg->" + fields[i].name + "_length;\n";// for the moment
					}
					else
					{
						output += "  int num_" + fields[i].name + " = " + std::to_string(fields[i].array_size) + ";\n";
					}
					
					// allocate the array
					// need to do it!
					// first, read the number of strings, then read each string
					output += "  for (int i = 0; i < num_" + fields[i].name + "; i++) {\n";
					output += "    len += strlen(msg->" + fields[i].name + "[i]) + 4 + 1;\n";
					output += "  }\n";
				}
				n_arr++;
			}
			else if (fields[i].array_size == 0)
			{
				output += "  len += msg->" + fields[i].name + "_length*sizeof(" + fields[i].getBaseType() + ");\n";
				n_arr++;
			}
		}
		output += "  len -= " + std::to_string(n_arr) + "*sizeof(char*);\n";
		output += "  struct ps_msg_t omsg;\n";
		output += "  ps_msg_alloc(len, allocator, &omsg);\n";
		output += "  char* start = (char*)ps_get_msg_start(omsg.data);\n";
		for (size_t i = 0; i < fields.size(); i++)
		{
			if (fields[i].type == string_type)
			{
				if (fields[i].array_size == 1)
				{
					output += "  strcpy(start, msg->" + fields[i].name + ");\n";
					output += "  start += len_" + fields[i].name + ";\n";
				}
				else
				{
					// now encode it
					if (fields[i].array_size == 0)
					{
						// encode the array legnth
						output += "  *(uint32_t*)start = msg->" + fields[i].name + "_length;\n";
						output += "  start += 4;\n";
						output += "  for (int i = 0; i < msg->" + fields[i].name + "_length; i++)\n  {\n";
					}
					else
					{
						output += "  for (int i = 0; i < " + std::to_string(fields[i].array_size) + "; i++)\n  {\n";
					}
					
					// now encode each string
					output += "    int len = strlen(msg->" + fields[i].name + "[i]) + 1;\n"; 
					output += "    *(uint32_t*)start = len;\n";
					output += "    start += 4;\n";
					output += "    memcpy(start, msg->" + fields[i].name + "[i], len);\n";
					output += "    start += len;\n";
					output += "  }\n";
				}
			}
			else if (fields[i].array_size == 0)
			{
				// write in the length
				output += "  *(uint32_t*)start = msg->" + fields[i].name + "_length;\n";
				output += "  start += 4;\n";
				output += "  memcpy(start, msg->" + fields[i].name + ", msg->" + fields[i].name + "_length*sizeof(" + fields[i].getBaseType() + "));\n";
				output += "  start += msg->" + fields[i].name + "_length*sizeof(" + fields[i].getBaseType() + ");\n";
			}
			else
			{
				// todo, dont use memcpy
				if (fields[i].array_size > 1)
				{
					output += "  memcpy(start, msg->" + fields[i].name + ", sizeof(" + fields[i].getBaseType() + ")*" + std::to_string(fields[i].array_size) + ");\n";
					output += "  start += sizeof("+ fields[i].getBaseType() + ")*" + std::to_string(fields[i].array_size) + ";\n";
				}
				else
				{
					output += "  memcpy(start, &msg->" + fields[i].name + ", sizeof(" + fields[i].getBaseType() + "));\n";
					output += "  start += sizeof(" + fields[i].getBaseType() + ");\n";
				}
			}
		}
		output += "  return omsg;\n";
		output += "}\n";
	}

	// generate the actual message definition
	int field_count = fields.size();
	for (auto& f: fields)
	{
		field_count += f.type->fields.size();
	}
	output += "struct ps_message_definition_t " + type_name + "_def = { ";
	if (enumerations.size() == 0)
	{
		output += std::to_string(hash) + ", \"" + name + "\", " + std::to_string(field_count) + ", " + type_name + "_fields, " + type_name + "_encode, " + type_name + "_decode, 0, 0 };\n";
	}
	else
	{
		output += std::to_string(hash) + ", \"" + name + "\", " + std::to_string(field_count) + ", " + type_name + "_fields, " + type_name + "_encode, " + type_name + "_decode, " + std::to_string(enumerations.size()) + ", " + type_name + "_enums };\n";
	}

	output += "\n#ifdef __cplusplus\n";
	output += "#include <memory>\n";
	output += "#include <string>\n";
	output += "#include <vector>\n";
	//output += "#include <iostream>\n";
	output += "#include <pubsub_cpp/array_vector.h>\n";
	output += "namespace " + ns + "\n{\n";
    output += "namespace msg\n{\n";
	output += "#pragma pack(push, 1)\n";
	output += "struct " + raw_name + "\n{\n";

	for (auto f: fields)
	{
		std::string type = f.type == string_type ? "char*" : f.getBaseType();
		if (f.array_size == 1)
		{
			output += "  " + type + " " + f.name + ";\n";
		}
		else if (f.array_size == 0)
		{
			output += "  ArrayVector<" + type + "> " + f.name + ";\n";
		}
		else
		{
			output += "  " + type + " " + f.name + "["+std::to_string(f.array_size) + "];\n";
		}
	}
	output += "\n";
	
	// add any enumerations
	if (enumerations.size() > 0)
	{
		output += "  enum\n  {\n";
		for (int i = 0; i < enumerations.size(); i++)
		{
			auto en = enumerations[i];
			output += "    ";
			output += en.name;
			output += " = ";
			output += en.value;
			if (i != enumerations.size() - 1)
			{
				output += ",";
			}
			output += "\n";
		}
		output += "  };\n\n";
	}

	/*output += "  ~" + raw_name + "()\n  {\n";
	output += "std::cout <<\"destructor\\n\";\n";
	output += "  }\n\n";

	output += "  " + raw_name + "()\n  {\n";
	output += "std::cout <<\"constructor\\n\";\n";
	output += "  }\n\n";*/

	// generate new and delete that use malloc/free to interop correctly with our C allocation
	output += "  void* operator new(size_t size)\n";
    output += "  {\n";
    //output += "    std::cout<< \"Overloading new operator with size: \" << size << std::endl;\n";
    output += "    return malloc(size);\n";
    output += "  }\n\n";
 
    output += "  void operator delete(void * p)\n";
    output += "  {\n";
    //output += "    std::cout<< \"Overloading delete operator \" << std::endl;\n";
    output += "    free(p);\n";
    output += "  }\n\n";

	output += "  static const ps_message_definition_t* GetDefinition()\n  {\n";
	output += "    return &" + type_name + "_def;\n  }\n\n";
	output += "  ps_msg_t Encode() const\n  {\n";
	output += "    return " + ns + "__" + raw_name + "_encode(&ps_default_allocator, this);\n  }\n\n";
	output += "  static " + raw_name + "* Decode(const void* data)\n  {\n";
	output += "    return (" + raw_name + "*)" + ns + "__" + raw_name + "_decode(data, &ps_default_allocator);\n  }\n";// + ns + "__" + raw_name + "_encode(0, this);\n  }\n";
	output += "};\n";
	output += "#pragma pack(pop)\n";
	output += "typedef std::shared_ptr<" + raw_name + "> " + raw_name + "SharedPtr;\n";
	
	output += "}\n";
	output += "}\n";
	// add any enumerations for C
	if (enumerations.size() > 0)
	{
		output += "#else\n";
		auto upper_raw_name = raw_name;
		std::transform(upper_raw_name.begin(), upper_raw_name.end(), upper_raw_name.begin(), ::toupper);
		for (int i = 0; i < enumerations.size(); i++)
		{
			auto en = enumerations[i];
			output += "#define ";
			output += upper_raw_name;
			output += "_";
			output += en.name;
			output += " ";
			output += en.value;
			output += "\n";
		}
	}
	output += "#endif\n";
	//printf("Output:\n%s", output.c_str());

	return output;
}
#include <stdio.h>  /* defines FILENAME_MAX */
//#define WINDOWS  /* uncomment this line to use it for windows.*/
#ifdef WIN32
#include <direct.h>
#define GetCurrentDir _getcwd
#else
#include <unistd.h>
#define GetCurrentDir getcwd
#endif

#include <string>
#include <fstream>
#include <streambuf>
int main(int num_args, char** args)
{
	//ok, so this reads in a list of files then converts those all to header files which define serialization and the
	//message definition

	// takes in first, the message file, then the message name w/ namespace, then file to output to
	// ex: string.msg std_msgs/String
	/*num_args = 4;
	args[1] = "C:\\Users\\mbries\\Desktop\\pubsub\\msg\\Pose.msg";
	args[2] = "pubsub";
	args[3] = "Pose.msg.h";*/
    if ((num_args-1)%3 != 0)
    {
        printf("Not enough arguments!");
        return -1;
    }

	// okay, read in each message file then lets generate things for it
	for (int i = 1; i < num_args; i+=3)
	{
		//printf("%s\n", args[i]);
		std::ifstream t(args[i], std::ios::binary);
		std::string str;

		t.seekg(0, std::ios::end);
		str.reserve(t.tellg());
		t.seekg(0, std::ios::beg);

		str.assign((std::istreambuf_iterator<char>(t)),
			std::istreambuf_iterator<char>());

		// remove a BOM if there is one
		if (str.length() >= 3 && str[0] == '\357'
			&& str[1] == '\273' && str[2] == '\277')
		{
			str[0] = ' ';
			str[1] = ' ';
			str[2] = ' ';
		}

		//str = str.substr(3);

		//printf("%s\n", str.c_str());

		std::string file_name = args[i];
		for (size_t i = 0; i < file_name.size(); i++)
		{
			if (file_name[i] == '\\')
			{
				file_name[i] = '/';
			}
		}
		file_name = file_name.substr(file_name.find_last_of('/') + 1);
		// remove everything after the .
		std::string name = std::string(args[i+1]) + "__" + file_name.substr(0, file_name.find_first_of('.'));

		char current_directory[FILENAME_MAX];
		GetCurrentDir(current_directory, FILENAME_MAX);

		try
		{
			_current_file = current_directory;
			_current_file += "/";
			_current_file += args[i];
			std::string output = generate(str.c_str(), name.c_str());

			// write it back to file
			std::string out_name = args[i+2];
			std::ofstream o(out_name, std::ios::binary);
			if (o.is_open() == false)
			{
				printf("Failed to create output file %s\n", out_name.c_str());
				return -1;
			}
			o << output;
		}
		catch (int ex)
		{
			printf("Message generation failed.\n");
			return -1;
		}
	}

	/*int num_messages = 5;

	const char* test = "string data";

	const char* test2 = "int data1[5]\nstring data2";

	const char* test3 = "int buttons\nfloat axes[4]";
	const char* test4 = "int a";
	generate(test, "std_msgs/String");
	generate(test2, "std_msgs/Test");
	generate(test3, "joy_msgs/Joy");

	generate(test4, "std_msgs/Int");*/

	//const char* test = "int32 width\r\nint32 height\r\nuint8 data[]";
	//generate(test, "okay/Image");
	//getchar();
	return 0;
}
