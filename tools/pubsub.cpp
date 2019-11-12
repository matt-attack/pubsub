// PubSubCMake.cpp : Defines the entry point for the application.
//

#include "src/Node.h"
#include "src/Publisher.h"
#include "src/Subscriber.h"
#include "src/Serialization.h"

#include <stdio.h>

#include <string>

using namespace std;

void ps_msg_alloc(unsigned int size, ps_msg_t* out_msg)
{
	out_msg->len = size;
	out_msg->data = (void*)((char*)malloc(size + sizeof(ps_msg_header)));
}

void print_help()
{
	printf("Usage: pubsub <verb> <subverb> (arg1) (arg2)\n"
		" Verbs:\n"
		"   topic -\n"
		"      info (topic name)\n");
}

#include <Windows.h>

void wait(ps_node_t* node)
{
	printf("Waiting for responses...\n\n");
	int start = GetTickCount();
	while (ps_okay() && start + 3000 > GetTickCount())
	{
		ps_node_spin(node);
	}
}

#include "src/Parse.h"
#include <iostream>
#include <map>
#include <vector>
struct Topic
{
	std::string type;

	std::vector<std::string> subscribers;
	std::vector<std::string> publishers;
};


ps_message_definition_t definition = { 0, 0 };

int ps_field_sizes[] = { 1,2,4,8,1,2,4,8,8,4,8,4,0,0,0 };
ps_msg_t serialize_value(const Value& value)
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
	int message_size = 0;
	for (int i = 0; i < definition.num_fields; i++)
	{
		auto& field = definition.fields[i];
		Value& value = mapping[field.name];// its okay if it doesnt exist, it gets filled with Value()

		// now serialize based on the field type
		// change it to the prefered type if necessary
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
			if (value.type == Map && field.length > 1)
			{
				// this is fine
			}
			else if (value.type != Number && value.type != None)
			{
				throw std::string("Cannot map value to number");
			}
			message_size += size*field.length;
		}
		else if (field.type == FT_Array)
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
	ps_msg_alloc(message_size, &msg);

	// finally serialize
	char* pos = (char*)ps_get_msg_start(msg.data);
	for (int i = 0; i < definition.num_fields; i++)
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
			long int data = value.flt;
			long long int max = 1;
			max <<= size * 8;

			// check range
			if (data >= max)
			{
				printf("WARNING: field is out of range for its type");
			}

			memcpy(pos, &data, size);
			pos += size;
		}
		else if (field.type == FT_Float32)
		{
			// do da float
			if (value.type != Map)
			{
				// just write the same value n times
				int size = ps_field_sizes[field.type];
				float data = value.type == None ? 0.0 : value.flt;
				for (int i = 0; i < field.length; i++)
				{
					memcpy(pos, &data, size);
					pos += size;
				}
			}
		}
		else if (field.type == FT_Float64)
		{
			// do da float
			if (value.type != Map)
			{
				// just write the same value n times
				int size = ps_field_sizes[field.type];
				double data = value.type == None ? 0.0 : value.flt;
				for (int i = 0; i < field.length; i++)
				{
					memcpy(pos, &data, size);
					pos += size;
				}
			}
		}
		else
		{
			throw std::string("Unhandled field type");
		}
	}

	ps_deserialize_print(ps_get_msg_start(msg.data), &definition);

	return msg;
}

std::map<std::string, Topic> _topics;


int main(int num_args, char** aargs)
{
	ps_node_t node;
	ps_node_init(&node, "Query");

	// for running tests
	num_args = 5;
	char* args[] = { "aaa", "topic", "pub", "/joy", "{buttons:32}" };

	node.adv_cb = [](const char* topic, const char* type, const char* node, void* data)
	{
		auto t = _topics.find(topic);
		if (t == _topics.end())
		{
			Topic t;
			t.type = type;
			t.publishers.push_back(node);
			_topics[topic] = t;
		}
		else
		{
			auto& t = _topics[topic];
			for (int i = 0; i < t.publishers.size(); i++)
			{
				if (t.publishers[i] == node)
					return;
			}
			t.publishers.push_back(node);
		}
		//printf("Get pub %s type %s node %s\n", topic, type, node);
		return;
	};

	node.sub_cb = [](const char* topic, const char* type, const char* node, void* data)
	{
		auto t = _topics.find(topic);
		if (t == _topics.end())
		{
			Topic t;
			t.type = type;
			t.subscribers.push_back(node);
			_topics[topic] = t;
		}
		else
		{
			auto& t = _topics[topic];
			for (int i = 0; i < t.subscribers.size(); i++)
			{
				if (t.subscribers[i] == node)
					return;
			}
			t.subscribers.push_back(node);
		}
		//printf("Get sub %s type %s node %s\n", topic, type, node);
		return;
	};

	node.def_cb = [](const char* type, const ps_message_definition_t* def)
	{
		//printf("got message definition info");
		definition = *def;
	};

	ps_node_system_query(&node);
	if (num_args > 1)
	{
		std::string verb = args[1];
		if (verb == "topic")
		{
			if (num_args < 3)
			{
				printf("Not enough arguments");
				return 0;
			}

			std::string subverb = args[2];

			if (subverb == "list")
			{
				wait(&node);

				printf("Topics:\n--------------\n");
				for (auto topic : _topics)
				{
					std::cout << topic.first << "\n";
				}
				return 0;
			}

			if (num_args < 4)
			{
				printf("Not enough arguments");
				return 0;
			}

			std::string topic = args[3];

			if (subverb == "echo")
			{
				// create a subscriber
				ps_sub_t sub;
				std::vector<void*> todo_msgs;

				// subscribe to the topic and publish anything we get
				bool subscribed = false;
				while (ps_okay())
				{
					ps_node_spin(&node);

					// spin until we get the topic
					auto info = _topics.find(topic);
					if (!subscribed && info != _topics.end())
					{
						std::cout << "Topic " << topic << " found!\n";
						//std::cout << info->first.c_str() << " " <<  info->second.type.c_str();
						subscribed = true;

						ps_node_create_subscriber(&node, info->first.c_str(), 0/*info->second.type.c_str()*/, &sub, 1, true);
					}

					if (!subscribed)
					{
						//printf("Waiting for topic...\n");
						continue;
					}

					if (todo_msgs.size() && sub.received_message_def.fields != 0)
					{
						for (auto msg : todo_msgs)
						{
							ps_deserialize_print(msg, &sub.received_message_def);
							printf("-------------\n");
						}
						todo_msgs.clear();
					}

					// get and deserialize the messages
					void* data;
					while ((data = ps_sub_deque(&sub)) && ps_okay())
					{
						if (sub.received_message_def.fields == 0)
						{
							//printf("WARN: got message but no message definition yet...\n");
							// queue it up, then print them out once I get it
							todo_msgs.push_back(data);
						}
						else
						{
							ps_deserialize_print(data, &sub.received_message_def);
						}
						printf("-------------\n");
					}
				}
			}
			else if (subverb == "info")
			{
				wait(&node);

				auto info = _topics.find(topic);
				if (info == _topics.end())
				{
					std::cout << "Topic " << topic << " not found!\n";
					return 0;
				}

				std::cout << "Type: " << info->second.type << "\n";
				std::cout << "Published by:\n";
			    for (auto pub: info->second.publishers)
					std::cout << " " << pub << "\n";

				std::cout << "\nSubscribed by:\n";
				for (auto sub : info->second.subscribers)
					std::cout << " " << sub << "\n";
				return 0;
			}
			else if (subverb == "show")
			{
				// print out the message definition string for this topic
				bool got_data = false;
				while (ps_okay())
				{
					ps_node_spin(&node);
					Sleep(1);

					// spin until we get the topic
					auto info = _topics.find(topic);
					if (!got_data && info != _topics.end())
					{
						std::cout << "Topic " << topic << " found!\n";
						//std::cout << info->first.c_str() << " " << info->second.type.c_str();
						got_data = true;

						//get the message definition from it
						std::cout << "Querying for message definition...\n";
						ps_node_query_message_definition(&node, info->second.type.c_str());
					}

					if (!got_data)
					{
						//printf("Waiting for topic...\n");
						continue;
					}

					if (!definition.num_fields)
					{
						//printf("Waiting for fields...\n");
						continue;
					}

					printf("Got message description, publishing...\n");
					// print the message format as a string
					ps_print_definition(&definition);

					return 0;
				}
			}

			if (subverb == "pub")
			{
				std::string data = num_args > 4 ? "" : args[4];

				//ok, lets get the topic type and publish at 1 Hz for the moment
				ps_pub_t pub;
				ps_msg_t msg;
				msg.data = 0;

				// subscribe to the topic and publish anything we get
				bool got_data = false;
				bool got_message = false;
				while (ps_okay())
				{
					ps_node_spin(&node);
					Sleep(1);

					// spin until we get the topic
					auto info = _topics.find(topic);
					if (!got_data && info != _topics.end())
					{
						std::cout << "Topic " << topic << " found!\n";
						std::cout << info->first.c_str() << " " << info->second.type.c_str();
						got_data = true;

						//get the message definition from it
						std::cout << "Querying for message definition...\n";
						ps_node_query_message_definition(&node, info->second.type.c_str());
					}

					if (!got_data)
					{
						//printf("Waiting for topic...\n");
						continue;
					}

					if (!definition.num_fields)
					{
						//printf("Waiting for fields...\n");
						continue;
					}
					else if (msg.data == 0)
					{
						printf("Found message description, publishing...\n");
						// encode the message according to the definition and create the publisher
						ps_node_create_publisher(&node, info->first.c_str(), &definition, &pub);

						// then actually serialize the message
						// build the input string from arguments
						std::string input;
						for (int i = 4; i < num_args; i++)
						{
							input += args[i];
							input += ' ';
						}
						Value out;

						try
						{
							parse(input, out);
						}
						catch (std::string error)
						{
							printf("Failure parsing message value: %s", error.c_str());
							return -1;
						}

						try
						{
							msg = serialize_value(out);
						}
						catch (std::string error)
						{
							printf("Serializing failed: %s", error.c_str());
							return -1;
						}
					}

					//publish
					printf("publishing\n");
					ps_pub_publish(&pub, &msg);
					Sleep(1000);
				}
			}
		}
		else if (verb == "node")
		{
			if (num_args == 3)
			{
				std::string verb2 = args[2];
				if (verb2 == "list")
				{
					wait(&node);

					// build a list of nodes then spit them out
					std::map<std::string, int> nodes;
					for (auto& topic : _topics)
					{
						for (auto sub : topic.second.subscribers)
							nodes[sub] = 1;

						for (auto pub : topic.second.publishers)
							nodes[pub] = 1;
					}

					std::cout << "Nodes:\n------------\n";
					for (auto node : nodes)
					{
						std::cout << " " << node.first << "\n";
					}
					return 0;
				}
			}
			if (num_args >= 4)
			{
				std::string verb2 = args[2];
				std::string node_name = args[3];
				if (verb2 == "info")
				{
					wait(&node);

					std::vector<std::string> subs;
					std::vector<std::string> pubs;

					for (auto& topic : _topics)
					{
						for (auto sub : topic.second.subscribers)
							if (sub == node_name)
								subs.push_back(topic.first);

						for (auto pub : topic.second.publishers)
							if (pub == node_name)
								pubs.push_back(topic.first);
					}

					std::cout << "Node: " << node_name;

					if (subs.size() == 0 && pubs.size() == 0)
					{
						std::cout << "Could not find any subs or pubs for this node. Is it up?\n";
						return 0;
					}

					std::cout << "\nSubscribers:\n-------\n";
					for (auto sub : subs)
					{
						std::cout << " " << sub << "\n";
					}

					std::cout << "\nPublishers:\n-------\n";
					for (auto pub : pubs)
					{
						std::cout << " " << pub << "\n";
					}
					return 0;
				}
			}
		}
		else
		{
			printf("ERROR: Unhandled verb %s\n", args[1]);
		}
	}
	else
	{
		print_help();
	}

	// this destroys a node and everything inside of it
	ps_node_destroy(&node);

	return 0;
}
