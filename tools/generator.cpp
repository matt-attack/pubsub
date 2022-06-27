#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <map>

#include <vector>
#include <string>
#include <sstream>

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

struct field
{
	std::string name;
	std::string type;

	int array_size;
	
	std::string flag;

	std::string serialize()
	{

	}

	std::string getBaseType()
	{
		if (type == "int64")
		{
			return "int64_t";
		}
		if (type == "int32")
		{
			return "int32_t";
		}
		if (type == "int16")
		{
			return "int16_t";
		}
		if (type == "int8")
		{
			return "int8_t";
		}
		if (type == "uint64")
		{
			return "uint64_t";
		}
		if (type == "uint32")
		{
			return "uint32_t";
		}
		if (type == "uint16")
		{
			return "uint16_t";
		}
		if (type == "uint8")
		{
			return "uint8_t";
		}
		if (type == "string")
		{
			// probably should revamp this later
			return "char*";
		}
		if (type == "float")
		{
			return "float";
		}
		if (type == "double")
		{
			return "double";
		}

		return "invalid";
	}
	
	std::string getFlags()
	{
		if (flag == "enum")
			return "FF_ENUM";
		if (flag == "bitmask")
			return "FF_BITMASK";
		if (flag == "")
			return "FF_NONE";
		
		printf("ERROR: Invalid flag %s\n", flag.c_str());
		return "invalid";
	}

	std::string getTypeEnum()
	{
		if (type == "int64")
		{
			return "FT_Int64";
		}
		if (type == "int32")
		{
			return "FT_Int32";
		}
		if (type == "int16")
		{
			return "FT_Int16";
		}
		if (type == "int8")
		{
			return "FT_Int8";
		}
		if (type == "uint64")
		{
			return "FT_UInt64";
		}
		if (type == "uint32")
		{
			return "FT_UInt32";
		}
		if (type == "uint16")
		{
			return "FT_UInt16";
		}
		if (type == "uint8")
		{
			return "FT_UInt8";
		}
		if (type == "string")
		{
			// probably should revamp this later
			return "FT_String";
		}
		if (type == "float")
		{
			return "FT_Float32";
		}
		if (type == "double")
		{
			return "FT_Float64";
		}
		printf("ERROR: Invalid type %s\n", type.c_str());
		return "invalid";
	}
	
	void GenerateFree(std::string& output)
	{
		if (type == "string" && array_size != 1)
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
		
		if (type == "string" || array_size != 1)
		{
			output += "    if (this->" + name + ")\n";
			output += "      free(this->" + name + ");\n";
		}
	}
	
	void GenerateCopy(std::string& output, const std::string& source)
	{
		if (type == "string")
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
	// todo, also remove extra spacing
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

	// also generate the has while we are at it
	uint32_t hash = 0;
	for (auto& line : lines)
	{
		for (unsigned int i = 0; i < line.length(); i++)
		{
			hash *= std::abs(line[i]);
			hash += i;
		}

		//okay so each of these should contain a type, then a field
		auto words = split(line, ' ');
		if (words.size() == 0)
		{
			continue;
		}
		else if (words.size() == 2)
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
			// also fill in array size
			fields.push_back({ name, type, size});
		}
		// a line with flags maybe?
		else if (words.size() == 3)
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
			// also fill in array size
			fields.push_back({ name, type, size, flag});
		}
		else
		{
			// this is either an enum or an error
			auto equals = split(line, '=');
			if (equals.size() == 2)
			{
				// its an enum!
				//printf("it was an enum: %s=%s\n", equals[0].c_str(), equals[1].c_str());
				enumerations.push_back({equals[0], equals[1], (int)fields.size()});
			}
			else
			{
				printf("WARNING: Unexpectedly formatted line in message.\n");
			}
		}
	}

	for (auto field : fields)
	{
		//printf("Got field '%s' of type %s\n", field.name.c_str(), field.type.c_str());
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
		output += "  { " + field.getTypeEnum() + ", " + field.getFlags() + ", \"" + field.name + "\", ";
		output += std::to_string(field.array_size) + ", 0 }, \n";// todo use for array types
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
		if (field.type == "string")
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
		output += "  struct " + type_name + "* out = (struct " + type_name + "*)allocator->alloc(sizeof(" + type_name + "), allocator->context);\n";
		output += "  *out = *(" + type_name + "*)data;\n";
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
			if (fields[i].type == "string")
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
		int n_str = 0;
		for (size_t i = 0; i < fields.size(); i++)
		{
			if (fields[i].type == "string")
			{
				if (fields[i].array_size == 1)
				{
					output += "  int len_" + fields[i].name + " = strlen(msg->" + fields[i].name + ") + 1; \n";
					output += "  len += len_" + fields[i].name + ";\n";
					n_str++;
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
					n_str++;
				}
			}
			else if (fields[i].array_size == 0)
			{
				output += "  len += msg->" + fields[i].name + "_length*sizeof(" + fields[i].getBaseType() + ");\n";
				n_str++;
			}
		}
		output += "  len -= " + std::to_string(n_str) + "*sizeof(char*);\n";
		output += "  struct ps_msg_t omsg;\n";
		output += "  ps_msg_alloc(len, allocator, &omsg);\n";
		output += "  char* start = (char*)ps_get_msg_start(omsg.data);\n";
		for (size_t i = 0; i < fields.size(); i++)
		{
			if (fields[i].type == "string")
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
	output += "namespace " + ns + "\n{\n";
    output += "namespace msg\n{\n";
	output += "struct " + raw_name + ": public " + type_name + "\n{\n";
	
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
	
	// create destructor
	bool needs_destructor = false;
	for (int i = 0; i < fields.size(); i++)
	{
		if (fields[i].type == "string" || fields[i].array_size == 0)
		{
			needs_destructor = true;
		}
	}
	
	if (needs_destructor)
	{
		output += "  ~" + raw_name + "()\n  {\n";
		for (int i = 0; i < fields.size(); i++)
		{
			fields[i].GenerateFree(output);
		}
		output += "  }\n\n";

		// add a copy constructor and constructor now...
		output += "  " + raw_name + "(const " + raw_name + "& obj)\n  {\n";
		for (size_t i = 0; i < fields.size(); i++)
		{
			fields[i].GenerateCopy(output, "obj");
		}
		output += "  }\n\n";
		// todo maybe just use stl types for strings/arrays?

		output += "  " + raw_name + "& operator=(const " + raw_name + "& obj)\n  {\n";
		for (int i = 0; i < fields.size(); i++)
		{
			fields[i].GenerateFree(output);
		}
		output += "\n";
		for (size_t i = 0; i < fields.size(); i++)
		{
			fields[i].GenerateCopy(output, "obj");
		}
		output += "    return *this;\n  }\n\n";

		// mark all pointers as null to avoid crashes
		output += "  " + raw_name + "()\n  {\n";
		for (size_t i = 0; i < fields.size(); i++)
		{
			if (fields[i].type == "string")
			{
				output += "    " + fields[i].name + " = 0;\n";
				if (fields[i].array_size == 0)
				{
					output += "    " + fields[i].name + "_length = 0;\n";
				}
			}
			else if (fields[i].array_size == 0)
			{
				output += "    " + fields[i].name + " = 0;\n";
				output += "    " + fields[i].name + "_length = 0;\n";
			}
		}
		output += "  }\n\n";
	}
	output += "  static const ps_message_definition_t* GetDefinition()\n  {\n";
	output += "    return &" + type_name + "_def;\n  }\n\n";
	output += "  ps_msg_t Encode() const\n  {\n";
	output += "    return " + ns + "__" + raw_name + "_encode(&ps_default_allocator, this);\n  }\n\n";
	output += "  static " + raw_name + "* Decode(const void* data)\n  {\n";
	output += "    return (" + raw_name + "*)" + ns + "__" + raw_name + "_decode(data, &ps_default_allocator);\n  }\n";// + ns + "__" + raw_name + "_encode(0, this);\n  }\n";
	output += "};\n";
	output += "typedef std::shared_ptr<" + raw_name + "> " + raw_name + "SharedPtr;\n";
	output += "}\n";
	output += "}\n";
	// add any enumerations for C
	if (enumerations.size() > 0)
	{
		output += "#else\n";
		for (int i = 0; i < enumerations.size(); i++)
		{
			auto en = enumerations[i];
			output += "#define ";
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
		std::string output = generate(str.c_str(), name.c_str());

		// write it back to file
		std::string out_name = args[i+2];
		std::ofstream o(out_name, std::ios::binary);
		o << output;
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
