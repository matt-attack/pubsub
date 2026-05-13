#include <pubsub_pipeline/block.h>

#include <pubsub_cpp/arg_parse.h>

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

using namespace pubsub::pipeline;

int main(int argc, char** argv)
{
  pubsub::ArgParser parser;
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
  bool pub = parser.GetBool("p");
  if (true)
  {
    node = new pubsub::Node("playback", false, false);
    
    auto real_node = node->getNode();
    ps_tcp_transport_init(&tcp_transport, real_node);
    ps_node_add_transport(real_node, &tcp_transport);
  }

  Context context(node, false);
  
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
      auto nodelet = (BlockBase*)loader.load(item.c_str());
      if (nodelet == 0)
      {
        printf("failed to create nodelet\n");
        return -1;
      }
      context.add_block(std::move(std::unique_ptr<BlockBase>(nodelet)));
    }
  }
  
  // todo support simulation

  context.start();
  
  //next things to work on: logging
  
  //also, lets start on integrating something like TF
  
  //okay, bug: with just the controller nodelet it gets stuck in playback because no pubs and timer doesnt seem to endstop
  
  context.join();

  return 0;
}
