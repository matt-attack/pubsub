#pragma once

#include <pubsub_cpp/Node.h>
#include <pubsub_cpp/Time.h>

#include <condition_variable>
#include <queue>
#include <set>
#include <atomic>
#include <memory>

#include <pthread.h>

// for now we just support driving + latched and timer

class Context;

// Okay each component is a Node, each node can have multiple 

/// Get the value type from a pointer to data member
template<typename T>
struct member_pointer_value;
template<typename Class, typename Value>
struct member_pointer_value<Value Class::*>
{
    typedef Value type;
};

/// Get the class type from a pointer to data member
template<typename T>
struct member_pointer_class;
template<typename Class, typename Value>
struct member_pointer_class<Value Class::*>
{
    typedef Class type;
};

/// Base class which just has a virtual destructor
struct HolderBase
{
  pubsub::Time time;

  virtual ~HolderBase() {};
  
  // todo work to remove the need for this
  virtual void* get() = 0;
  
  virtual HolderBase* clone() = 0;
};

struct Sub
{
  std::string topic;
  // index of last consumed message
  int32_t last_msg_idx = -1;// todo should I use int64?
  
  std::function<void(const void* msg, pubsub::Time time)> cb;
  std::function<void()> clear;
};

struct Block;
class Publisher
{
  Block* node_;
  std::string topic_;

public:
  // todo make private
  // used to clear the publisher when we go out of scope
  std::shared_ptr<HolderBase> reference_;
public:

  Publisher() {
  
  }

  Publisher(Block* b, std::string t) {
    node_ = b;
    topic_ = t;
  }

  template <class T>
  void publish(T msg, pubsub::Time time) const;
  
  template <class T>
  void publish(std::shared_ptr<T> msg, pubsub::Time time) const;
  
  void publish(const void* ptr, pubsub::Time time, uint32_t hash) const;
  
  void publish_end() const;
};

struct Block
{
  struct RealBlock
  {
    std::function<void(pubsub::Time, void*)> do_thing;
    std::function<void(pubsub::Time)> do_timeout;
    std::function<void()> do_shutdown;
    std::condition_variable cv;
    std::vector<Sub> subs;
    //std::vector<std::string> pubs;// todo keep track with weak or something
    std::deque<std::unique_ptr<HolderBase>> queue;
    bool is_timer = false;
    double timeout = -1.0;
    
    std::map<std::string, std::atomic<int>> counts;
    std::vector<Sub> driving;
    
    std::vector<pubsub::SubscriberBase*> subscribers;
    
    std::vector<std::function<void(Context*)>> to_add;
    std::map<std::string, std::shared_ptr<pubsub::PublisherBase>> pubs;
    
    RealBlock()
    {
     
    }
    
    ~RealBlock() {
      printf("real block out of scope\n");
      for (auto sub: subscribers) {
        delete sub;
      }
    }
  };
  std::shared_ptr<HolderBase> holder;
  std::shared_ptr<RealBlock> data;
  double rate = 0.0;
  std::string name;
  Context* context = 0;
  
  std::function<void()> on_context;
  
  Block(const std::string& name) : name(name)
  {
    
  }
  
  void set_context(Context* ctx);
  
  virtual ~Block() {}
    
  template <typename T>
  void subscribe(T offset, const std::string& topic, bool driving = false);
  
  template <typename T>
  void subscribe(T offset, int index, const std::string& topic, bool driving = false);
  
  template <typename T>
  Publisher advertise(const std::string& topic);
  
  void timeout(double timeout_sec, std::function<void(pubsub::Time)> cb)
  {
    data->do_timeout = cb;
    data->timeout = timeout_sec;
  }
  
  template <typename T>
  void start(std::function<void(const T&, pubsub::Time time)> cb);
  
  void set_on_shutdown(std::function<void()> cb)
  {
    data->do_shutdown = cb;
  }
};

template <typename T>
class PipelineBlock: public Block
{
public:
  PipelineBlock(const std::string& name)
    : Block(name)
  {
    struct Holder: public HolderBase
    {
      T data;
      
      void* get() override { return &data; }
      
      HolderBase* clone() override { return new Holder(*this); }
    };

    auto& block = *this;
    block.holder.reset(new Holder());
    block.data.reset(new Block::RealBlock());
    block.data->is_timer = false;
    
    // todo rename this as it isnt really "start"
    start([this](const T& msg, pubsub::Time time) { update(msg, time); });
  }
  
  virtual void update(const T& message, pubsub::Time time) {}
  
  void start(std::function<void(const T&, pubsub::Time time)> cb) {
    Block::start<T>(cb);
  }
};

template <typename T>
class PipelineTimer: public Block
{
public:
  PipelineTimer(const std::string& name, double rate);
  
  virtual void update(const T& message, pubsub::Time time) {}
  
  void start(std::function<void(const T&, pubsub::Time time)> cb) {
    Block::start<T>(cb);
  }
};

// A message sample in a stream.
struct Sample
{
  pubsub::Time time;
  bool timer_sample = false;
  bool is_end = false;
  
  mutable int remaining = 0;
  int32_t index;
  
  std::shared_ptr<HolderBase> message;
  
  bool operator <(const Sample& left) const {
    return time < left.time;
  }
};

// Stream of message samples in time order
struct Stream
{
  std::set<Sample> samples;
  std::vector<Block> subscribers;
  std::string topic;
  std::string type;
  uint64_t hash = 0;
  std::function<HolderBase*(const void*)> decode;// to decode a raw message
  
  // list of topics that are driven by any topic published on this message
  std::vector<std::string> driven_topics;
  
  int publishers = 0;
  uint32_t index_counter = 0;
  
  bool ended = false;
  
  void enqueue_timer(pubsub::Time time);
  
  template <class T>
  void enqueue(pubsub::Time time, const T& msg);
  
  void enqueue_holder(pubsub::Time time, HolderBase* msg);

  void enqueue_end();
//private:
  void enqueue_placeholders(pubsub::Time time);
};

// Dummy node that has no blocks and drives execution in playback
struct MockNode: public PipelineBlock<int>
{
  MockNode(Context& ctx) : PipelineBlock<int>("mock") {
    set_context(&ctx);
  }

  ~MockNode() {
    shutdown();
  }

  void shutdown() {
    // stop all pubs and timers
  
  }
  
  Publisher advertise(const std::string& topic);
};



void thread_playback(Block*, pubsub::Time, pubsub::Time);
void thread_live(Block*);

// Defines a
class MockNode;
class Publisher;
struct Stream;
class Context
{
  friend class MockNode;
  friend class Publisher;
  friend struct Stream;
  friend class Block;
  template <typename T>
  friend class PipelineTimer;
  friend void thread_live(Block*);
  friend void thread_playback(Block*, pubsub::Time, pubsub::Time);

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

  void add_node(std::unique_ptr<Block>&& node)
  {
    node->set_context(this);
    node_mutex.lock();
    nodes_.emplace_back(std::move(node));
    
    // now set it up
    
    // free the node on its shutdown to clear any pubs and such
    int index = nodes_.size() - 1;
    nodes_.back()->set_on_shutdown([this, index]() {
      printf("freeing node\n");
      node_mutex.lock();
      nodes_[index].reset();
      node_mutex.unlock();
    });
    node_mutex.unlock();
  }
  
  void add_node(Block* block)
  {
    add_node(std::move(std::unique_ptr<Block>(block)));
  }
  
  void join()
  {
    // wait for all nodes to finish
    for (auto& thread: node_threads_)
    {
      if (thread.joinable())
      {
        thread.join();
      }
    }
  }
  
  static void recurse(
    std::map<std::string, std::vector<std::string>>& inputs_to_outputs,
    std::vector<std::string>& out, std::string topic)
  {
    out.push_back(topic);
    for (const auto& out_topic: inputs_to_outputs[topic])
    {
      recurse(inputs_to_outputs, out, out_topic);
    }
  };
  
  // todo make start time implicit like it is for timers
  void start_playback(pubsub::Time start_time, pubsub::Time end_time)
  {
    if (!is_playback)
    {
      throw std::runtime_error("Must call start in non-playback mode.");
    }

    // publish endstops for any topics with no publishers
    for (auto& stream: streams)
    {
      if (stream.second.publishers == 0)
      {
        auto& topic = stream.first;
        printf("stream %s had no publishers at start, endstopping\n", stream.first.c_str());
        auto context = this;
  
        std::string name = "context";
        auto s = context->streams.find(topic);
        if (s == context->streams.end())
        {
          printf("[%s] enqueing endstop message on topic %s but there were no subscribers\n", name.c_str(), topic.c_str());
          
          continue;
        }
        
        if (s->second.ended)
        {
          printf("[%s] ending already ended stream %s\n", name.c_str(), topic.c_str());
          continue;
        }
        
        printf("[%s] enqueing endstop message on topic %s\n", name.c_str(), topic.c_str());
        s->second.ended = true;
        s->second.enqueue_end();
      }
    }
    
    // discover the message dependency graph
    // first lets get inputs and outputs of each block
    std::map<std::string, std::vector<std::string>> inputs_to_outputs;
    for (auto& block: nodes_)
    {
      //auto& subs = block->data->subs;
      auto& driving_subs = block->data->driving;
      auto& pubs = block->data->pubs;
      for (const auto& sub: driving_subs)
      {
        for (const auto& pub: pubs)
        {
          inputs_to_outputs[sub.topic].push_back(pub.first);
        }
      }
      
      // we only store subs
      
      
      // basically goal will be for each node, indentify any outputs for a driving input
    }
    
    // print basic graph nodes and outputs
    for (const auto& item: inputs_to_outputs)
    {
      for (const auto& out: item.second)
      {
        printf("topic %s immediately results in a %s\n", item.first.c_str(), out.c_str());
      }
    }
    
    // finally get the full set
    for (const auto& item: inputs_to_outputs)
    {
      // now for each input, determine the complete set of outputs recursively
      std::vector<std::string> topics;
      for (const auto& out: item.second)
      {
        recurse(inputs_to_outputs, topics, out);
      }
      
      printf("Topic %s\n", item.first.c_str());
      for (const auto& out: topics)
      {
        // check for duplicates
        bool found = false;
        for (const auto& dt: streams[item.first].driven_topics)
        {
          if (dt == out)
          {
            found = true;
            break;
          }
        }
        
        if (!found)
        {
          printf("  %s\n", out.c_str());
          streams[item.first].driven_topics.push_back(out);
        }
      }
    }
  
    // start all of the block threads
    node_mutex.lock();
    for (auto& block: nodes_)
    {
      auto copy = block.get();
      node_threads_.emplace_back([copy, start_time, end_time]()
      {
        thread_playback(copy, start_time, end_time);
      });
      auto handle = node_threads_.back().native_handle();
      pthread_setname_np(handle, block->name.substr(0, 15).c_str());
    }  
    node_mutex.unlock();
  }
  
  // Start all nodes and blocks running in this context
  void start()
  {
    if (is_playback)
    {
      throw std::runtime_error("Must call start_playback() in playback");
    }
  
    // start all of the block threads
    node_mutex.lock();
    for (auto& block: nodes_)
    {
      auto copy = block.get();
      node_threads_.emplace_back([copy]()
      {
        thread_live(copy);
      });
      auto handle = node_threads_.back().native_handle();
      pthread_setname_np(handle, block->name.substr(0, 15).c_str());
    }
    node_mutex.unlock();
  }
  
  void abort()
  {
    // shutdown all threads
    running = false;
    node_mutex.lock();
    for (const auto& block: nodes_)
    {
      if (block)
      {
        block->data->cv.notify_one();
      }
    }
    node_mutex.unlock();
  }
  
  void stop()
  {
    // stop timers and let existing data propagate
    run_timers = false;
    node_mutex.lock();
    for (const auto& block: nodes_)
    {
      if (block)
      {
        block->data->cv.notify_one();
      }
    }
    node_mutex.unlock();
  }

private:
  
  int mock_pubs = 0;
  
  void publish_end()
  { 
    // publish ends for timer streams
    for (auto& stream: streams)
    {
      if (stream.first[0] != '~' || stream.second.ended)
      {
        continue;
      }
      
      printf("[%s] enqueing endstop message on topic %s\n", "context", stream.first.c_str());
      stream.second.ended = true;
      stream.second.enqueue_end();
    }
  }
};

template <class T>
PipelineTimer<T>::PipelineTimer(const std::string& name, double rate)
  : Block(name)
{
  struct Holder: public HolderBase
  {
    T data;
     
    void* get() override { return &data; }
     
    HolderBase* clone() override { return new Holder(*this); }
  };
    
  auto& block = *this;
  block.holder.reset(new Holder());
  block.data.reset(new Block::RealBlock());
  block.data->is_timer = true;
  block.rate = rate;

  data->to_add.push_back([this, rate](Context* context)
  { 
    std::string timer_name = "~timer" + std::to_string(context->timer_id++);
    Sub s;
    s.topic = timer_name;
    data->driving.push_back(s);    
    
    std::unique_lock<std::mutex> lk(context->stream_mutex);
    auto& stream = context->streams[timer_name];
    stream.subscribers.push_back(*this);
    stream.topic = timer_name;
    stream.publishers++;
  });
    
  // todo rename this as it isnt really "start"
  start([this](const T& msg, pubsub::Time time) { update(msg, time); });
}

void Block::set_context(Context* ctx)
{ 
  // init everything with the context
  context = ctx;
  
  if (on_context)
  {
    on_context();
  }
  
  for (const auto& item: data->to_add)
  {
    item(ctx);
  }
  data->to_add.clear();
}

Publisher MockNode::advertise(const std::string& topic)
{
  auto ctx = context;
  ctx->streams[topic].publishers++;
  
  ctx->mock_pubs++;
    
  struct XHolder: public HolderBase
  {
    std::string topic;
    Context* ctx;
      
    XHolder(std::string topic, Context* ctx) : topic(topic), ctx(ctx) {}
     
    ~XHolder() {
      // todo lock this properly
      ctx->stream_mutex.lock();
        
      // decrement our topic count and 
      // enqueue end messages if there are no more publishers left
      auto& stream = ctx->streams[topic];
      stream.publishers--;
      ctx->mock_pubs--;
      // if our mock node has no more pubs left, then end the context
      ctx->stream_mutex.unlock();
      
      //subscribers[0].ctx->stream_mutex.unlock();
      printf("removing publisher %s\n", topic.c_str());
      if (stream.publishers == 0)
      {
        printf("pushing end for publisher %s\n", topic.c_str());
        stream.enqueue_end();
      }
      
      if (ctx->mock_pubs == 0)
      {
        ctx->publish_end();
      }
    }
        
    void* get() override
    {
      return 0;
    }
      
    HolderBase* clone() override { return 0; }
  };
    
  Publisher pub(this, topic);
  pub.reference_.reset(new XHolder(topic, context));
  return pub;
}

template <class T>
void Publisher::publish(T msg, pubsub::Time time) const
{
  // todo avoid copy
  auto msg_copy = std::make_shared<T>();
  *msg_copy = msg;
  
  publish(msg_copy, time);
}

template <class T>
void Publisher::publish(std::shared_ptr<T> msg, pubsub::Time time) const
{
  auto context = node_->context;
  
  /*msg->header.sequence = ++
  
  log the trace event for publish
  
  store:
   a. topic name
   b. sequence number
   c. publish time
   d. block execution id - this may not always be present, probably need to store this in a threadlocal
   e. block name*/
  
  auto& name = node_->name;

  auto s = context->streams.find(topic_);
  if (s == context->streams.end())
  {
    printf("[%s] enqueing message on topic %s but there were no subscribers\n", name.c_str(), topic_.c_str());
    return;
  }
  
  printf("[%s] enqueing shared message on topic %s at time %f\n", name.c_str(), topic_.c_str(), time.toSec());

  if (!context->is_playback)
  {
    auto pub = node_->data->pubs.find(topic_);
    auto real_pub = (pubsub::Publisher<T>*)pub->second.get();
    real_pub->publish(msg);
  }
  else
  {
    s->second.enqueue(time, msg);
    if (context->always_publish)
    {
      auto pub = node_->data->pubs.find(topic_);
      auto real_pub = (pubsub::Publisher<T>*)pub->second.get();
      real_pub->publish(msg);
    }
  }
}

void Publisher::publish(const void* ptr, pubsub::Time time, uint32_t hash) const
{
  auto context = node_->context;
  
  auto s = context->streams.find(topic_);
  if (s == context->streams.end())
  {
    //printf("[%s] enqueing message on topic %s but there were no subscribers\n", name.c_str(), topic_.c_str());
    return;
  }
  
  // validate hash on publish so it doesnt even make it downstream
  if (s->second.hash && s->second.hash != hash)
  {
    throw std::runtime_error("Hash mismatch on topic '" + topic_ + "'. The message type may be incorrect or has changed.");
  }
  
  //printf("[%s] enqueing message on topic %s at time %f\n", name.c_str(), topic_.c_str(), time.toSec());
  
  // decode and enqueue
  if (s->second.decode)
  {
    // if no decode, there are no subs
    auto msg = s->second.decode(ptr);
    s->second.enqueue_holder(time, msg);
  }
}
  
void Publisher::publish_end() const
{
  auto context = node_->context;
  
  auto& name = node_->name;
  auto s = context->streams.find(topic_);
  if (s == context->streams.end())
  {
    printf("[%s] enqueing endstop message on topic %s but there were no subscribers\n", name.c_str(), topic_.c_str());
    
    return;
  }
  
  if (s->second.ended)
  {
    printf("[%s] ending already ended stream %s\n", name.c_str(), topic_.c_str());
    return;
  }
  
  printf("[%s] enqueing endstop message on topic %s\n", name.c_str(), topic_.c_str());
  s->second.ended = true;
  s->second.enqueue_end();
}

void Stream::enqueue_placeholders(pubsub::Time time)
{
  auto context = subscribers[0].context;
  for (const auto& topic: driven_topics)
  {
    printf("enqueing placeholder on topic %s at time %f from topic %s\n", topic.c_str(), time.usec/1e6, this->topic.c_str());
    auto& stream = context->streams[topic];
    
    // if the last sample is a placeholder, combine with it
    // this doesnt make sense after all
    /*if (stream.samples.size())
    {
      auto& back = *stream.samples.rbegin();
      if (!back.message && !back.is_end)
      {
        // its a placeholder! update it
        back.end_time = time;
        continue;
      }
    }*/
    
    Sample s2;
    s2.remaining = stream.subscribers.size();
    s2.time = time;// todo when I have latency take that into account
    s2.index = stream.index_counter++;
    s2.message = {};// placeholder
    stream.samples.insert(s2);
  }
}

void Stream::enqueue_timer(pubsub::Time time)
{
  if (subscribers.size() == 0)
  {
    return;
  }
  auto context= subscribers[0].context;
  auto& stream_mutex = context->stream_mutex;
  //todo should probably block if this gets too far ahead (how do I know how long that is?)
  Sample s;
  s.remaining = subscribers.size();
  s.time = time;
  s.timer_sample = true;
  s.index = index_counter++;
  stream_mutex.lock();
  //check if we have too many samples queued and block if so
  //we have too many samples if
  samples.insert(s);
  
  enqueue_placeholders(time);
  stream_mutex.unlock();
    
  for (const auto& sub: subscribers)
  {
    sub.data->counts[topic]++;
    sub.data->cv.notify_one();
  }
}

void Stream::enqueue_end()
{
  if (subscribers.size() == 0)
  {
    return;
  }

  auto& stream_mutex = subscribers[0].context->stream_mutex;
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
    if (!iter->message && !iter->timer_sample)
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
    sub.data->counts[topic]++;
    sub.data->cv.notify_one();
  }
}

void Stream::enqueue_holder(pubsub::Time time, HolderBase* msg)
{
  if (subscribers.size() == 0)
  {
    return;
  }
  auto context = subscribers[0].context;
  auto& stream_mutex = context->stream_mutex;

  Sample s;
  s.remaining = subscribers.size();
  s.time = time;
  //s.index = index_counter++;
  s.message.reset(msg);
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
    sub.data->counts[topic]++;
    sub.data->cv.notify_one();
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
  
template <class T>
void Stream::enqueue(pubsub::Time time, const T& msg)
{
  struct XHolder: public HolderBase
  {
    T msg;
      
    void* get() override
    {
      return &msg;
    }
      
    HolderBase* clone() override { return new XHolder(*this); }
  };

  if (subscribers.size() == 0)
  {
    return;
  }
  
  auto h = new XHolder();
  h->msg = msg;
  enqueue_holder(time, h);
}

template <typename T>
Publisher Block::advertise(const std::string& topic)
{
  data->to_add.push_back([topic, this](Context* context)
  {
    if (context->node && (!context->is_playback || context->always_publish))
    {
      auto pub = data->pubs.find(topic);
      if (pub == data->pubs.end())
      {
        data->pubs[topic] = std::shared_ptr<pubsub::PublisherBase>(new pubsub::Publisher<T>(*context->node, topic));
        pub = data->pubs.find(topic);
      }
    }
    else
    {
      data->pubs[topic] = {};
    }
    context->streams[topic].publishers++;
    context->streams[topic].type = T::GetDefinition()->name;
  });
  
  struct XHolder: public HolderBase
  {
    std::string topic;
    Block* blk;
      
    XHolder(std::string topic, Block* blk) : topic(topic), blk(blk) {}
     
    ~XHolder() {
      // if this happens the publisher was destroyed before the context was created
      if (!blk->context)
      {
        return;
      }
      
      // todo lock context mutex
        
      // decrement our topic count and 
      // enqueue end messages if there are no more publishers left
      auto& stream = blk->context->streams[topic];
      stream.publishers--;
      printf("removing publisher %s\n", topic.c_str());
      if (stream.publishers == 0)
      {
        printf("pushing end for publisher %s\n", topic.c_str());
        stream.enqueue_end();
      }
    }
        
    void* get() override
    {
      return 0;
    }
      
    HolderBase* clone() override { return 0; }
  };
    
  Publisher pub(this, topic);
  pub.reference_.reset(new XHolder(topic, this));
  return pub;
}

void thread_live(Block* node)
{
  std::string name_ = node->name;
  //std::string driving = node->data->driving[0].topic;// todo fix me for multiple driving
  int idx = 0;
  auto context = node->context;
  
  if (node->rate > 0)
  {
    // timer!
    double dt = 1.0/node->rate;
    dt = std::max(1.0, dt*1000000.0);
    while (ps_okay())
    {
      // grab latest data
      std::unique_lock<std::mutex> lk(context->stream_mutex);// todo use other mutex
      auto front = std::unique_ptr<HolderBase>(node->holder->clone());

      //printf("[%s] waking for %s\n", name_.c_str(), driving.c_str());
      lk.unlock();
      node->data->do_thing(pubsub::Time::now(), front->get());
      ps_sleep_us(dt);
    }
  }
  else
  {
    while (ps_okay())
    {
      //printf("[%s] sleeping for live\n", name_.c_str());
      std::unique_lock<std::mutex> lk(context->stream_mutex);// todo use other mutex
      if (node->data->queue.size() == 0)
      {
        if (node->data->timeout > 0)
        {
          auto res = node->data->cv.wait_for(lk, std::chrono::duration<double, std::ratio<1>>(node->data->timeout));
          if (res == std::cv_status::timeout)
          {
            //notify of a timeout
            lk.unlock();
            node->data->do_timeout(pubsub::Time::now());
            continue;
          }
        }
        else
        {
          node->data->cv.wait(lk);
        }
      }
      //printf("[%s] waking for %s\n", name_.c_str(), driving.c_str());
      
      auto front = std::move(node->data->queue.front());
      node->data->queue.pop_front();
      lk.unlock();
      node->data->do_thing(front->time, front->get()); 
    }
  }
  
  // let the callback go out of scope
  node->data->do_thing = {};
  node->data->do_timeout = {};
  if (node->data->do_shutdown)
  {
    node->data->do_shutdown();
  }
}

void thread_playback(Block* node, pubsub::Time start_time, pubsub::Time end_time)
{
  std::string name_ = node->name;
  // if there are no driving topics, warn and exit
  // todo maybe this should throw at node construction?
  if (node->data->driving.size() == 0)
  {
    printf("WARNING: Node %s has no driving topics so will not run.\n", name_.c_str());
    node->data->do_thing = {};
    node->data->do_timeout = {};
    if (node->data->do_shutdown)
    {
      node->data->do_shutdown();
    }
    return;
  }
  // we are a thread!
  auto ctx = node->context;
  auto& streams = ctx->streams;
  
  // todo need to get the start time of playback for this to work correctly
  pubsub::Time last_cb_time = start_time;
  
  pubsub::Time next_timer_time = start_time;
  next_timer_time.usec -= 1;
  
  {
    auto& sub = node->data->driving[0];
    ctx->stream_mutex.lock();
    streams[sub.topic].enqueue_placeholders(next_timer_time);
    ctx->stream_mutex.unlock();
  }
  bool first_loop = true;
  
  Sample timer_sample;
  Sub timer_sub;
  // todo can I condense the ctx->running check and ps_okay check to a single one?
  while (ps_okay() && ctx->running)
  {
    //printf("[%s] waiting for new message\n", name_.c_str());
    std::unique_lock<std::mutex> lk(ctx->stream_mutex);

    // check each of our subs to try and find representative data
    // we need to #1 grab the next driving message in time
    // then for any following messages, grab the first message of each not coming after that
    
    const Sample* driving_msg = 0;
    
    Sub* driving_sub = 0;
    
    if (node->rate != 0)
    {
      auto& sub = node->data->driving[0];
      
      // we're a timer!
      driving_sub = &sub;
      // dont need to wait for a message, just run
      timer_sample.time = next_timer_time;
      timer_sample.timer_sample = true;
      driving_msg = &timer_sample;
      next_timer_time += pubsub::Duration(1.0/node->rate);
      
      if (ctx->run_timers == false || timer_sample.time > end_time)
      {
        printf("told to stop running timers or hit end\n");
        break;
      }
      
      // publish any placeholders here
      //lk.unlock();
      streams[sub.topic].enqueue_placeholders(next_timer_time);
      //lk.lock();
    }
    else if (node->data->driving.size() == 1)
    {
      auto& sub = node->data->driving[0];
      if (node->data->counts[sub.topic] <= 0)
        node->data->cv.wait(lk);
      // todo check ctx running here? not necessary afaik
      if (node->data->counts[sub.topic] > 0)
        node->data->counts[sub.topic]--;
      
      //printf("[%s] woke for new message\n", name_.c_str());
      
      // first look for driving
      for (const auto& msg: streams[sub.topic].samples)
      {
        if (msg.index <= sub.last_msg_idx)
        {
          // already processed
          continue;
        }
        
        if (msg.is_end)
        {

        }
        else if (!msg.message && !msg.timer_sample)
        {
          // its a placeholder, wait for the real message
          printf("[%s] found placeholder on topic %s at time %li, waiting for real message\n", name_.c_str(), sub.topic.c_str(), msg.time.usec);
          break;
        }

        driving_msg = &msg;
        driving_sub = &sub;

        break;
      }
    }
    else
    {
      // since we have multiple driving, we have to do this fancily
      // loop through every driving to find the lowest timestamp > last_driving and assert
      // that each topic has one message
      
      // first make sure that every sub has a sample or EoF > last_driving
      for (auto& sub: node->data->driving)
      {
        auto& stream = streams[sub.topic];
        while (true)
        {
          bool found = false;
          for (const auto& msg: stream.samples)
          {
            if (msg.index <= sub.last_msg_idx)
            {
              // already processed
              continue;
            }
            
            if (msg.is_end)
            {

            }
            else if (!msg.message && !msg.timer_sample)
            {
              // its a placeholder, wait for the real message
              printf("[%s] found placeholder on topic %s, waiting for real message\n", name_.c_str(), sub.topic.c_str());
              break;
            }

            found = true;
            break;
          }
          
          if (found)
          {
            break;
          }
          else
          {
            // wait for a new message on this topic
            auto& counts = node->data->counts[sub.topic];
            if (counts <= 0)
            {
              node->data->cv.wait(lk);
            }
            if (!ctx->running)
            {
              goto exit;
            }
            if (counts > 0)
            {
              counts--;
            }
            
            printf("[%s] woke for new message %s\n", name_.c_str(), sub.topic.c_str());
          }
        }
      }
      
      // clear any other driving messages from the struct
      // todo only have to do this on the onees we dont select
      for (auto& sub: node->data->driving)
      {
        sub.clear();
      }
      
      // finally go through the available messages by time, and consume the one with the smallest timestamp
      // todo can probably make thist list above
      int i = 0;
      for (auto& sub: node->data->driving)
      {
        auto& stream = streams[sub.topic];
        for (const auto& msg: stream.samples)
        {
          if (msg.index <= sub.last_msg_idx)
          {
            // already processed
            continue;
          }

          //printf("[%s] checking with message on topic %s at time %li\n", name_.c_str(), sub.topic.c_str(), msg.time.usec);
          if (driving_msg == 0 || msg.time < driving_msg->time)
          {
            driving_msg = &msg;
            driving_sub = &sub;
          }
          break;
        }
        i++;
      }
      
      // then fall through to below
    }
    
    // no driving, dont bother yet
    if (!driving_msg)
    {
      printf("[%s] no driving message found yet for '%s'\n", name_.c_str(), node->data->driving[0].topic.c_str());
      continue;
    }
    
    // todo how do I get message times of non-driving messages?
    
    // exit if we hit the end
    // todo this logic is wrong with multiple driving at the end
    // we need to wait for the last stream to end to exit
    if (driving_msg->is_end)
    {
      printf("[%s] exiting thread, got last message\n", name_.c_str());
      break;
    }
    
    driving_sub->last_msg_idx = driving_msg->index;
    
    auto driving_copy = *driving_msg;// todo maybe dont really copy
    
    // update the sample with the driving
    bool call_callback = true;
    if (driving_copy.timer_sample)
    {
      printf("was timer sample at %f\n", driving_copy.time.usec/1e6);
      if (first_loop)
      {
        // first loop is guaranteed to have no messages, so skip searching
        // to bootstrap otherwise we can get stuck
        printf("was first loop, skipping look for messages\n");
        first_loop = false;
        goto call;
      }
    }
    else if (driving_copy.message)
    {
      driving_sub->cb(driving_copy.message->get(), driving_copy.time);
    }
    else
    {
      printf("[%s] was placeholder sample\n", name_.c_str());
      call_callback = false;
    }
    
    // check if we should trigger a timeout (and if so how many)
    if (node->data->timeout > 0)
    {
      pubsub::Duration timeout(node->data->timeout); 
      while (driving_msg->time > (last_cb_time + timeout)) {
        auto cb_time = last_cb_time + timeout;
        printf("calling timeout %li\n", cb_time.usec);
        lk.unlock();
        node->data->do_timeout(cb_time);
        lk.lock();
        last_cb_time = cb_time;
      }
      // only update the cb time if the message was real
      if (call_callback)
      {
        last_cb_time = driving_msg->time;
      }
    }
    
    // decrement reference count for msg

    //printf("decremented references on driving to %i\n", driving_msg->remaining);
    driving_msg->remaining--;
    if (driving_msg->remaining == 0)
    {
      // remove the sample
      //printf("[%s] freeing sample at %f on topic %s\n", name_.c_str(), driving_copy.time.toSec(), driving.c_str());
      
      streams[driving_sub->topic].samples.erase(*driving_msg);
      //printf("freed message on topic %s %li left\n", driving_sub->topic.c_str(), streams[driving].samples.size());
    }
    
    // okay, we now need to wait until we have a new enough non-driving so need a non-driving > than our driving time
    for (auto& sub: node->data->subs)
    {
      while (true)
      {
        //printf("[%s] looking for sample on topic %s\n", name_.c_str(), driving.c_str());
        // now find the messages
        bool after_found = false;
        const Sample* best_msg = 0;
        //todo how should this handle messages with the same timestamp?
        // maybe it doesnt need to, we dont generally have duplicates on the same topic
        
        // try and find the latest valid sample less than driving time
        // if we have placeholders less than driving time, fail out
        for (const auto& msg: streams[sub.topic].samples)
        {
          if (msg.is_end)
          {
            after_found = true;
            break;
          }
          else if (msg.message)
          {
            // valid, can count as the message
            if (msg.time < driving_copy.time)
            {
              best_msg = &msg;
            }
            else
            {
              after_found = true;
              break;
            }
          }
          else
          {
            // placeholder, can count as after found but not the actual message
            if (msg.time < driving_copy.time)
            {
              break;
            }
            else if (best_msg)
            {
              after_found = true;
              break;
            }
          }
        }
        
        if (!after_found && streams[sub.topic].publishers == 0)
        {
          // no publishers, this is fine
        }
        else if (!after_found)
        {
          printf("[%s] no new enough message found on topic %s, newest was %f vs %f, waiting\n", name_.c_str(), sub.topic.c_str(), best_msg ? best_msg->time.toSec() : -1.0f, driving_copy.time.toSec());
          
          // now wait
          auto& counts = node->data->counts[sub.topic];
          if (counts <= 0)
          {
            node->data->cv.wait(lk);
          }
          if (!ctx->running)
          {
            goto exit;
          }
          if (counts > 0)
          {
            counts--;
          }
          continue;
        }
        
        //printf("[%s] found sample on topic %s\n", name_.c_str(), driving.c_str());
        break;
      }
    }
    
    for (auto& sub: node->data->subs)
    {
      auto& stream = streams[sub.topic];
      // now find the messages
      const Sample* best_msg = 0;
      for (const auto& msg: stream.samples)
      {
        // todo should this be <=?
        if (msg.time < driving_copy.time)
        {
          best_msg = &msg;
        }
        else
        {
          break;
        }
      }
      
      if (best_msg && best_msg->is_end)
      {}
      else if (best_msg)
      {
        // store it for the callback
        if (best_msg->message)
        {
          sub.cb(best_msg->message->get(), best_msg->time);
        }
        
        //printf("subs loop\n");
        auto best_time = best_msg->time;
        auto best_index = best_msg->index;
        
        // dont use best_msg anymore as we are going to free it
        // remove references
        for (auto it = stream.samples.begin(); it != stream.samples.end();)
        {
          //printf("remove loop\n");
          const auto& msg = *it;
          if (msg.index >= sub.last_msg_idx && msg.index <= best_index)
          {
            // remove references
            msg.remaining--;
            if (msg.remaining == 0)
            {
              // remove somehow
              //printf("removed sample\n");
              it = stream.samples.erase(it);
              continue;
            }
          }
          else
          {
            break;
          }
          it++;
        }

        sub.last_msg_idx = best_index;
      }
    }
    
    //printf("[%s] running callback at time %f\n", name_.c_str(), driving_copy.time.toSec());

call:
    // now call back!
    lk.unlock();
    
    // skip any fake messages we grabbed
    if (!call_callback)
    {
      //printf("continuing\n");
      continue;
    }

    auto start = std::chrono::steady_clock::now();
    //todo add some metrics calculations here
    //lets track min and max runtimes as well as avg (how do we define average?)
    node->data->do_thing(driving_copy.time, node->holder->get());
    auto end = std::chrono::steady_clock::now();
    auto duration = end - start;
    auto good_duration = std::chrono::duration_cast<std::chrono::duration<double, std::chrono::seconds::period>>(duration).count();
    
    // todo track metrics
    //what do I call these blocks? should blocks be named?
    //I can describe the blocks by their inputs and outputs
    //printf("[%s] callback took %f\n", name_.c_str(), good_duration);
  }
  
  // todo need to run timeouts until end of playback
  // todo whats the end time? perhaps we require it be provided at start?
  /*if (node->data->timeout > 0)
  {
    pubsub::Duration timeout(node->data->timeout); 
    while (driving_msg->time > (last_cb_time + timeout)) {
      auto cb_time = last_cb_time + timeout;
      printf("calling timeout %li\n", cb_time.usec);
      node->data->do_timeout(cb_time);
      last_cb_time = cb_time;
    }
  }*/

exit:
  printf("[%s] exiting!\n", name_.c_str());
  
  //todo perhaps the executor should own the block/node?
  
  // delete callback so it goes out of scope along with anything it captured
  node->data->do_thing = {};
  node->data->do_timeout = {};
  if (node->data->do_shutdown)
  {
    node->data->do_shutdown();
  }
}

template <typename T>
void Block::start(std::function<void(const T&, pubsub::Time)> cb)
{
  auto holder = this->holder.get();
  
  this->data->do_thing = [cb, holder](pubsub::Time time, void* hldr)
  {
    T& a = *(T*)hldr;
    cb(a, time);
  };
}

template <typename T>
void Block::subscribe(T offset, int index, const std::string& topic, bool driving)
{
  // offset is a vector
  typedef typename member_pointer_value<T>::type ValueT;
  typedef typename member_pointer_class<T>::type ClassT;
  typedef typename ValueT::value_type SharedPtrT;
  typedef typename ValueT::value_type::element_type MsgT;
  auto message_holder = (ClassT*)this->holder->get();
  auto message_dest = &(message_holder->*offset);
  auto cb_holder = data.get();
  
  data->to_add.push_back([this, topic, cb_holder, message_dest, driving, index](Context* context)
  {
    std::unique_lock<std::mutex> lk(context->stream_mutex);
    auto& stream = context->streams[topic];
    stream.subscribers.push_back(*this);
    stream.topic = topic;
    stream.type = MsgT::GetDefinition()->name;
    if (stream.hash)
    {
      if (stream.hash != MsgT::GetDefinition()->hash)
      {
        throw std::runtime_error("Message hash mismatch");
      }
    }
    else
    {
      stream.hash = MsgT::GetDefinition()->hash;
    }
    stream.decode = [](const void* msg) -> HolderBase* {
      struct XHolder: public HolderBase
      {
        SharedPtrT msg;
        
        XHolder(void* ptr)
        {
          msg.reset((MsgT*)ptr);
        }
          
        void* get() override
        {
          return &msg;
        }
          
        HolderBase* clone() override { return new XHolder(*this); }
      };

      return new XHolder(MsgT::Decode(msg));
    };

    if (!context->is_playback)
    {
      printf("subscribing to %s\n", topic.c_str());
      auto sub = context->node->subscribe<MsgT>(topic, [context, topic, cb_holder, message_dest, driving, index, this](const std::shared_ptr<MsgT>& msg){
        //printf("got callback for topic %s\n", topic.c_str());
        
        auto& vec = *message_dest;
        if (vec.size() <= index)
        {
          vec.resize(index + 1);
        }
        vec[index] = msg;

        if (driving)
        {
          auto time = pubsub::Time::now();
          // trigger! need to copy the whole message dest
          std::unique_lock<std::mutex> lk(context->stream_mutex);
          auto clone = this->holder->clone();
          clone->time = time;
          cb_holder->queue.push_back(std::unique_ptr<HolderBase>(clone));
          cb_holder->cv.notify_one();
        }
      }, 100, 1);
      cb_holder->subscribers.push_back(sub);
    }
  });
  
  if (cb_holder->is_timer && driving)
  {
    throw std::invalid_argument("Cannot configure a topic as driving with a timer block.");
  }
  // todo remove this with destructor
  
  Sub s;
  s.topic = topic;
  s.cb = [message_dest, index](const void* msg, pubsub::Time time)
  {
    //printf("stored message\n");
    //printf("callback\n");
      //printf("shared\n");
    // ValueT should be a shared ptr to the message type
    auto ptr = (SharedPtrT*)msg;
      
    auto& vec = *message_dest;
    if (vec.size() <= index)
    {
      vec.resize(index + 1);
    }
    vec[index] = *ptr;
  };
  s.clear = [message_dest, index]() {
    auto& vec = *message_dest;
    if (vec.size() <= index)
    {
      vec.resize(index + 1);
    }
    vec[index] = {};
  };
  if (driving)
  {
    cb_holder->driving.push_back(s);
  }
  else
  {
    cb_holder->subs.push_back(s);
  }
      
  printf("added %s at topic %s\n", typeid(ValueT).name(), topic.c_str());
}

template <typename T>
void Block::subscribe(T offset, const std::string& topic, bool driving)
{
  typedef typename member_pointer_value<T>::type ValueT;
  typedef typename member_pointer_class<T>::type ClassT;
  typedef typename ValueT::element_type MsgT;

  auto message_holder = (ClassT*)this->holder->get();
  auto message_dest = &(message_holder->*offset);
  auto cb_holder = data.get();
  
  data->to_add.push_back([topic, this, cb_holder, message_dest, driving](Context* context)
  {
    auto& stream = context->streams[topic];
    stream.subscribers.push_back(*this);
    stream.topic = topic;
    stream.type = MsgT::GetDefinition()->name;
    if (stream.hash)
    {
      if (stream.hash != MsgT::GetDefinition()->hash)
      {
        throw std::runtime_error("Message hash mismatch");
      }
    }
    else
    {
      stream.hash = MsgT::GetDefinition()->hash;
    }
    stream.decode = [](const void* msg) -> HolderBase* {
      struct XHolder: public HolderBase
      {
        std::shared_ptr<MsgT> msg;
        
        XHolder(void* ptr)
        {
          msg.reset((MsgT*)ptr);
        }
          
        void* get() override
        {
          return &msg;
        }
          
        HolderBase* clone() override { return new XHolder(*this); }
      };

      return new XHolder(MsgT::Decode(msg));
    };
    
    if (!context->is_playback)
    {
      printf("subscribing to %s\n", topic.c_str());
      auto sub = context->node->subscribe<typename ValueT::element_type>(topic, [context, topic, cb_holder, message_dest, driving, this](const std::shared_ptr<typename ValueT::element_type>& msg){
        //printf("got callback for topic %s\n", topic.c_str());
        *message_dest = msg;

        if (driving)
        {
          auto time = pubsub::Time::now();
          // trigger! need to copy the whole message dest
          std::unique_lock<std::mutex> lk(context->stream_mutex);
          auto clone = this->holder->clone();
          clone->time = time;
          cb_holder->queue.push_back(std::unique_ptr<HolderBase>(clone));
          cb_holder->cv.notify_one();
        }
      }, 100, 1);
      cb_holder->subscribers.push_back(sub);
    }
  });
  
  if (cb_holder->is_timer && driving)
  {
    throw std::invalid_argument("Cannot configure a topic as driving with a timer block.");
  }

  // todo remove this with destructor
  
  Sub s;
  s.topic = topic;
  s.cb = [message_dest](const void* msg, pubsub::Time time)
  {
    //printf("stored message\n");
    //printf("callback\n");
      //printf("shared\n");
      // ValueT should be a shared ptr to the message type
    auto ptr = (ValueT*)msg;
    *message_dest = *ptr;
  };
  s.clear = [message_dest]()
  {
    *message_dest = {};
  };
  if (driving)
  {
    cb_holder->driving.push_back(s);
  }
  else
  {
    cb_holder->subs.push_back(s);
  }
      
  printf("added %s at topic %s\n", typeid(ValueT).name(), topic.c_str());
}

extern std::map<std::string, std::function<void*()>>* loaders;

#define CLASS_LOADER_REGISTER_CLASS_INTERNAL_WITH_MESSAGE(Derived, Base, UniqueID, Message) \
  namespace \
  { \
  struct ProxyExec ## UniqueID \
  { \
    typedef  Derived _derived; \
    typedef  Base _base; \
    ProxyExec ## UniqueID() \
    { \
      if (!std::string(Message).empty()) { \
        printf("%s", Message); \
      } \
      if (loaders == 0) { \
        loaders = new std::map<std::string, std::function<void*()>>(); \
      } \
      (*loaders)[ Message ] = []() { \
        return new Derived(); \
      }; \
    } \
private: \
  }; \
  static ProxyExec ## UniqueID g_register_plugin_ ## UniqueID; \
  }  // namespace

#define REGISTER_BLOCK(Derived, name) CLASS_LOADER_REGISTER_CLASS_INTERNAL_WITH_MESSAGE(Derived, Block, Derived, name)
