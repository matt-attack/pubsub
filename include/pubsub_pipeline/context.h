#pragma once

#include "holder.h"
#include "stream.h"

#include <memory>
#include <set>
#include <vector>
#include <string>
#include <functional>

#include <pubsub_cpp/Node.h>

namespace pubsub
{
namespace pipeline
{

class MockNode;
class Publisher;
struct Stream;
class BlockBase;
// Defines an operating environment for a series of Blocks, either playback or live
class Context
{
  friend class MockNode;
  friend class Publisher;
  friend struct Stream;
  friend class BlockBase;
  template <typename T>
  friend class Timer;
  
  static void thread_live(BlockBase*);
  static void thread_playback(BlockBase*, pubsub::Time, pubsub::Time);

  bool is_playback;
  pubsub::Node* node = 0;
  std::mutex stream_mutex;
  std::map<std::string, Stream> streams;
  int timer_id = 0;
  bool always_publish = false;
  volatile bool run_timers = true;
  volatile bool running = true;
  
  std::mutex block_mutex_;
  std::vector<std::unique_ptr<BlockBase>> blocks_;
  std::vector<std::thread> block_threads_;
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

  void add_block(std::unique_ptr<BlockBase>&& node);
  
  void add_block(BlockBase* block);
  
  // wait for all blocks to exit
  void join();
  
  // Start all blocks running in this context in playback mode between the given start and end time
  void start_playback(pubsub::Time start_time, pubsub::Time end_time);
  
  // Start all blocks running in this context
  void start();
  
  // exit playback ASAP
  void abort();
  
  // in playback, stop all timers forcing simulation to end after current cycle
  void stop();
};
}
}
