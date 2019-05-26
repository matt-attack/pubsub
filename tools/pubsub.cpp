// PubSubCMake.cpp : Defines the entry point for the application.
//

#include "src/Node.h"
#include "src/Publisher.h"
#include "src/Subscriber.h"
#include "src/Serialization.h"

#include <stdio.h>

#include <string>

using namespace std;

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

int main(int num_args, char** args)
{
	ps_node_t node;
	ps_node_init(&node, "Query", "192.168.0.103", false);

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
				// wait for details about the topic
				//wait(&node);
				// create a subscriber
				ps_sub_t sub;

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

						ps_node_create_subscriber(&node, info->first.c_str(), info->second.type.c_str(), &sub, 1, true);
					}

					if (!subscribed)
					{
						//printf("Waiting for topic...\n");
						continue;
					}

					// get and deserialize the messages
					void* data;
					while ((data = ps_sub_deque(&sub)) && ps_okay())
					{
						if (sub.received_message_def.fields == 0)
						{
							printf("ERROR: got message but no message definition yet...\n");
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

			if (subverb == "pub")
			{
				std::string data = num_args > 4 ? "" : args[4];

				//ok, lets get the topic type and publish at 1 Hz for the moment
				ps_pub_t pub;
				ps_msg_t msg;

				// subscribe to the topic and publish anything we get
				bool got_data = false;
				while (ps_okay())
				{
					ps_node_spin(&node);

					// spin until we get the topic
					auto info = _topics.find(topic);
					if (!got_data && info != _topics.end())
					{
						std::cout << "Topic " << topic << " found!\n";
						std::cout << info->first.c_str() << " " << info->second.type.c_str();
						got_data = true;

						ps_message_definition_t *type;
						ps_node_create_publisher(&node, info->first.c_str(), type, &pub);
						//ps_node_create_subscriber(&node, info->first.c_str(), info->second.type.c_str(), &sub, 1, true);

						//okay, now serialize the message
						//std_msgs_String rmsg;
						//rmsg.value = "Hello";
						//msg = std_msgs_String_encode(&rmsg);
					}

					if (!got_data)
					{
						//printf("Waiting for topic...\n");
						continue;
					}

					//publish
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
