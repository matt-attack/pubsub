#include <pubsub_pipeline/node_base.h>

#include <pubsub_cpp/arg_parse.h>

//#include <rucksack/rucksack.h>

#include <pubsub/TCPTransport.h>

#include <cassert>

#include <dlfcn.h>

typedef void*(*fun)(void*);

struct Loader
{
  void* (*load)(const char*) = 0;
  char** (*list)() = 0;
  
  std::vector<std::string> names;
};

Loader load_library(const std::string& path)
{
  void* handle = dlopen(path.c_str(), RTLD_GLOBAL | RTLD_LAZY);
  if (!handle)
  {
    printf("Failed to open dll %s\n", path.c_str());
    return Loader();
  }

  Loader l;
 
  *(void**)(&l.load) = dlsym(handle, "create_nodelet");
  if (l.load == 0)
  {
    printf("Failed to load symbol\n");
    return Loader();
  }
  
  *(void**)(&l.list) = dlsym(handle, "list_nodelets");
  if (l.list == 0)
  {
    printf("Failed to load symbol\n");
    return Loader();
  }
  
  // get available nodelets
  auto list = l.list();
  auto list_start = list;
  while (*list != 0)
  {
    l.names.push_back(*list);
    delete[] *list;
    list++;
  }
  delete[] list_start;
  
  return l;
}

/*void playback(const std::string& file, Context& context, pubsub::Node* node)
{
  rucksack::SackIndexedReader sack;
  if (!sack.open(file))
  {
    printf("ERROR: Opening sack failed!\n");
    return;
  }
  
  MockNode pb_node(context);
  
  auto& index = sack.index();
  struct ChannelInfo
  {
    std::string topic;
    int count = 0;
    int hash = 0;
  };
  std::vector<ChannelInfo> channels;
  for (auto& c: index.channels)
  {
    ChannelInfo ci;
    ci.topic = c.topic;
    ci.count = 0;
    ci.hash = c.definition.hash;
    channels.push_back(ci);
  }
  uint64_t end_time = 0;
  for (auto& m: index.messages)
  {
    auto ci = m.chunk_index;
    auto connection = index.chunks[ci].connection_id;
    channels[connection].count++;
    end_time = std::max(m.timestamp, end_time);
  }
  
  // Count the number of messages on each topic
  // this is used so we can identify the last message published in the data
  std::map<std::string, int> remaining;
  std::map<std::string, Publisher> pubs;
  for (auto& c: channels)
  {
    remaining[c.topic] = c.count;
    if (c.count > 0)
    {
      pubs[c.topic] = pb_node.advertise(c.topic);
    }
  }
  
  // check if we need to do any message migration
  auto& streams = context.get_streams();
  for (const auto& info: channels)
  {
    auto& stream = streams.find(info.topic)->second;
    if (info.hash != stream.hash && stream.hash != 0)
    {
      throw std::runtime_error("early mismatch");
    }
    
    // todo support migration
    // easiest way to do this is to cut and paste within the encoded message
  }

  bool started = false;
  auto real_begin = pubsub::Time::now();
  pubsub::Time start_time;

  const float time_scale = 0.0;// todo allow changing this via parameter <= 0 is infinite

  rucksack::MessageHeader const* hdr;
  rucksack::SackChannelDetails const* info;
  while (const void* msg = sack.read(hdr, info))
  {
    if (!ps_okay())
    {
      // destroy pubs
      pubs.clear();
      break;
    }

    if (node)
    {
      ps_node_spin(node->getNode());
    }

    if (!started)
    {
      context.start_playback(pubsub::Time(hdr->time), pubsub::Time(end_time));
      start_time = pubsub::Time(hdr->time);
      started = true;
    }

    // optionally pace messages here if someone asks for real time
    if (time_scale > 0.0)
    {
      double diff = (pubsub::Time(hdr->time) - start_time).toSec();
      //printf("diff %f\n%li\n", diff, time.usec);
      double real_diff;
      //printf("real diff %f\n%li\n", real_diff, time.usec);
      while (real_diff = ((pubsub::Time::now() - real_begin).toSec() * time_scale), (diff > real_diff))// converts to "sim time"
      {
        double dt_us = (diff - real_diff)*1000000.0;
        printf("sleep dt %f\n", dt_us);

        // make sure we dont sleep too long so control c works if the bag is silly
        if (dt_us > 500000)
        {
          dt_us = 500000;

          if (ps_okay() == false)
          {
            break;
          }
        }
        ps_sleep_us(std::max<int>(dt_us, 0));
      }
    }
    
    //printf("Message %s at %lf\n", info->topic.c_str(), hdr->time/1000000.0);

    // validate if the hash matches subscribers
    
    // if it doesnt match, we need to convert
    
    pubs[info->topic].publish(msg, pubsub::Time(hdr->time), info->definition.hash);
    
    // remove the publisher so 
    remaining[info->topic] -= 1;
    if (remaining[info->topic] == 0)
    {
      printf("hit end of topic %s\n", info->topic.c_str());
      pubs.erase(info->topic);
    }
  }
  // there should be no topics left in pubs at this point and the context is just waiting for everything to propagate while it auto-joins
  assert(pubs.size() == 0);
}*/

int main(int argc, char** argv)
{
  pubsub::ArgParser parser;
  parser.AddOption({ "r", "replay" }, "Replay the file with the given name through these nodelets.");
  parser.AddFlag({ "p", "publish" }, "Publish topics.");
  parser.SetUsage("Usage: nodelet_manager <nodelets to launch>...\n\nLaunches the listed nodelets in a single process.");
  parser.Parse(argv, argc);
  
  auto positional = parser.GetAllPositional();

  if (!positional.size())
  {
    printf("Not enough arguments.\n");
    return -1;
  }
  
  std::map<std::string, std::vector<std::string>> to_load;
  for (auto& arg: positional)
  {
    auto p = arg.find_first_of(':');
    if (p == std::string::npos)
    {
      printf("Bad argument. Nodelets to load must be in the form: <library>:<node name>\n");
      return -1;
    }
    std::string file = arg.substr(0, p);
    std::string name = arg.substr(p + 1);
    to_load[file].push_back(name);
  }
  
  struct ps_transport_t tcp_transport;
  
  pubsub::Node* node = 0;
  auto replay = parser.GetString("r");
  bool pub = parser.GetBool("p");
  if (!replay.size() || pub)
  {
    node = new pubsub::Node("playback", false, false);
    
    auto real_node = node->getNode();
    ps_tcp_transport_init(&tcp_transport, real_node);
    ps_node_add_transport(real_node, &tcp_transport);
  }

  Context context(node, replay.size() > 0);
  
  for (const auto& list: to_load)
  {
    Loader loader = load_library(list.first);
    if (!loader.load)
    {
      printf("Failed to load library\n");
      return -1;
    }
    
    for (const auto& name: loader.names)
    {
      printf("Contains %s\n", name.c_str());
    }
  
    for (const auto& item: list.second)
    {
      auto nodelet = (Block*)loader.load(item.c_str());
      if (nodelet == 0)
      {
        printf("failed to create nodelet\n");
        return -1;
      }
      context.add_node(std::move(std::unique_ptr<Block>(nodelet)));
    }
  }

  //double rate = parser.GetDouble("r");
  //bool latched = parser.GetBool("l");
  
  if (replay.size())
  {
    //playback(replay, context, node);
  }
  else
  {
    context.start();
  }
  
  //next things to work on: logging
  
  //also, lets start on integrating something like TF
  
  //okay, bug: with just the controller nodelet it gets stuck in playback because no pubs and timer doesnt seem to endstop
  
  context.join();

  return 0;
}
