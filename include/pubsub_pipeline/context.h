#pragma once

#include "holder.h"
#include "stream.h"

#include <memory>
#include <set>
#include <vector>
#include <string>
#include <functional>

#include <pubsub_cpp/Node.h>

// Defines a
class MockNode;
class Publisher;
struct Stream;
class Block;
class Context
{
  friend class MockNode;
  friend class Publisher;
  friend struct Stream;
  friend class Block;
  template <typename T>
  friend class PipelineTimer;
  
  static void thread_live(Block*);
  static void thread_playback(Block*, pubsub::Time, pubsub::Time);

  bool is_playback;
  pubsub::Node* node = 0;
  std::mutex stream_mutex;
  std::map<std::string, Stream> streams;
  int timer_id = 0;
  bool always_publish = false;
  volatile bool run_timers = true;
  std::vector<std::unique_ptr<Block>> nodes_;
  volatile bool running = true;
  std::mutex node_mutex;
  
  std::vector<std::thread> node_threads_;
public:

  const std::map<std::string, Stream>& get_streams()
  {
    return streams;
  }
  
  std::map<std::string, Stream>& _streams()
  {
    return streams;
  }

  // playback constructor
  Context() : Context(0) { is_playback = true; }

  // live constructor (though if the_node is 0, it turns into playback)
  // okay, so publish if node is provided, 
  Context(pubsub::Node* the_node, bool playback = false)
  {
    node = the_node;
    if (the_node && playback)
    {
      is_playback = true;
      always_publish = false;// value for this isnt important in playback mode
    }
    else if (the_node)
    {
      // always publish mode
      is_playback = false;
      always_publish = true;
    }
    else
    {
      is_playback = false;
      always_publish = false;
    }
  }

  ~Context()
  {
    join();
  }

  void add_node(std::unique_ptr<Block>&& node);
  
  void add_node(Block* block);
  
  void join();
  
  static void recurse(
    std::map<std::string, std::vector<std::string>>& inputs_to_outputs,
    std::vector<std::string>& out, std::string topic)
  {
    out.push_back(topic);
    for (const auto& out_topic: inputs_to_outputs[topic])
    {
      recurse(inputs_to_outputs, out, out_topic);
    }
  }
  
  // todo make start time implicit like it is for timers
  void start_playback(pubsub::Time start_time, pubsub::Time end_time);
  
  // Start all nodes and blocks running in this context
  void start();
  
  void abort();
  
  void stop();
};
