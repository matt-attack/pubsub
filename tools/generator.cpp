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

		return "invalid";
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

		return "invalid";
	}
};

std::string generate(const char* definition, const char* name)
{
	std::string output;

	// first parse out the fields
	std::vector<field> fields;
	
	auto lines = split(definition, '\n');

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
	}

	// also generate the has while we are at it
	uint32_t hash = 0;
	for (auto& line : lines)
	{
		for (int i = 0; i < line.length(); i++)
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

	// add header guard
	output += "#pragma once\n\n";

	output += "#include <stdint.h>\n\n";

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
		output += "void* " + type_name + "_decode(const void* data, ps_allocator_t* allocator)\n{\n";
		output += "  " + type_name + "* out = (" + type_name + "*)allocator->alloc(sizeof(" + type_name + "), allocator->context);\n";
		output += "  *out = *(" + type_name + "*)data;\n";
		output += "  return out;\n";
		output += "\n}\n\n";

		// now for encode
		output += "ps_msg_t " + type_name + "_encode(ps_allocator_t* allocator, const void* msg)\n{\n";
		output += "  int len = sizeof(" + type_name + ");\n";
		output += "  ps_msg_t omsg;\n";
		output += "  ps_msg_alloc(len, &omsg);\n";
		output += "  memcpy(ps_get_msg_start(omsg.data), msg, len);\n";
		output += "  return omsg;\n}\n";
	}
	else
	{
		//need to split it in sections between the strings
		output += "void* " + type_name + "_decode(const void* data, ps_allocator_t* allocator)\n{\n";
		output += "  char* p = (char*)data;\n";
		output += "  int len = sizeof("+type_name+");\n";
		for (int i = 0; i < fields.size(); i++)
		{
			if (fields[i].type == "string")
			{
				output += "  len -= sizeof(char*);\n";
				output += "  int len_" + fields[i].name + " = strlen(p) + 1; \n";
				output += "  len += len_" + fields[i].name + ";\n";
				output += "  p += len_" + fields[i].name + ";\n";
			}
			else
			{
				output += "  p += sizeof(" + fields[i].getBaseType() + ");\n";
			}
		}
		output += "  p = (char*)data;\n";// start from beginning again
		output += "  "+type_name+"* out = (" + type_name + "*)allocator->alloc(len, allocator->context);\n";
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
				output += "  out->" + fields[i].name + " = (char*)allocator->alloc(len_" + fields[i].name + ", allocator->context);\n";
				output += "  memcpy(out->" + fields[i].name + ", p, len_" + fields[i].name + ");\n";// is memcpy faster in this case?
				output += "  p += len_" + fields[i].name + ";\n";
			}
		}
		output += "  return (void*)out;\n";
		output += "}\n\n";

		//typedef ps_msg_t(*ps_fn_encode_t)(ps_allocator_t* allocator, const void* msg);
		output += "ps_msg_t " + type_name + "_encode(ps_allocator_t* allocator, const void* data)\n{\n";
		output += "  const " + type_name + "* msg = (const " + type_name + "*)data;\n";
		output += "  int len = sizeof(" + type_name + ");\n";
		output += "  // for each string, add their length\n";
		int n_str = 0;
		for (int i = 0; i < fields.size(); i++)
		{
			if (fields[i].type == "string")
			{
				output += "  int len_" + fields[i].name + " = strlen(msg->" + fields[i].name + ") + 1; \n";
				output += "  len += len_" + fields[i].name + ";\n";
				n_str++;
			}
		}
		output += "  len -= " + std::to_string(n_str) + "*sizeof(char*);\n";
		output += "  ps_msg_t omsg;\n";
		output += "  ps_msg_alloc(len, &omsg);\n";
		output += "  char* start = (char*)ps_get_msg_start(omsg.data);\n";
		for (int i = 0; i < fields.size(); i++)
		{
			if (fields[i].type == "string")
			{
				output += "  strcpy(start, msg->" + fields[i].name + ");\n";
				output += "  start += len_" + fields[i].name + ";\n";
			}
			else
			{
				// todo, dont use memcpy
				output += "  memcpy(start, &msg->" + fields[i].name + ", sizeof(" + fields[i].getBaseType() + "));\n";
				output += "  start += sizeof(" + fields[i].getBaseType() + ");\n";
			}
		}
		output += "  return omsg;\n";
		output += "}\n";
	}

	// generate the actual message definition
	int field_count = fields.size();
	output += "ps_message_definition_t " + type_name + "_def = { ";
	output += std::to_string(hash) + ", \"" + name + "\", " + std::to_string(field_count) + ", " + type_name + "_fields, " + type_name + "_encode, " + type_name + "_decode };\n";


	printf("Output:\n%s", output.c_str());

	return output;
}


#include <string>
#include <fstream>
#include <streambuf>
void main(int num_args, char** args)
{
	//ok, so this reads in a list of files then converts those all to header files which define serialization and the
	//message definition

	// takes in first, the message file, then the message name w/ namespace
	// ex: string.msg std_msgs/String
	//num_args = 2;
	//args[1] = "C:\\Users\\space\\Desktop\\pubsub_proto\\PubSub\\msg\\std_msgs__Joy.txt";

	// okay, read in each message file then lets generate things for it
	for (int i = 1; i < num_args; i++)
	{
		printf("%s\n", args[i]);
		std::ifstream t(args[i], std::ios::binary);
		std::string str;

		t.seekg(0, std::ios::end);
		str.reserve(t.tellg());
		t.seekg(3, std::ios::beg);

		str.assign((std::istreambuf_iterator<char>(t)),
			std::istreambuf_iterator<char>());

		// remove a BOM if there is one
		/*if (str.length() >= 3 && str[0] == 0xEF
			&& str[1] == 0xBB && str[2] == 0xBF)
		{
			//str[0] = ' ';
			//str[1] = ' ';
			//str[2] = ' ';
		}*/
		printf("%s\n", str.c_str());

		std::string file_name = args[i];
		for (int i = 0; i < file_name.size(); i++)
		{
			if (file_name[i] == '\\')
			{
				file_name[i] = '/';
			}
		}
		file_name = file_name.substr(file_name.find_last_of('/') + 1);
		// remove everything after the .
		std::string name = file_name.substr(0, file_name.find_first_of('.'));
		std::string output = generate(str.c_str(), name.c_str());

		// write it back to file
		std::string out_name = args[i];
		out_name += ".h";
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
	//getchar();
}