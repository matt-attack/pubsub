#pragma once

#include "Parse.h"

#include <string.h>

// handles serialization for values from Parse
int ps_field_sizes[] = { 1,2,4,8,1,2,4,8,8,4,8,4,0,0,0 };
ps_msg_t serialize_value(const Value& value, const ps_message_definition_t& definition)
{
	ps_msg_t msg;

	// okay, lets do a mapping of values to fields
	std::map<std::string, Value> mapping;
	if (value.type == Map)
	{
		if (value.map.size() == 1 && definition.num_fields == 1)
		{
			//just map it and warn
			if (value.map.begin()->first != definition.fields[0].name)
			{
				printf("Warning: field name %s doesn't match message which has field %s.\n",
					value.map.begin()->first.c_str(),
					definition.fields[0].name);
			}
			mapping[definition.fields[0].name] = value.map.begin()->second;
		}
		else if (value.map.size() <= definition.num_fields)
		{
			// do the mapping yo and warn for anything left
			for (auto& item : value.map)
			{
				if (item.first[0] >= '0' && item.first[0] <= '9')
				{
					// give it to the nth item of the map
					int num = std::atoi(item.first.c_str());
					mapping[definition.fields[num].name] = item.second;
				}
				else
				{
					// this should be safe
					mapping[item.first] = item.second;
				}
			}
		}
		else
		{
			throw std::string("More fields in message than expected.");
		}
	}
	else if (definition.num_fields == 1)
	{
		mapping[definition.fields[0].name] = value;
	}
	else
	{
		throw std::string("Unexpected number of fields");
	}

	//okay, now serialize

	// first iterate through and calculate size
	unsigned int message_size = 0;
	for (unsigned int i = 0; i < definition.num_fields; i++)
	{
		auto& field = definition.fields[i];
		Value& value = mapping[field.name];
		if (field.type == FT_String)
		{
			if (value.type == None)
			{
				value = Value("");
				message_size += 1;
			}
			else if (value.type == String)
			{
				message_size += 1 + value.str.length();
			}
			else
			{
				// for now lets throw
				throw std::string("Cannot map value to string");
			}
		}
		else if (field.type < FT_MaxFloat)
		{
			// fixed size
			int size = ps_field_sizes[field.type];
			if (field.length >= 1)
			{
				// this is fine
				
				message_size += size * field.length;
			}
			else if (field.length == 0)
			{
				// dynamic array
				message_size += size * value.arr.size() + 4;
			}
			else if (value.type != Number && value.type != None)
			{
				throw std::string("Cannot map value to number");
			}
		}
		else if (field.type == FT_Struct)
		{
			//unhandled for now
			throw std::string("Unhandled field type");
		}
		else
		{
			//unhandled
			throw std::string("Unhandled field type");
		}
	}

	//allocate message
	//printf("Allocated %i bytes for message\n", message_size);
	ps_msg_alloc(message_size, 0, &msg);

	// finally serialize
	char* pos = (char*)ps_get_msg_start(msg.data);
	char* start = pos;
	for (unsigned int i = 0; i < definition.num_fields; i++)
	{
		auto& field = definition.fields[i];
		const Value& value = mapping[field.name];
		if (field.type == FT_String)
		{
			memcpy(pos, value.str.c_str(), value.str.length() + 1);
			pos += value.str.length() + 1;
		}
		else if (field.type < FT_MaxInteger)
		{
			// kinda lazy hacky atm
			int size = ps_field_sizes[field.type];
			long long int data = value.flt;
			long long int max = 1;
			max <<= size * 8;

			// check range
			if (data >= max)
			{
				printf("WARNING: field is out of range for its type\n");
			}

			// write the length if necessary
			if (field.length == 0)
			{
				uint32_t len = 0;
				memcpy(pos, &len, 4);
				pos += 4;
			}

			if (value.type == Array)
			{

			}
			else
			{
				
			}

			//need to handle arrays and write the correct length

			memcpy(pos, &data, size);
			pos += size;
		}
		else if (field.type == FT_Float32)
		{
			if (field.length == 0)
			{
				uint32_t len = 0;
				memcpy(pos, &len, 4);
				pos += 4;
			}

			// do da float
			if (value.type != Array)
			{
				// just write the same value n times
				size_t size = ps_field_sizes[field.type];
				float data = value.type == None ? 0.0 : value.flt;
				for (unsigned int i = 0; i < field.length; i++)
				{
					memcpy(pos, &data, size);
					pos += size;
				}
			}
			else
			{
				// fill it in using the array
				// just write the same value n times
				size_t size = ps_field_sizes[field.type];
				float data = value.type == None ? 0.0 : value.flt;
				size_t len = field.length == 0 ? value.arr.size() : field.length;
				for (unsigned int i = 0; i < len; i++)
				{
					float data = i < value.arr.size() ? value.arr[i].flt : 0.0;
					memcpy(pos, &data, size);
					pos += size;
				}
			}
		}
		else if (field.type == FT_Float64)
		{
			if (value.type == None)
			{
				// just pad it
				if (field.length == 0)
				{
					uint32_t len = 0;
					memcpy(pos, &len, 4);
					pos += 4;
				}
				auto len = field.length*ps_field_sizes[field.type];
				memset(pos, 0, len);
				pos += len;
			}
			else if (value.type == Number)
			{
				if (field.length != 1)
				{
					throw std::string("Cannot provide a number for an array.");
				}

				double v = value.flt;
				memcpy(pos, &v, 8);
				v += 8;
			}
			else if (value.type == Array)
			{
				uint32_t len = field.length;
				if (field.length == 0)
				{
					len = value.arr.size();
					memcpy(pos, &len, 4);
					pos += 4;
				}
                //printf("len: %i at %i\n", len, pos-start);
				size_t size = ps_field_sizes[field.type];
				for (size_t j = 0; j < len; j++)
				{
					double data = j < value.arr.size() ? value.arr[j].flt : 0.0;
					memcpy(pos, &data, 8);
					pos += 8;
				}
			}
			else
			{
				throw std::string("unhandled type");
			}
		}
		else
		{
			throw std::string("Unhandled field type");
		}
	}
	//printf("totlen: %i\n", pos-start);

	printf("Publishing The Message Below:\n");
	ps_deserialize_print(ps_get_msg_start(msg.data), &definition, 0, 0);

	return msg;
}
