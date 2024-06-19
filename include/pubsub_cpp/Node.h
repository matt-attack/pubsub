#pragma once

#include <pubsub/Node.h>
#include <pubsub/Publisher.h>
#include <pubsub/Subscriber.h>
#include <pubsub/System.h>


#include <vector>
#include <thread>
#include <mutex>
#include <string>
#include <map>
#include <deque>
#include <functional>
#include <algorithm>
#include <memory>


//#include <WS2tcpip.h>
#include <pubsub/Events.h>

namespace pubsub
{
static std::map<std::string, std::string> _remappings;

// how intraprocess passing works
class SubscriberBase;
class PublisherBase;

// this mutex protects both of the below
static std::mutex _publisher_mutex;
static std::multimap<std::string, PublisherBase*> _publishers;
static std::multimap<std::string, SubscriberBase*> _subscribers;

// this assumes topic and ns are properly checked
// ns should not have a leading slash, topic should if it is absolute
inline std::string handle_remap(const std::string& topic, const std::string& ns)
{
    //printf("Handling remap of %s in ns %s\n", topic.c_str(), ns.c_str());
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
    if (ns.length())
	    return "/" + ns + "/" + topic;
    else
        return "/" + topic;
}

// not thread safe
inline void initialize(const char** args, const int argc)
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
inline std::string validate_name(const std::string& name, bool remove_leading_slashes = false)
{
    //printf("Validating %s\n", name.c_str());
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
      int i = 0;
      while (name[i] == '/') { i++;}
	
      return name.substr(i);
	}
    else
    {
      // remove any duplicate slashes we may have
      std::string out;
      if (name.length())
      {
        out += name[0];
      }
      for (size_t i = 1; i < name.length(); i++)
      {
        if (name[i] == '/' && out[i-1] == '/')
        {
          continue;
        }
        out += name[i];
      }
      return out;
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
	bool marked_ = false;

	ps_event_set_t* event_set_;
public:
	// todo try and eliminate this needing to be recursive
	// recursive for the moment...
	std::recursive_mutex lock_;

	// todo fix me being public
	// probably need to add advertise and subscribe functions to me
	std::vector<SubscriberBase*> subscribers_;

	Node(const std::string& name, bool use_broadcast = false) : original_name_(name), event_set_(0)
	{
		// look up the namespace for this node (todo)
		std::string ns = "/";
		
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
        if (namespace_.length())
		    return "/" + namespace_ + "/" + real_name_;
        else
        {
            return "/" + real_name_;
        }
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

    inline void setEventSet(ps_event_set_t* set)
    {
      event_set_ = set;
    }

	inline ps_event_set_t* getEventSet()
	{
		return event_set_;
	}

	// mark that we have a message to process
	void mark() { marked_ = true; }

	bool marked() { bool res = marked_; marked_ = false; return res;}
};

class SubscriberBase;
class PublisherBase
{
	friend class SubscriberBase;
protected:
	std::string topic_;
	std::string remapped_topic_;
	bool latched_;

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
	std::shared_ptr<T> latched_msg_;
public:
	friend class Subscriber<T>;

	Publisher(Node& node, const std::string& topic, bool latched = false)// : topic_(topic)
	{
		node_ = &node;
		topic_ = topic;
		latched_ = latched;

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

        // look for any matching subscribers and add them to our list
        auto iterpair = _subscribers.equal_range(topic);
        for (auto it = iterpair.first; it != iterpair.second; ++it)
        {
          node.lock_.lock();
          subs_.push_back(it->second);
          node.lock_.unlock();
        }
		_publisher_mutex.unlock();
	}

	~Publisher()
	{
		close();
	}

	void close()
	{
		if (!node_)
		{
			return;
		}

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

		node_ = 0;
	}

	// note due to locking only one publish can happen on a node at a time
	// this does not copy the message for intraprocess
	void publish(const std::shared_ptr<T>& msg)
	{
		if (latched_)
		{
			latched_msg_ = msg;
		}

		// loop through shared subscribers
		node_->lock_.lock();
		// now go through my local subscriber list
		for (size_t i = 0; i < subs_.size(); i++)
		{
			auto sub = subs_[i];

			//printf("Publishing locally with no copy..\n");

			auto specific_sub = (Subscriber<T>*)sub;
			ps_event_set_trigger(specific_sub->node_->getEventSet());
			specific_sub->queue_mutex_.lock();
			specific_sub->queue_.push_front(msg);
			if (specific_sub->queue_.size() > specific_sub->queue_size_)
			{
				specific_sub->queue_.pop_back();
			}
			specific_sub->queue_mutex_.unlock();
			specific_sub->node_->mark();
		}

		ps_pub_publish_ez(&publisher_, (void*)msg.get());
		node_->lock_.unlock();
	}

	// note due to locking only one publish can happen on a node at a time
	// this publish type copies the message for intraprocess
	void publish(const T& msg)
	{
		std::shared_ptr<T> copy;
		if (latched_) {
			copy = std::shared_ptr<T>(new T);
			*copy = msg;
			// save for later
			latched_msg_ = copy;
		}
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

			// help this isnt thread safe (or is it?)
			auto specific_sub = (Subscriber<T>*)sub;
			ps_event_set_trigger(specific_sub->node_->getEventSet());
			specific_sub->queue_mutex_.lock();
			specific_sub->queue_.push_front(copy);
			if (specific_sub->queue_.size() > specific_sub->queue_size_)
			{
				specific_sub->queue_.pop_back();
			}
			specific_sub->queue_mutex_.unlock();
			specific_sub->node_->mark();
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

	void addCustomEndpoint(const int ip_addr, const short port, const unsigned int stream_id)
	{
		ps_endpoint_t endpoint;
		endpoint.address = ip_addr;
		endpoint.port = port;
		ps_pub_add_endpoint_client(&publisher_, &endpoint, stream_id);
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
	static void AddSubscriber(const std::string& topic, SubscriberBase* sub, std::function<void(void*)> cb)
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
				// if its latched, get the message from it
				if (it->second->latched_)
				{
					// hmm, this should just queue not call
					cb(it->second);
				}
				// add me to its sub list
				it->second->subs_.push_back(sub);
				it->second->GetNode()->lock_.unlock();
			}
		}

        _subscribers.insert(std::pair<std::string, SubscriberBase*>(topic, sub));

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

        //remove me from the subscriber list
        auto subiterpair = _subscribers.equal_range(topic);
        for (auto it = subiterpair.first; it != subiterpair.second; ++it)
        {
          if (it->second == sub)
          {
            _subscribers.erase(it);
            break;
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

	Subscriber(Node& node, const std::string& topic, std::function<void(const std::shared_ptr<T>&)> cb, unsigned int queue_size = 1, int preferred_transport = 0) : cb_(cb), queue_size_(queue_size)
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
		};

		struct ps_subscriber_options options;
		ps_subscriber_options_init(&options);

		options.queue_size = 0;
		options.cb = cb2;
		options.cb_data = this;
		options.allocator = 0;
		options.ignore_local = true;
		options.preferred_transport = preferred_transport;


		node.lock_.lock();
		ps_node_create_subscriber_adv(node.getNode(), remapped_topic_.c_str(), T::GetDefinition(), &subscriber_, &options);
		node.subscribers_.push_back(this);
		node.lock_.unlock();

		AddSubscriber(remapped_topic_, this, [&](void* p){
			auto pub = (Publisher<T>*)p;
			ps_event_set_trigger(node_->getEventSet());
			queue_mutex_.lock();
			queue_.push_front(pub->latched_msg_);
			if (queue_.size() > queue_size_)
			{
				queue_.pop_back();
			}
			queue_mutex_.unlock();
			node_->mark();
		});
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
		close();
	}

	void close()
	{
		if (!node_)
		{
			return;
		}

		RemoveSubscriber(remapped_topic_, this);

		node_->lock_.lock();
		auto it = std::find(node_->subscribers_.begin(), node_->subscribers_.end(), this);
		if (it != node_->subscribers_.end())
			node_->subscribers_.erase(it);
		ps_sub_destroy(&subscriber_);
		node_->lock_.unlock();

		node_ = 0;
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

}
