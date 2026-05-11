#include "pubsub_pipeline/context.h"
#include "pubsub_pipeline/node_base.h"

void Context::add_node(std::unique_ptr<Block>&& new_node)
{
  new_node->set_context(this);
  node_mutex.lock();
  
  // throw if we have a node with the same name
  for (auto& node: nodes_)
  {
    if (node->name == new_node->name)
    {
      node_mutex.unlock();
      throw std::runtime_error("Context already contains a node with the name '" + node->name + "'. Block names must be unique within a context.");
    }
  }
  
  if (new_node->data->driving.size() == 0)
  {
    node_mutex.unlock();
    throw std::runtime_error("A block must contain at least one driving topic to function.");
  }
  
  
  nodes_.emplace_back(std::move(new_node));
    
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
  
void Context::add_node(Block* block)
{
  add_node(std::move(std::unique_ptr<Block>(block)));
}

void Context::join()
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

// todo make start time implicit like it is for timers
void Context::start_playback(pubsub::Time start_time, pubsub::Time end_time)
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

void Context::start()
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

void Context::stop()
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

void Context::abort()
{
  // shutdown all threads
  stream_mutex.lock();
  running = false;
  stream_mutex.unlock();
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


void Context::thread_live(Block* node)
{
  std::string name_ = node->name;
  int idx = 0;
  auto context = node->data->context;
  
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
      node->data->do_thing(front.first, front.second->get()); 
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

void Context::thread_playback(Block* node, pubsub::Time start_time, pubsub::Time end_time)
{
  std::string name_ = node->name;
  // if there are no driving topics, the code below generally infinite loops, so fail out
  // this case shouldnt occur anyways as we validate this on node add
  if (node->data->driving.size() == 0)
  {
    throw std::runtime_error("Unexpected.");
  }
  auto ctx = node->data->context;
  auto& streams = ctx->streams;
  
  // todo need to get the start time of playback for this to work correctly
  pubsub::Time last_cb_time = start_time;
  
  // if we can, try and start the timer just before the start of playback
  // so that it fires "dry" (without any messages)
  // this is useful for testing that people are properly checking for message presence
  pubsub::Time next_timer_time = start_time;
  if (next_timer_time.usec > 0)
  {
    next_timer_time.usec -= 1;
  }
  
  // if we are a timer place initial placeholders for our first timer update
  if (node->data->is_timer)
  {
    auto& sub = node->data->driving[0];
    std::unique_lock<std::mutex> lk(ctx->stream_mutex);
    streams[sub.topic].enqueue_placeholders(next_timer_time);
  }
  bool first_loop = true;
  
  Sample timer_sample;
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
      
      // we're a timer! just make fake driving messages since we always know when we plan to run
      driving_sub = &sub;
      timer_sample.time = next_timer_time;
      driving_msg = &timer_sample;
      next_timer_time += pubsub::Duration(1.0/node->rate);
      
      if (ctx->run_timers == false || timer_sample.time > end_time)
      {
        printf("told to stop running timers or hit end\n");
        break;
      }
      
      // publish any placeholders for the next update, this is important
      // as it can break deadlocks due to topic cycles
      // note this function needs locks held, but we already hold them
      streams[sub.topic].enqueue_placeholders(next_timer_time);
    }
    else if (node->data->driving.size() == 1)
    {
      auto& sub = node->data->driving[0];
      if (node->data->counts[sub.topic] <= 0)
        node->data->cv.wait(lk);
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
        else if (!msg.message)
        {
          // its a placeholder, wait for the real message
          printf("[%s] found placeholder on topic %s at time %f, waiting for real message\n", name_.c_str(), sub.topic.c_str(), msg.time.usec/1e6);
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
            else if (!msg.message)
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
      // todo only have to do this on the ones we don't select
      for (auto& sub: node->data->driving)
      {
        sub.clear();
      }
      
      // finally go through the available messages by time, and consume the one with the smallest timestamp
      // todo can probably make this list above
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
      }
    }
    
    // no driving, dont bother yet
    if (!driving_msg)
    {
      printf("[%s] no driving message found yet for '%s'\n", name_.c_str(), node->data->driving[0].topic.c_str());
      continue;
    }
    
    // exit if we hit the end
    if (driving_msg->is_end)
    {
      printf("[%s] exiting thread, got last message\n", name_.c_str());
      break;
    }
    
    driving_sub->last_msg_idx = driving_msg->index;
    
    auto driving_copy = *driving_msg;// todo maybe dont really copy
    
    // update the sample with the driving
    bool call_callback = true;
    if (node->data->is_timer)
    {
      printf("was timer sample at %f\n", driving_copy.time.usec/1e6);
      if (first_loop)
      {
        // first cycle is guaranteed to have no messages, so skip searching
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
      while (driving_msg->time > (last_cb_time + timeout))
      {
        auto cb_time = last_cb_time + timeout;
        printf("calling timeout %li\n", cb_time.usec);
        lk.unlock();
        // todo we should probably call this with data just with no driving data
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
    driving_msg->remaining--;
    //driving_msg->owners.erase(name_);// for debugging
    if (driving_msg->remaining == 0)
    {
      // remove the sample
      //printf("[%s] freeing sample at %f on topic %s\n", name_.c_str(), driving_copy.time.toSec(), driving_sub->topic.c_str());
      
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
        
        auto best_time = best_msg->time;
        auto best_index = best_msg->index;
        
        // dont use best_msg anymore as we are going to free it
        // remove references to any messages we already used
        for (auto it = stream.samples.begin(); it != stream.samples.end();)
        {
          const auto& msg = *it;
          if (msg.index >= sub.last_msg_idx && msg.index <= best_index)
          {
            // remove references
            msg.remaining--;
            //msg.owners.erase(name_); // for debugging
            if (msg.remaining == 0)
            {
              it = stream.samples.erase(it);
              continue;
            }
          }
          else if (msg.index > best_index)
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
  
  // todo need to run timeouts until end of playback or simulation
  // for this to work I need the end time for simulation which we dont keep track of
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
  
  // delete callbacks so anything they capture go out of scope (such as publishers)
  node->data->do_thing = {};
  node->data->do_timeout = {};
  if (node->data->do_shutdown)
  {
    node->data->do_shutdown();
  }
}
