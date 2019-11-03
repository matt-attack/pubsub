#include <stdio.h>
#include <stdlib.h>

#include <map>



/*
// generation test
ps_field_t std_msgs_String_fields[] = { { FT_String, "value", 1, 0 } };
ps_message_definition_t std_msgs_String_def = { 123456789, "std_msgs/String", 1, std_msgs_String_fields};
struct std_msgs_String
{
	char* value;
};

// todo, this might have a data lifetime issue
void std_msgs_String_decode(const void* data, std_msgs_String* out)
{
	out->value = (char*)data;
}

ps_msg_t std_msgs_String_encode(const std_msgs_String* msg)
{
	int len = strlen(msg->value) + 1;
	ps_msg_t omsg;
	ps_msg_alloc(len, &omsg);
	memcpy(ps_get_msg_start(omsg.data), msg->value, len);
	return omsg;
}
*/

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

struct field
{
	std::string name;
	std::string type;

	int array_size;

	std::string serialize()
	{

	}

	std::string getBaseType()
	{
		if (type == "int")
		{
			return "int32_t";
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
	}

	std::string getTypeEnum()
	{
		if (type == "int")
		{
			return "FT_Int32";
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
	}
};

void generate(const char* definition, const char* name)
{
	std::string output;

	// first parse out the fields
	std::vector<field> fields;
	
	auto lines = split(definition, '\n');

	// todo remove comments
	for (auto& line : lines)
	{
		//okay so each of these should contain a type, then a field
		auto words = split(line, ' ');
		if (words.size() == 0)
		{
			continue;
		}
		else if (words.size() >= 2)
		{
			std::string name = words[1];
			std::string type = words[0];

			int size = 1;
			if (name.find('[') != -1)
			{
				int index = name.find('[');
				size = std::stoi(name.substr(index + 1));
				name = name.substr(0, index);
			}
			// also fill in array size
			fields.push_back({ name, type, size});
		}
	}

	for (auto field : fields)
	{
		printf("Got field %s of type %s\n", field.name.c_str(), field.type.c_str());
	}

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

	// now that we have these, generate stuff!, start with the struct
	output += "struct " + type_name + "\n{\n";
	for (auto& field : fields)
	{
		if (field.array_size > 1)
		{
			output += "  " + field.getBaseType() + " " + field.name + "[" + std::to_string(field.array_size) + "];\n";
		}
		else
		{
			output += "  " + field.getBaseType() + " " + field.name + ";\n";
		}
	}
	output += "};\n\n";

	// then generate the message definition data
	//ps_field_t std_msgs_String_fields[] = { { FT_String, "value", 1, 0 } };
	//ps_message_definition_t std_msgs_String_def = { 123456789, "std_msgs/String", 1, std_msgs_String_fields };

	// generate the fields
	output += "ps_field_t " + type_name + "_fields[] = {\n";
	for (auto& field : fields)
	{
		output += "  { " + field.getTypeEnum() + ", \"" + field.name + "\", ";
		output += std::to_string(field.array_size) + ", 0 }, \n";// todo use for array types
	}
	output += "};\n\n";

	// generate the actual message definition
	int field_count = fields.size();
	int hash = 123456789;
	output += "ps_message_definition_t " + type_name + "_def = { ";
	output += std::to_string(hash) + ", \"" + name + "\", " + std::to_string(field_count) + ", " + type_name +"_fields };\n";

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
	// for now, just check for a string
	for (auto& field : fields)
	{
		if (field.type == "string")
		{
			is_pure = false;
			break;
		}
	}

	if (is_pure)
	{
		//generate simple de/serializaton
		output += "void " + type_name + "_decode(const void* data, "+ type_name + "* out)\n{\n";
		output += "  *out = *(" + type_name + "*)data;\n}\n\n";

		// now for encode
		output += "ps_msg_t " + type_name + "_encode(void* allocator, const " + type_name + "* msg)\n{\n";
		output += "  int len = sizeof(" + type_name + ");\n";
		output += "  ps_msg_t omsg;\n";
		output += "  ps_msg_alloc(len, &omsg);\n";
		output += "  memcpy(ps_get_msg_start(omsg.data), msg, len);\n";
		output += "  return omsg;\n}\n";
	}
	else
	{
		//need to split it in sections between the strings
		output += "void " + type_name + "_decode(const void* data, " + type_name + "* out)\n{\n  char* p = data;\n";
		// for now lets just decode non strings
		for (int i = 0; i < fields.size(); i++)
		{
			if (fields[i].type != "string")
			{
				output += "  out->" + fields[i].name + " = *((" + fields[i].getBaseType() + "*)p);\n";
				output += "  p += sizeof(" + fields[i].getBaseType() + ");\n";
			}
			else
			{
				// need to allocate the string somehow...
				output += "  int len_" + fields[i].name + " = strlen(p); \n";
				output += "  out->" + fields[i].name + " = malloc(len_" + fields[i].name + " + 1);\n";
				output += "  strcpy(out->" + fields[i].name + ", p);\n";
			}
		}
		output += "}\n";

		output += "ps_msg_t " + type_name + "_encode(void* allocator, const " + type_name + "* msg)\n{\n";
		output += "  int len = sizeof(" + type_name + ");\n";
		output += "  // for each string, add their length\n";
		int n_str = 0;
		for (int i = 0; i < fields.size(); i++)
		{
			if (fields[i].type == "string")
			{
				output += "  int len_" + fields[i].name + " = strlen(msg->" + fields[i].name + "); \n";
				output += "  len += len_" + fields[i].name + ";\n";
				n_str++;
			}
		}
		output += "  len -= " + std::to_string(n_str) + "*sizeof(char*);\n";
		output += "  ps_msg_t omsg;\n";
		output += "  ps_msg_alloc(len, &omsg);\n";
		output += "  char* start = ps_get_msg_start(omsg.data);\n";
		for (int i = 0; i < fields.size(); i++)
		{
			if (fields[i].type == "string")
			{
				output += "  strcpy(start, msg->" + fields[i].name + ");\n";
				output += "  start += len_" + fields[i].name + " + 1;\n";
			}
			else
			{
				// todo, dont use memcpy
				output += "  memcpy(start, &msg->" + fields[i].name + ", sizeof(" + fields[i].getBaseType() + "));\n";
				output += "  start += sizeof(" + fields[i].getBaseType() + ");\n";
			}
		}
		output += "}\n";
	}


	printf("Output:\n%s", output.c_str());
}

void main(int num_args, char** args)
{
	//ok, so this reads in a list of files then converts those all to header files which define serialization and the
	//message definition

	// takes in first, the message file, then the message name w/ namespace
	// ex: string.msg std_msgs/String

	// okay, read in each message file then lets generate things for it

	int num_messages = 5;

	const char* test = "string data";

	const char* test2 = "int data1[5]\nstring data2";

	const char* test3 = "int buttons\nfloat axes[4]";
	generate(test, "std_msgs/String");
	generate(test2, "std_msgs/Test");
	generate(test3, "joy_msgs/Joy");
	getchar();
}