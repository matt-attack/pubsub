#include <pubsub_cpp/Node.h>

namespace pubsub
{
  std::mutex _publisher_mutex;
  std::multimap<std::string, PublisherBase*> _publishers;
  std::multimap<std::string, SubscriberBase*> _subscribers;
  
  std::map<std::string, std::string> _remappings;
}
