
#include <pubsub/Node.h>
#include <pubsub/Publisher.h>
#include <pubsub/Subscriber.h>
#include <pubsub/Serialization.h>
#include <pubsub/System.h>
#include <pubsub/TCPTransport.h>

#include <pubsub_cpp/Time.h>
#include "../high_level_api/Serialize.h"

#include <pubsub_cpp/arg_parse.h>

#include <iostream>
#include <map>
#include <vector>
#include <deque>

#include <stdio.h>

#include <string>
#include <string.h>



using namespace std;

void print_help()
{
	printf("Usage: pubsub <verb> <subverb> (arg1) (arg2)\n"
		" Verbs:\n"
		"   topic -\n"
		"      list\n"
		"      hz (topic name)\n"
		"      bw (topic name)\n"
		"      info (topic name)\n"
		"      show (topic name)\n"
		"      pub (topic name) (message)\n"
		"   node -\n"
		"      list\n"
		"      info (node name)\n");
}

void wait(ps_node_t* node)
{
	printf("Waiting for responses...\n\n");
	unsigned long long start = GetTimeMs();
	while (ps_okay() && start + 3000 > GetTimeMs())
	{
		ps_node_wait(node, 100);
		ps_node_spin(node);
	}
}

struct Topic
{
	std::string type;

	std::vector<std::string> subscribers;
	std::vector<std::string> publishers;
};

ps_message_definition_t definition = { 0, 0 };


std::map<std::string, Topic> _topics;

struct NodeInfo
{
	int port;
	int address;
    int transports;
};

std::map<std::string, NodeInfo> _nodes;

int main(int num_args_real, char** args)
{
	// for running tests
	int num_args = num_args_real;
	//num_args = 5;

	//char* args[] = { "aaa", "topic", "hz", "/data", "-w", "10" };
	//num_args = (sizeof args / sizeof args[0]);

	if (num_args <= 1)
	{
		print_help();
		return 0;
	}

	// Setup the node with a random name
	static ps_node_t node;
	ps_node_init(&node, "Query", "", false);

    struct ps_transport_t tcp_transport;
    ps_tcp_transport_init(&tcp_transport, &node);
    ps_node_add_transport(&node, &tcp_transport);

	// Setup introspection callbacks
	node.adv_cb = [](const char* topic, const char* type, const char* node, const ps_advertise_req_t* data)
	{
		NodeInfo info;
		info.address = data->addr;
		info.port = data->port;
        info.transports = data->transports;
		_nodes[node] = info;

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
			for (unsigned int i = 0; i < t.publishers.size(); i++)
			{
				if (t.publishers[i] == node)
					return;
			}
			t.publishers.push_back(node);
		}
		//printf("Get pub %s type %s node %s\n", topic, type, node);
		return;
	};

	node.sub_cb = [](const char* topic, const char* type, const char* node, const ps_subscribe_req_t* data)
	{
        auto iter = _nodes.find(node);
        if (iter == _nodes.end())
        {
          NodeInfo info;
          info.address = data->addr;
          info.port = data->port;
          info.transports = 0;
          _nodes[node] = info;
        }
        else
        {
          iter->second.address = data->addr;
          iter->second.port = data->port;
        }
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
			for (unsigned int i = 0; i < t.subscribers.size(); i++)
			{
				if (t.subscribers[i] == node)
					return;
			}
			t.subscribers.push_back(node);
		}
		//printf("Get sub %s type %s node %s\n", topic, type, node);
		return;
	};

	node.def_cb = [](const ps_message_definition_t* def)
	{
		//printf("got message definition info");
		ps_copy_message_definition(&definition, def);
	};

	// Query the other nodes in the network for their data
	ps_node_system_query(&node);

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
			pubsub::ArgParser parser;
			parser.AddMulti({ "i" }, "Print info about the publisher with each message.");
			parser.AddMulti({ "n" }, "Number of messages to echo.", "0");
			parser.AddMulti({ "skip", "s" }, "Skip factor for the subscriber.", "0");
            parser.AddMulti({ "tcp" }, "Prefer the TCP transport.");

			parser.Parse(args, num_args, 3);

			static bool print_info = parser.GetBool("i");
            double vn = parser.GetDouble("n");
            if (vn <= 0)
            {
              vn = 2147483647;
            }
			static unsigned long long int n = vn;
			int skip = parser.GetDouble("s");
            bool tcp = parser.GetBool("tcp");


			// create a subscriber
			static ps_sub_t sub;
			static std::vector<std::pair<void*, ps_msg_info_t>> todo_msgs;

			// subscribe to the topic and publish anything we get
			static unsigned long long int count = 0;
			bool subscribed = false;
			while (ps_okay())
			{
				ps_node_wait(&node, 1000);
				ps_node_spin(&node);

				// spin until we get the topic
				auto info = _topics.find(topic);
				if (!subscribed && info != _topics.end())
				{
					std::cout << "Topic " << topic << " found!\n";
					//std::cout << info->first.c_str() << " " <<  info->second.type.c_str();
					subscribed = true;

					struct ps_subscriber_options options;
					ps_subscriber_options_init(&options);

					options.skip = skip;
					options.queue_size = 0;
					options.want_message_def = true;
					options.allocator = 0;
					options.ignore_local = false;
                    options.preferred_transport = tcp ? 1 : 0;
					options.cb = [](void* message, unsigned int size, void* data, const ps_msg_info_t* info)
					{
						if (todo_msgs.size() && sub.received_message_def.fields != 0)
						{
							for (auto msg : todo_msgs)
							{
								if (print_info)
								{
									const auto info = &msg.second;
									for (auto& n : _nodes)
									{
										if (info->address == n.second.address
											&& info->port == n.second.port)
										{
											std::cout << "Node: " << n.first << "\n";
										}
									}
									// map the id to the node
									std::cout << "From: "
										<< ((info->address & 0xFF000000) >> 24) << "."
										<< ((info->address & 0xFF0000) >> 16) << "."
										<< ((info->address & 0xFF00) >> 8) << "."
										<< ((info->address & 0xFF)) << ":" << info->port << "\n";
									printf("-------------\n");
								}
								ps_deserialize_print(msg.first, &sub.received_message_def);
								printf("-------------\n");
								free(msg.first);
								if (++count >= n)
								{
									//need to commit sodoku here..
									// this destroys a node and everything inside of it
									ps_node_destroy(&node);
									//return 0;
									exit(0);
								}
							}
							todo_msgs.clear();
						}

						// get and deserialize the messages
						if (sub.received_message_def.fields == 0)
						{
							//printf("WARN: got message but no message definition yet...\n");
							// queue it up, then print them out once I get it
							todo_msgs.push_back({ message, *info });
						}
						else
						{
							if (print_info)
							{
								for (auto& n : _nodes)
								{
									if (info->address == n.second.address
										&& info->port == n.second.port)
									{
										std::cout << "Node: " << n.first << "\n";
									}
								}
								// map the id to the node
								std::cout << "From: "
									<< ((info->address & 0xFF000000) >> 24) << "."
									<< ((info->address & 0xFF0000) >> 16) << "."
									<< ((info->address & 0xFF00) >> 8) << "."
									<< ((info->address & 0xFF)) << ":" << info->port << "\n";
								printf("-------------\n");
							}
							ps_deserialize_print(message, &sub.received_message_def);
							printf("-------------\n");

							free(message);
							if (++count >= n)
							{
								// need to commit sodoku here..
								// this destroys a node and everything inside of it
								ps_node_destroy(&node);
								//return 0;
								exit(0);
							}
						}
					};

					ps_node_create_subscriber_adv(&node, info->first.c_str(), 0, &sub, &options);
				}
			}
		}
		else if (subverb == "hz" || subverb == "bw")
		{
			pubsub::ArgParser parser;
			parser.AddMulti({ "w", "window" }, "Window size for averaging.", "100");

			parser.Parse(args, num_args, 2);

			// create a subscriber
			ps_sub_t sub;
			std::vector<void*> todo_msgs;

			std::deque<std::pair<pubsub::Time, unsigned int>> message_times;
			static size_t window = parser.GetDouble("w");
			// subscribe to the topic
			while (ps_okay())
			{
				ps_node_wait(&node, 1000);
				ps_node_spin(&node);

				// spin until we get the topic
				auto info = _topics.find(topic);
				if (info != _topics.end())
				{
					std::cout << "Topic " << topic << " found!\n";

					auto cb = [](void* message, unsigned int size, void* data, const ps_msg_info_t* info)
					{
						std::deque<std::pair<pubsub::Time, unsigned int>>* message_times = (std::deque<std::pair<pubsub::Time, unsigned int>>*)data;
						message_times->push_back({ pubsub::Time::now(), size });

						if (message_times->size() > window)
							message_times->pop_front();
					};

					ps_node_create_subscriber_cb(&node, info->first.c_str(), 0, &sub, cb, &message_times, false, 0, false);
					break;
				}
			}

			// now just receive and time as fast as we can
			unsigned long long last_print = GetTimeMs();
			while (ps_okay())
			{
				ps_node_wait(&node, 100);
				ps_node_spin(&node);

				if (last_print + 1000 < GetTimeMs())
				{
					// print the timing
					last_print = GetTimeMs();

					if (message_times.size() < 2)
					{
						printf("Not enough messages...\n");
					}
					else
					{
						if (subverb == "bw")
						{
							// in sec
							double delta = (pubsub::Time::now() - message_times.front().first).toSec();
							unsigned long long total = 0;
							for (size_t i = 0; i < message_times.size(); i++)
							{
								total += message_times[i].second;
							}
							double rate = ((double)total) / ((double)delta);
							if (rate > 5000000)
							{
								printf("Bw: %0.3lf MB/s n=%zi\n", rate / 1000000.0, message_times.size());
							}
							else if (rate > 5000)
							{
								printf("Bw: %0.3lf KB/s n=%zi\n", rate / 1000.0, message_times.size());
							}
							else
							{
								printf("Bw: %lf B/s n=%zi\n", rate, message_times.size());
							}
						}
						else
						{
							double delta = (pubsub::Time::now() - message_times.front().first).toSec();
							double rate = ((double)(message_times.size() - 1)) / ((double)delta);
							printf("Rate: %lf Hz n=%zi\n", rate, message_times.size());
						}
					}
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
			for (auto pub : info->second.publishers)
            {
                int transports = _nodes[pub].transports;
                std::string tps = "UDP";
                if (transports & PS_TRANSPORT_TCP)
                {
                    tps += ", TCP";
                }
				std::cout << " " << pub << " (" << tps <<")\n";
            }

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
				ps_node_wait(&node, 1000);
				ps_node_spin(&node);

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
		else if (subverb == "pub")
		{
			std::string data = num_args > 4 ? "" : args[4];

			pubsub::ArgParser parser;
			parser.AddMulti({ "r", "rate" }, "Publish rate in Hz.", "1.0");
			parser.AddMulti({ "l", "latch" }, "Latches the topic.", "true");

			parser.Parse(args, num_args, 2);

			double rate = parser.GetDouble("r");
			bool latched = parser.GetBool("l");

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

				// spin until we get the topic
				auto info = _topics.find(topic);
				if (!got_data && info != _topics.end())
				{
					std::cout << "Topic " << topic << " found!\n";
					std::cout << "Type: " << " " << info->second.type.c_str() << "\n";
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
					ps_node_create_publisher(&node, info->first.c_str(), &definition, &pub, latched);

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
						msg = serialize_value(out, definition);
					}
					catch (std::string error)
					{
						printf("Serializing failed: %s", error.c_str());
						return -1;
					}
				}

				//publish
				// copy the message
				ps_msg_t cpy = ps_msg_cpy(&msg);
				ps_pub_publish(&pub, &cpy);
				ps_sleep(1000.0 / rate);
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
		else if (num_args >= 4)
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

				std::cout << "Node: " << node_name << "\n";


				// print port info if we got it
				if (_nodes.find(node_name) != _nodes.end())
				{
					NodeInfo info = _nodes[node_name];
					std::cout << " Address: "
						<< ((info.address & 0xFF000000) >> 24) << "."
						<< ((info.address & 0xFF0000) >> 16) << "."
						<< ((info.address & 0xFF00) >> 8) << "."
						<< ((info.address & 0xFF)) << ":" << info.port << "\n";

                    int transports = info.transports;
                    std::string tps = "UDP";
                    if (transports & PS_TRANSPORT_TCP)
                    {
                        tps += ", TCP";
                    }
                    std::cout << " Transports: " << tps << "\n";
				}

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

	// this destroys a node and everything inside of it
	ps_node_destroy(&node);

	return 0;
}
