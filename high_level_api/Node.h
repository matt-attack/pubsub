#pragma once

#include "../src/Node.h"
#include "../src/Publisher.h"
#include "../src/Subscriber.h"
#include "../src/System.h"


#include <vector>
#include <thread>
#include <mutex>
#include <string>
#include <map>

namespace pubsub
{
static std::map<std::string, std::string> _remappings;

// how intraprocess passing works
class SubscriberBase;
static std::mutex _subscriber_mutex;
static std::multimap<std::string, SubscriberBase*> _subscribers;

// this assumes topic and ns are properly checked
// ns should not have a leading slash, topic should if it is absolute
std::string handle_remap(const std::string& topic, const std::string& ns)
{
	// we need at least one character
	if (topic.length() == 0)
	{
		return "";
	}

	// handle absolute topics
	if (topic[0] == '/')
	{
		return topic;
	}

	//okay lets search for remappings

	// first look for an exact match for the local topic
	auto iter = _remappings.find(topic);
	if (iter != _remappings.end())
	{
		// this is still in our namespace if the value doesnt have a leading slash
		if (iter->second.length() && iter->second[0] == '/')
		{
			return iter->second;// was a remapping to a global
		}
		else
		{
			return "/" + ns+ "/" + iter->second;
		}
	}

	// now try an exact match for the fully qualified topic if the topic isnt n
	iter = _remappings.find(ns + "/" + topic);
	if (iter != _remappings.end())
	{
		// this is still in our namespace if the value doesnt have a leading slash
		if (iter->second.length() && iter->second[0] == '/')
		{
			return iter->second;// was a remapping to a global
		}
		else
		{
			return "/" + ns + "/" + iter->second;
		}
	}

	// okay, we had no remappings, use our namespace
	return "/" + ns + "/" + topic;
}

// not thread safe
void initialize(const char** args, const int argc)
{
	// fills out global remappings, names and namespaces

	// todo, how do I set namespaces if we have multiple nodes in the same process?
	for (int i = 1; i < argc; i++)
	{
		std::string arg = args[i];
		int pos = arg.find(":=");
		if (pos != std::string::npos)
		{
			//okay, lets split it
			std::string key = arg.substr(0, pos);
			std::string value = arg.substr(pos + 1);

			printf("Got key %s value %s\n", key.c_str(), value.c_str());
		}
	}
}


// makes sure that a user given name is valid
// valid names must be all lowercase and only
std::string validate_name(const std::string& name, bool remove_leading_slashes = false)
{
	for (size_t i = 0; i < name.length(); i++)
	{
		if (name[i] >= 'A' && name[i] <= 'Z')
		{
			printf("Identifier %c is invalid. All characters must be lowecase", name[i]);
			return "";
		}
	}

	// todo remove any double slashes and check for invalid characters

	if (remove_leading_slashes)
	{
		if (name[0] == '/')
			return name.substr(1);
	}
	return name;
}

// todo need to make sure multiple subscribers in a process share data

// safe to use each node in a different thread after initialize is called
// making calls to functions on the same node is not thread safe
class SubscriberBase;
class Spinner;
class Node
{
	friend class Spinner;
	friend class SubscriberBase;
	std::string original_name_;
	std::string real_name_;
	std::string namespace_;

	ps_node_t node_;
public:
	// todo fix me being public
	// probably need to add advertise and subscribe functions to me
	std::vector<SubscriberBase*> subscribers_;

	Node(const std::string& name, bool use_broadcast = false) : original_name_(name)
	{
		// look up the namespace for this node (todo)
		std::string ns = "/ns_test";
		
		// now look up the name (todo)
		std::string arg_name = original_name_;

		// fix up and check the namespace and name
		real_name_ = validate_name(arg_name, true);
		namespace_ = validate_name(ns, true);

		std::string underlying_name = getQualifiedName();
		ps_node_init(&node_, underlying_name.c_str(), "", use_broadcast);
	}

	~Node()
	{
		ps_node_destroy(&node_);
	}

	std::string getQualifiedName()
	{
		return "/" + namespace_ + "/" + real_name_;
	}

	const std::string& getName()
	{
		return real_name_;
	}

	ps_node_t* getNode()
	{
		return &node_;
	}

	const std::string& getNamespace()
	{
		return namespace_;
	}

	inline int spin()
	{
		return ps_node_spin(&node_);
	}
};

template<class T> 
class Publisher
{
	std::string topic_;
	std::string remapped_topic_;

	ps_pub_t publisher_;
public:

	Publisher(Node& node, const std::string& topic, bool latched = false) : topic_(topic)
	{
		// clean up the topic
		std::string real_topic = validate_name(topic_);

		// look up remapping
		remapped_topic_ = handle_remap(real_topic, "/" + node.getNamespace());

		ps_node_create_publisher(node.getNode(), remapped_topic_.c_str(), T::GetDefinition(), &publisher_, latched);
	}

	~Publisher()
	{
		ps_pub_destroy(&publisher_);
	}

	// this does not copy the message for intraprocess
	void publish(const std::shared_ptr<T>& msg)
	{
		// loop through shared subscribers
		_subscriber_mutex.lock();
		auto subs = _subscribers.equal_range(remapped_topic_);
		for (auto ii = subs.first; ii != subs.second; ii++)
		{
			printf("Publishing locally copy-free..\n");
			auto sub = ii->second;

			// hopefully its the right type
			// todo verify or this will crash horribly
			auto specific_sub = (Subscriber<T>*)sub;
			specific_sub->cb_(msg);
		}
		_subscriber_mutex.unlock();

		ps_pub_publish_ez(&publisher_, (void*)msg.get());
	}

	// this publish type copies the message for intraprocess
	void publish(const T& msg)
	{
		std::shared_ptr<T> copy;
		// loop through shared subscribers
		_subscriber_mutex.lock();
		auto subs = _subscribers.equal_range(remapped_topic_);
		for (auto ii = subs.first; ii != subs.second; ii++)
		{
			printf("Publishing locally with a copy..\n");
			if (!copy)
			{
				//copy to shared ptr
				copy = std::shared_ptr<T>(new T);
				*copy = msg;
			}
			auto sub = ii->second;

			// hopefully its the right type
			// todo verify or this will crash horribly
			auto specific_sub = (Subscriber<T>*)sub;
			specific_sub->cb_(copy);
		}
		_subscriber_mutex.unlock();

		// todo make this only copy/encode if necessary
		ps_pub_publish_ez(&publisher_, (void*)&msg);
	}

	unsigned int getNumSubscribers()
	{
		return ps_pub_get_subscriber_count(&publisher_);
	}
};

class Spinner;
class SubscriberBase
{
	friend class Spinner;
protected:
	ps_sub_t subscriber_;

	std::function<void(void*)> raw_cb_;
public:

	ps_sub_t* GetSub()
	{
		return &subscriber_;
	}
};

template<class T> 
class Subscriber: public SubscriberBase
{
	friend class Spinner;
	std::string remapped_topic_;

public:
	// todo put me in the right place
	std::function<void(const std::shared_ptr<T>&)> cb_;

	Subscriber(Node& node, const std::string& topic, std::function<void(const std::shared_ptr<T>&)> cb, unsigned int queue_size = 1) : cb_(cb)
	{
		// clean up the topic
		std::string validated_topic = validate_name(topic);

		remapped_topic_ = handle_remap(validated_topic, "/" + node.getNamespace());

		ps_node_create_subscriber(node.getNode(), remapped_topic_.c_str(), T::GetDefinition(), &subscriber_, queue_size, false, 0, true);

		node.subscribers_.push_back(this);

		raw_cb_ = [this](void* msg)
		{
			// convert to shared ptr
			auto msg_ptr = std::shared_ptr<T>((T*)msg);
			cb_(msg_ptr);
		};

		_subscriber_mutex.lock();
		_subscribers.insert(std::pair<std::string, SubscriberBase*>(remapped_topic_, this));
		_subscriber_mutex.unlock();
	}

	~Subscriber()
	{
		_subscriber_mutex.lock();
		// remove me from the list
		auto iterpair = _subscribers.equal_range(remapped_topic_);
		for (auto it = iterpair.first; it != iterpair.second; ++it)
		{
			if (it->second == this) 
			{
				_subscribers.erase(it);
				break;
			}
		}
		_subscriber_mutex.unlock();

		ps_sub_destroy(&subscriber_);

		// todo remove me from the list
		//auto it = std::find(node.subscribers_.begin(), node.subscribers_.end(), 5);
		//if (it != node.subscribers_.end())
		//	node.subscribers_.erase(it);
	}

	T* deque()
	{
		return (T*)ps_sub_deque(&subscriber_);
	}

	const std::string& getQualifiedTopic()
	{
		return remapped_topic_;
	}
};

// todo use smart pointers for nodes
class Spinner
{
	std::vector<Node*> nodes_;

	bool running_;

	std::thread thread_;
	std::mutex list_mutex_;
public:

	// todo make it work with more than one thread
	Spinner(int num_threads = 1) : running_(true)
	{
		thread_ = std::thread([this]()
		{
			while (running_ && ps_okay())
			{
				list_mutex_.lock();
				for (auto node : nodes_)
				{
					if (ps_node_spin(node->getNode()))
					{
						// we got a message, now call a subscriber
						for (size_t i = 0; i < node->subscribers_.size(); i++)
						{
							auto sub = node->subscribers_[i]->GetSub();
							while (void* msg = ps_sub_deque(sub))
							{
								node->subscribers_[i]->raw_cb_(msg);
							}
						}
					}
				}
				list_mutex_.unlock();

				ps_sleep(10);// make this configurable?
			}
		});
	}

	~Spinner()
	{
		if (running_)
		{
			stop();
		}
	}

	void addNode(Node& node)
	{
		list_mutex_.lock();
		nodes_.push_back(&node);
		list_mutex_.unlock();
	}

	void wait()
	{
		thread_.join();
	}

	void stop()
	{
		// kill everything
		if (!running_)
		{
			return;
		}

		running_ = false;
		thread_.join();// wait for it to stop
	}
};

}