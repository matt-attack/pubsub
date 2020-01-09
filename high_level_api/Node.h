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
#include <deque>
#include <functional>
#include <algorithm>

namespace pubsub
{
static std::map<std::string, std::string> _remappings;

// how intraprocess passing works
class SubscriberBase;
class PublisherBase;
//static std::mutex _subscriber_mutex;
//static std::multimap<std::string, SubscriberBase*> _subscribers;
static std::mutex _publisher_mutex;
static std::multimap<std::string, PublisherBase*> _publishers;

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
	std::string qualified_name_;// necessary to hold the pointer to the name for C

	ps_node_t node_;
public:
	// todo try and eliminate this needing to be recursive
	// recursive for the moment...
	std::recursive_mutex lock_;

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

		qualified_name_ = getQualifiedName();
		ps_node_init(&node_, qualified_name_.c_str(), "", use_broadcast);
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

class SubscriberBase;
class PublisherBase
{
	friend class SubscriberBase;
protected:
	std::string topic_;
	std::string remapped_topic_;

	Node* node_;

	ps_pub_t publisher_;

	std::vector<SubscriberBase*> subs_;

public:
	const std::string& GetTopic()
	{
		return remapped_topic_;
	}

	Node* GetNode()
	{
		return node_;
	}
};

template<class T>
class Subscriber;
template<class T> 
class Publisher: public PublisherBase
{

public:

	Publisher(Node& node, const std::string& topic, bool latched = false)// : topic_(topic)
	{
		node_ = &node;
		topic_ = topic;

		// clean up the topic
		std::string real_topic = validate_name(topic_);

		// look up remapping
		remapped_topic_ = handle_remap(real_topic, node.getNamespace());

		node.lock_.lock();
		ps_node_create_publisher(node.getNode(), remapped_topic_.c_str(), T::GetDefinition(), &publisher_, latched);
		node.lock_.unlock();

		//add me to the publisher list
		_publisher_mutex.lock();
		_publishers.insert(std::pair<std::string, PublisherBase*>(remapped_topic_, this));
		_publisher_mutex.unlock();
	}

	~Publisher()
	{
		//remove me from the publisher list
		_publisher_mutex.lock();
		// remove me from the list
		auto iterpair = _publishers.equal_range(remapped_topic_);
		for (auto it = iterpair.first; it != iterpair.second; ++it)
		{
			if (it->second == this)
			{
				_publishers.erase(it);
				break;
			}
		}
		_publisher_mutex.unlock();

		node_->lock_.lock();
		ps_pub_destroy(&publisher_);
		node_->lock_.unlock();
	}

	// note due to locking only one publish can happen on a node at a time
	// this does not copy the message for intraprocess
	void publish(const std::shared_ptr<T>& msg)
	{
		// loop through shared subscribers
		node_->lock_.lock();
		// now go through my local subscriber list
		for (size_t i = 0; i < subs_.size(); i++)
		{
			auto sub = subs_[i];

			//printf("Publishing locally with no copy..\n");

			auto specific_sub = (Subscriber<T>*)sub;
			specific_sub->queue_mutex_.lock();
			specific_sub->queue_.push_front(msg);
			if (specific_sub->queue_.size() > specific_sub->queue_size_)
			{
				specific_sub->queue_.pop_back();
			}
			specific_sub->queue_mutex_.unlock();
			//specific_sub->cb_(copy);
		}

		ps_pub_publish_ez(&publisher_, (void*)msg.get());
		node_->lock_.unlock();
	}

	// note due to locking only one publish can happen on a node at a time
	// this publish type copies the message for intraprocess
	void publish(const T& msg)
	{
		std::shared_ptr<T> copy;
	
		node_->lock_.lock();
		// now go through my local subscriber list
		for (size_t i = 0; i < subs_.size(); i++)
		{
			auto sub = subs_[i];

			//printf("Publishing locally with a copy..\n");
			if (!copy)
			{
				//copy to shared ptr
				copy = std::shared_ptr<T>(new T);
				*copy = msg;
			}

			// help this isnt thread safe
			auto specific_sub = (Subscriber<T>*)sub;
			specific_sub->queue_mutex_.lock();
			specific_sub->queue_.push_front(copy);
			if (specific_sub->queue_.size() > specific_sub->queue_size_)
			{
				specific_sub->queue_.pop_back();
			}
			specific_sub->queue_mutex_.unlock();
			//specific_sub->cb_(copy);
		}

		// note this still copies unnecessarily if the topic is latched
		// todo make this only copy/encode if necessary
		ps_pub_publish_ez(&publisher_, (void*)&msg);

		node_->lock_.unlock();
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
	Node* node_;
	
	// note only one pub/sub can be created/destroyed at a time
	static void AddSubscriber(const std::string& topic, SubscriberBase* sub)
	{
		_publisher_mutex.lock();

		auto iterpair = _publishers.equal_range(topic);
		for (auto it = iterpair.first; it != iterpair.second; ++it)
		{
			if (it->second->GetTopic() == topic)
			{
				// todo verify that the message type is the same or that I'm a generic sub
				// also todo add generic C++ subs
				it->second->GetNode()->lock_.lock();
				// add me to its sub list
				it->second->subs_.push_back(sub);
				it->second->GetNode()->lock_.unlock();
			}
		}

		_publisher_mutex.unlock();
	}

	static void RemoveSubscriber(const std::string& topic, const SubscriberBase* sub)
	{
		_publisher_mutex.lock();

		auto iterpair = _publishers.equal_range(topic);
		for (auto it = iterpair.first; it != iterpair.second; ++it)
		{
			if (it->second->GetTopic() == topic)
			{
				it->second->GetNode()->lock_.lock();
				// remove me from its list if im there
				auto pos = std::find(it->second->subs_.begin(), it->second->subs_.end(), sub);
				if (pos != it->second->subs_.end())
					it->second->subs_.erase(pos);
				it->second->GetNode()->lock_.unlock();
			}
		}

		_publisher_mutex.unlock();
	}
public:

	ps_sub_t* GetSub()
	{
		return &subscriber_;
	}

	// returns if there are still more messages in the queue
	virtual bool CallOne() = 0;
};



template<class T> 
class Subscriber: public SubscriberBase
{
	friend class Spinner;
	friend class Publisher<T>;
	std::string remapped_topic_;

	std::mutex queue_mutex_;
	unsigned int queue_size_;
	std::deque<std::shared_ptr<T>> queue_;

	std::function<void(const std::shared_ptr<T>&)> cb_;

public:

	Subscriber(Node& node, const std::string& topic, std::function<void(const std::shared_ptr<T>&)> cb, unsigned int queue_size = 1) : cb_(cb), queue_size_(queue_size)
	{
		node_ = &node;

		// clean up the topic
		std::string validated_topic = validate_name(topic);

		remapped_topic_ = handle_remap(validated_topic, node.getNamespace());

		auto cb2 = [](void* msg, unsigned int size, void* th, const ps_msg_info_t* info)
		{
			// convert to shared ptr
			std::shared_ptr<T> msg_ptr((T*)msg);

			auto real_this = (Subscriber<T>*)th;
			real_this->queue_mutex_.lock();
			real_this->queue_.push_front(msg_ptr);
			if (real_this->queue_.size() > real_this->queue_size_)
			{
				real_this->queue_.pop_back();
			}
			real_this->queue_mutex_.unlock();
			//real_this->cb_(msg_ptr);
		};

		node.lock_.lock();
		ps_node_create_subscriber_cb(node.getNode(), remapped_topic_.c_str(), T::GetDefinition(), &subscriber_, cb2, this, false, 0, true);

		node.subscribers_.push_back(this);
		node.lock_.unlock();

		AddSubscriber(remapped_topic_, this);
	}

	virtual bool CallOne()
	{
		queue_mutex_.lock();
		if (queue_.size() > 0)
		{
			auto back = queue_.back();
			queue_.pop_back();

			queue_mutex_.unlock();
			cb_(back);
			return queue_.size() > 0;
		}
		queue_mutex_.unlock();
		return false;
	}

	~Subscriber()
	{
		RemoveSubscriber(remapped_topic_, this);

		node_->lock_.lock();
		auto it = std::find(node_->subscribers_.begin(), node_->subscribers_.end(), this);
		if (it != node_->subscribers_.end())
			node_->subscribers_.erase(it);
		ps_sub_destroy(&subscriber_);
		node_->lock_.unlock();
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
class BlockingSpinner
{
	std::vector<Node*> nodes_;

	bool running_;

	std::thread thread_;
	std::mutex list_mutex_;
public:

	// todo make it work with more than one thread
	BlockingSpinner(int num_threads = 1) : running_(true)
	{
		thread_ = std::thread([this]()
		{
			while (running_ && ps_okay())
			{
				list_mutex_.lock();
				for (auto node : nodes_)
				{
					// todo block for more nodes simultaneously
					ps_node_wait(node->getNode(), 10000);

					node->lock_.lock();
					if (ps_node_spin(node->getNode()))
					{

					}
					// todo how to make this not scale with subscriber count...
					// though, it isnt very important here
					// its probably worth more effort just make it not run any of this if we get no messages
					// we got a message, now call a subscriber
					for (size_t i = 0; i < node->subscribers_.size(); i++)
					{
						// this doesnt ensure switching subs, but works
						while (node->subscribers_[i]->CallOne()) {};
					}
					node->lock_.unlock();
				}
				list_mutex_.unlock();

				ps_sleep(10);// make this configurable?
			}
		});
	}

	~BlockingSpinner()
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
					node->lock_.lock();
					if (ps_node_spin(node->getNode()))
					{

					}
					// todo how to make this not scale with subscriber count...
					// though, it isnt very important here
					// its probably worth more effort just make it not run any of this if we get no messages
					// we got a message, now call a subscriber
					for (size_t i = 0; i < node->subscribers_.size(); i++)
					{
						// this doesnt ensure switching subs, but works
						while (node->subscribers_[i]->CallOne()) {};
					}
					node->lock_.unlock();
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