#pragma once

#include "util.h"
#include "context.h"

#include <pubsub_cpp/Node.h>
#include <pubsub_cpp/Time.h>

#include <condition_variable>
#include <queue>
#include <set>
#include <atomic>
#include <memory>

#include <pthread.h>

// for now we just support driving + latched and timer

// Okay each component is a Node, each node can have multiple 

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
  
  inline void publish(const void* ptr, pubsub::Time time, uint32_t hash) const;
  
  inline void publish_end() const;
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
    
    // queue for callbacks, used in live mode
    std::deque<std::pair<pubsub::Time, std::unique_ptr<HolderBase>>> queue;
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
  
  inline void set_context(Context* ctx);
  
  virtual ~Block() {}
    
  template <typename T>
  void subscribe(T offset, const std::string& topic, bool driving = false);
  
  template <typename T>
  void subscribe(T offset, int index, const std::string& topic, bool driving = false);
  
  template <typename T>
  Publisher advertise(const std::string& topic);
  
  inline void timeout(double timeout_sec, std::function<void(pubsub::Time)> cb)
  {
    data->do_timeout = cb;
    data->timeout = timeout_sec;
  }
  
  template <typename T>
  void start(std::function<void(const T&, pubsub::Time time)> cb);
  
  inline void set_on_shutdown(std::function<void()> cb)
  {
    data->do_shutdown = cb;
  }
};

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

// Dummy node that has no blocks and drives execution in playback
struct MockNode: public PipelineBlock<int>
{
  MockNode(Context& ctx) : PipelineBlock<int>("mock") {
    set_context(&ctx);
  }

  ~MockNode() {
    shutdown();
  }

  inline void shutdown() {
    // stop all pubs and timers
  
  }
  
  inline Publisher advertise(const std::string& topic);
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
    stream.subscribers.push_back(this);
    stream.topic = timer_name;
    stream.publishers++;
  });
    
  // todo rename this as it isnt really "start"
  start([this](const T& msg, pubsub::Time time) { update(msg, time); });
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
    struct XHolder: public HolderBase
    {
      std::shared_ptr<T> msg;
        
      void* get() override
      {
        return &msg;
      }
        
      HolderBase* clone() override { return new XHolder(*this); }
    };
  
    auto h = new XHolder();
    h->msg = msg;
    s->second.enqueue_holder(time, h);
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
    stream.subscribers.push_back(this);
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
          cb_holder->queue.push_back({time, std::unique_ptr<HolderBase>(clone)});
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
    stream.subscribers.push_back(this);
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
          cb_holder->queue.push_back({time, std::unique_ptr<HolderBase>(clone)});
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
  
  Sub s;
  s.topic = topic;
  s.cb = [message_dest](const void* msg, pubsub::Time time)
  {
    //printf("stored message\n");
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
