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

std::map<std::string, Topic> _topics;

ps_message_definition_t definition = { 0, 0 };

int main(int num_args, char** args)
{
	ps_node_t node;
	ps_node_init(&node, "Query");

	// for running tests
	//num_args = 5;
	//char* args[] = { "aaa", "topic", "pub", "/data", "\"Testing\"" };

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
				// print out the message definition string
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

						// okay, lets handle serialization
						// if we have just one field map the data to that field
						if (out.type != Map && definition.num_fields == 1)
						{
							auto& field = definition.fields[0];
							if (field.type < FT_MaxFloat)
							{
								// map to the appropriate size integer
								printf("Unhandled datatype.\n");
								return -1;
							}
							else if (field.type == FT_String)
							{
								// just shove it in there
								if (out.type == String)
								{
									const char* val = out.str.c_str();
									int len = strlen(val) + 1;
									ps_msg_alloc(len, &msg);
									memcpy(ps_get_msg_start(msg.data), val, len);
								}
								else
								{
									printf("Unhandled datatype.\n");
									return -1;
								}
							}
							else
							{
								printf("Unhandled datatype.\n");
								return -1;
							}
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
