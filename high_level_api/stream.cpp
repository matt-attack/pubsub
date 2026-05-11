#include "pubsub_pipeline/stream.h"
#include <pubsub_pipeline/node_base.h>

void Stream::enqueue_placeholders(pubsub::Time time)
{
  auto context = subscribers[0]->context;
  for (const auto& topic: driven_topics)
  {
    printf("enqueing placeholder on topic %s at time %f from topic %s\n", topic.c_str(), time.usec/1e6, this->topic.c_str());
    auto& stream = context->streams[topic];
    
    Sample s2;
    s2.remaining = stream.subscribers.size();
    s2.time = time;// todo when I have latency take that into account
    s2.index = stream.index_counter;
    s2.message = {};// placeholder
    auto res = stream.samples.insert(s2);
    if (res.second)
    {
      stream.index_counter++;
    }
    else
    {
      printf("already had placeholder at this time on topic %s\n", topic.c_str());
    }
  }
}

void Stream::enqueue_end()
{
  if (subscribers.size() == 0)
  {
    return;
  }

  auto& stream_mutex = subscribers[0]->context->stream_mutex;
  Sample s;
  s.remaining = subscribers.size();
  s.is_end = true;
  s.time = pubsub::Time(std::numeric_limits<uint64_t>::max());
  s.index = index_counter++;
  
  // need to remove any leftover placeholders, iterate back from end removing any
  stream_mutex.lock();
  while (samples.size())
  {
    auto iter = --samples.end();
    if (!iter->message)
    {
      printf("deleting placeholder at time %f on topic %s\n", iter->time.usec/1e6, topic.c_str());
      samples.erase(iter);
      continue;
    }
    break;
  }
  // todo maybe check if ended here?

  samples.insert(s);
  stream_mutex.unlock();
    
  for (const auto& sub: subscribers)
  {
    sub->counts[topic]++;
    sub->cv.notify_one();
  }
}

void Stream::enqueue_holder(pubsub::Time time, HolderBase* msg)
{
  if (subscribers.size() == 0)
  {
    delete msg;
    return;
  }
  auto context = subscribers[0]->context;
  auto& stream_mutex = context->stream_mutex;

  Sample s;
  s.remaining = subscribers.size();
  s.time = time;
  //s.index = index_counter++;
  s.message.reset(msg);
  // for debugging refcounting
  /*for (auto& sub: subscribers)
  {
    s.owners.insert(sub->name);
  }*/
  stream_mutex.lock();
  
  // check if theres a placeholder I need to replace
  
  // todo dont do this stupidly
  bool found = false;
  int loops = 0;
  printf("enqueuing with %li samples on topic %s\n", samples.size(), topic.c_str());
  // if the last message isnt a placeholder it means we can skip this whole loop
  bool has_placeholder = false;
  if (samples.size())
  {
    auto last = samples.rbegin();
    if (last->is_end)
    {
      if (samples.size() > 1)
      {
        last++;
        if (!last->message)
        {
          has_placeholder = true;
        }
      }
    }
    else if (!last->message)
    {
      has_placeholder = true;
    }
  }
  if (has_placeholder)
  {
    for (auto iter = samples.begin(); iter != samples.end();)
    {
      loops++;
      // also remove any placeholders before this one
      if (iter->time < s.time && !iter->message)
      {
        printf("deleted placeholder on topic %s at time %f\n", topic.c_str(), iter->time.usec/1e6);
        iter = samples.erase(iter);
        continue;
      }
      if (iter->time == s.time)
      {
        s.index = iter->index;
        samples.erase(iter);
        samples.insert(s);
        found = true;
        break;
      }
      iter++;
    }
  }
  printf("looped over %i samples on topic %s\n", loops, topic.c_str());
  
  //basically we have a read position and two write positions
  
  // insert placeholders in other streams
  if (!found)
  {
    s.index = index_counter++;
    samples.insert(s);
  }
  
  auto first_time = samples.begin()->time;
  auto size = samples.size();
  
  enqueue_placeholders(time);
  stream_mutex.unlock();
    
  for (const auto& sub: subscribers)
  {
    //printf("enqueued with %s\n", sub.parent->name.c_str());
    sub->counts[topic]++;
    sub->cv.notify_one();
  }
  
  // make sure we dont get too far ahead of the rest of the system
  // todo make these are changable
  const pubsub::Duration max_ahead_time(5, 0);
  const int min_ahead_size = 10;
  if (size > min_ahead_size && (time - first_time) > max_ahead_time)
  {
    // wait for messages to get consumed
    while (true)
    {
      // todo ideally this would block on something
      ps_sleep(10);
      std::unique_lock<std::mutex> lk(stream_mutex);
      if (samples.size() < min_ahead_size)
      {
        break;
      }
      if ((time - samples.begin()->time) < max_ahead_time)
      {
        break;
      }
    }
  }
}
