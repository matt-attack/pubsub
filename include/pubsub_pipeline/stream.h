#pragma once

#include "holder.h"

#include <pubsub_cpp/Time.h>

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

// A message sample in a stream.
struct Sample
{
  pubsub::Time time;
  bool is_end = false;
  
  mutable int remaining = 0;
  int32_t index;
  
  std::shared_ptr<HolderBase> message;
  // mutable std::set<std::string> owners;// for debugging refcounting
  
  bool operator <(const Sample& left) const {
    return time < left.time;
  }
};

// Stream of message samples in time order
class RealBlock;
struct Stream
{
  std::set<Sample> samples;
  std::vector<std::shared_ptr<RealBlock>> subscribers;
  std::string topic;
  std::string type;
  
  // data for deserialization
  uint64_t hash = 0;
  std::function<HolderBase*(const void*)> decode;// to decode a raw message
  
  // list of topics that are driven by any topic published on this message
  std::vector<std::string> driven_topics;
  
  int publishers = 0;
  uint32_t index_counter = 0;
  
  bool ended = false;
  
  void enqueue_holder(pubsub::Time time, HolderBase* msg);

  void enqueue_end();
//private:
  void enqueue_placeholders(pubsub::Time time);
};
