
#include <pubsub/Node.h>
#include <pubsub/Publisher.h>
#include <pubsub/Subscriber.h>
#include <pubsub/System.h>
#include <pubsub/TCPTransport.h>

#include <pubsub/String.msg.h>
#include <pubsub/Int.msg.h>
#include <pubsub/PointCloud.msg.h>

#include <pubsub_pipeline/node_base.h>

#include <vector>
#include <set>

#include "mini_mock.hpp"

TEST(test_playback_basic, []()
{
  // Validate that messages are received in order and everything exits properly
  Context context;
  
  struct Data
  {
    pubsub::msg::Int::SharedPtr msg;
  };
  
  std::vector<int> received;

  // Create the subscriber block
  auto pb_node = std::make_unique<PipelineBlock<Data>>("test");
  pb_node->subscribe(&Data::msg, "/data", true);
  pb_node->update([&](const Data& data, pubsub::Time time)
  {
    received.push_back(data.msg->value);
  });
  context.add_node(std::move(pb_node));
  
  // Create mock node to publish and drive the execution of the pipeline
  MockNode mock(context);
  auto pub = std::make_shared<Publisher>(mock.advertise("/data"));
  
  // Actually start receiving nodes
  context.start_playback(pubsub::Time(1), pubsub::Time(2));
  
  //todo test throw if someone creates a pub after start
  
  auto pose = pubsub::msg::IntSharedPtr(new pubsub::msg::Int);
  pose->value = 0;
  pub->publish(*pose, pubsub::Time(1));
  pose->value = 1;
  pub->publish(*pose, pubsub::Time(2));
  
  // make all publishers go out of scope, this automatically ends timers too
  pub.reset();
  
  // wait for system to run until end
  context.join();
  
  EXPECT(received.size() == 2);
  EXPECT(received[0] == 0);
  EXPECT(received[1] == 1);
  
  // todo times of 0 break timers
});

TEST(test_playback_multidriving, []()
{
  // Validate that we get all driving messages in the expected order
  Context context;
  
  struct Data
  {
    pubsub::msg::Int::SharedPtr msg1, msg2;
  };
  
  std::vector<std::pair<int, int>> received;

  // Create the subscriber block
  auto pb_node = std::make_unique<PipelineBlock<Data>>("test");
  pb_node->subscribe(&Data::msg1, "/data1", true);
  pb_node->subscribe(&Data::msg2, "/data2", true);
  pb_node->update([&](const Data& data, pubsub::Time time)
  {
    if (data.msg1)
    {
      received.push_back({1, data.msg1->value});
    }
    else
    {
      received.push_back({2, data.msg2->value});
    }
  });
  context.add_node(std::move(pb_node));
  
  // Create mock node to publish and drive the execution of the pipeline
  MockNode mock(context);
  auto pub1 = std::make_shared<Publisher>(mock.advertise("/data1"));
  auto pub2 = std::make_shared<Publisher>(mock.advertise("/data2"));
  
  // Actually start receiving nodes
  context.start_playback(pubsub::Time(1), pubsub::Time(4));
  
  auto pose = pubsub::msg::IntSharedPtr(new pubsub::msg::Int);
  pose->value = 0;
  pub1->publish(*pose, pubsub::Time(1));
  pose->value = 1;
  pub2->publish(*pose, pubsub::Time(2));
  pub1.reset();// end stream 1 first
  
  pose->value = 2;
  pub2->publish(*pose, pubsub::Time(3));
  pose->value = 3;
  pub2->publish(*pose, pubsub::Time(4));
  pub2.reset();
  
  // wait for system to run until end
  context.join();
  
  EXPECT(received.size() == 4);
  
  // todo times of 0 break timers
  
  EXPECT(received[0].first == 1);
  EXPECT(received[0].second == 0);
  EXPECT(received[1].first == 2);
  EXPECT(received[1].second == 1);
  EXPECT(received[2].first == 2);
  EXPECT(received[2].second == 2);
  EXPECT(received[3].first == 2);
  EXPECT(received[3].second == 3);
});

/*TEST(test_playback_buffering, []()
{
  // Validate that nothing gets too far ahead
  Context context;
  
  struct Data
  {
    pubsub::msg::Int::SharedPtr msg;
  };
  
  std::vector<int> received;

  // Create the subscriber block
  volatile bool wait = true;
  auto pb_node = std::make_unique<PipelineBlock<Data>>("test");
  pb_node->subscribe(&Data::msg, "/data", true);
  pb_node->start([&](const Data& data, pubsub::Time time)
  {
    while (wait)
    {
      ps_sleep(10);
    }
    received.push_back(data.msg->value);
  });
  context.add_node(std::move(pb_node));
  
  // Create mock node to publish and drive the execution of the pipeline
  MockNode mock(context);
  auto pub = std::make_shared<Publisher>(mock.advertise("/data"));
  
  // Actually start receiving nodes
  context.start_playback(pubsub::Time(1), pubsub::Time(2));
  
  auto pose = pubsub::msg::IntSharedPtr(new pubsub::msg::Int);
  for (int i = 0; i < 100; i++) {
    pose->value = i;
    pub->publish(*pose, pubsub::Time(i, 1));
  }
  
  // make all publishers go out of scope, this automatically ends timers too
  pub.reset();
  
  // wait for system to run until end
  context.join();
  
  // inspect final state
  EXPECT(received.size() == 2);
  EXPECT(received[0] == 0);
  EXPECT(received[1] == 1);
});*/

TEST(test_playback_timeout, []()
{
  // Validate that messages are received in order and everything exits properly
  Context context;
  
  struct Data
  {
    pubsub::msg::Int::SharedPtr msg;
  };
  
  std::vector<int> received;
  std::vector<pubsub::Time> timeouts;

  // Create the subscriber block
  auto pb_node = std::make_unique<PipelineBlock<Data>>("test");
  {
    pb_node->subscribe(&Data::msg, "/data", true);
    pb_node->timeout(0.0005, [&](pubsub::Time timeout)
    {
      printf("called timeout\n");
      timeouts.push_back(timeout);
    });
    pb_node->update([&](const Data& data, pubsub::Time time)
    {
      received.push_back(data.msg->value);
    });
  }
  context.add_node(std::move(pb_node));
  
  // Create mock node to publish and drive the execution of the pipeline
  MockNode mock(context);
  auto pub = std::make_shared<Publisher>(mock.advertise("/data"));
  
  // Actually start receiving nodes
  context.start_playback(pubsub::Time(1000), pubsub::Time(4000));
  
  auto pose = pubsub::msg::IntSharedPtr(new pubsub::msg::Int);
  pose->value = 0;
  pub->publish(*pose, pubsub::Time(1000));
  pose->value = 1;
  pub->publish(*pose, pubsub::Time(2000));
  pose->value = 2;
  pub->publish(*pose, pubsub::Time(4000));
  
  // make all publishers go out of scope, this automatically ends timers too
  pub.reset();
  
  // wait for system to run until end
  context.join();
  
  // inspect final state
  EXPECT(received.size() == 3);
  EXPECT(received[0] == 0);
  EXPECT(received[1] == 1);
  EXPECT(received[2] == 2);
  
  EXPECT(timeouts.size() == 4);
  EXPECT(timeouts[0].usec == 1500);
  EXPECT(timeouts[1].usec == 2500);
  EXPECT(timeouts[2].usec == 3000);
  EXPECT(timeouts[3].usec == 3500);
});

TEST(test_playback_chain, []()
{
  // Validate that messages are received in order and everything exits properly
  Context context;
  
  struct Data
  {
    pubsub::msg::Int::SharedPtr msg;
  };
  
  std::vector<int> received;

  // Create the subscriber block
  {
    auto pb_node = std::make_unique<PipelineBlock<Data>>("test");
    pb_node->subscribe(&Data::msg, "/data", true);
    auto ipub = pb_node->advertise<pubsub::msg::Int>("/data2");
    pb_node->update([ipub] (const Data& data, pubsub::Time time)
    {
      ipub.publish(data.msg, time);
    });
    // make sure ipub goes out of scope here
    context.add_node(std::move(pb_node));
  }
  
  // Create the subscriber block
  
  {
    auto pb_node2 = std::make_unique<PipelineBlock<Data>>("test2");
    pb_node2->subscribe(&Data::msg, "/data", true);
    pb_node2->update([&](const Data& data, pubsub::Time time)
    {
      received.push_back(data.msg->value);
    });
    context.add_node(std::move(pb_node2));
  }
  
  MockNode mock(context);
  auto pub = std::make_shared<Publisher>(mock.advertise("/data"));
  
  // Actually start receiving nodes
  context.start_playback(pubsub::Time(1), pubsub::Time(2));
  
  auto pose = pubsub::msg::IntSharedPtr(new pubsub::msg::Int);
  pose->value = 0;
  pub->publish(*pose, pubsub::Time(1));
  pose->value = 1;
  pub->publish(*pose, pubsub::Time(2));
  
  // make all publishers go out of scope, this automatically ends timers too
  pub.reset();
  
  // wait for system to run until end
  context.join();
  
  // inspect final state
  EXPECT(received.size() == 2);
  EXPECT(received[0] == 0);
  EXPECT(received[1] == 1);
});

TEST(test_playback_timer_sub, []()
{
  // Validate that the timer runs as expected and everything exits
  Context context;
  
  struct Data {};
  
  std::vector<pubsub::Time> received;

  auto pb_node = std::make_unique<PipelineTimer<Data>>("test", 1.0);
  pb_node->update([&](const Data& data, pubsub::Time time)
  {
    printf("loop\n");
    received.push_back(time);
  });// todo this is kinda annoying, have to remember to call it
  context.add_node(std::move(pb_node));
  
  MockNode mock(context);
  auto pub = std::make_shared<Publisher>(mock.advertise("/data"));
  
  context.start_playback(pubsub::Time(10, 0), pubsub::Time(21, 0));
  
  pub.reset();
  
  // wait for system to run until end
  context.join();
  
  EXPECT(received.size() == 12);
  EXPECT(received[0] == pubsub::Time(9, 999999));
  EXPECT(received.back() == pubsub::Time(20, 999999));
});

TEST(test_playback_timer_subscriber, []() {
  // Validate that the timer runs as expected and we get the messages we expect
  Context context;
  
  struct Data {
    pubsub::msg::Int::SharedPtr msg;
  };
  
  std::vector<pubsub::Time> received;
  std::vector<pubsub::msg::Int::SharedPtr> received_msgs;

  auto pb_node = std::make_unique<PipelineTimer<Data>>("test", 1.0);
  {
    pb_node->subscribe(&Data::msg, "/data", false);
    pb_node->update([&](const Data& data, pubsub::Time time)
    {
      //printf("loop\n");
      received.push_back(time);
      received_msgs.push_back(data.msg);
    });
  }
  context.add_node(std::move(pb_node));
  
  MockNode mock(context);
  auto pub = std::make_shared<Publisher>(mock.advertise("/data"));
  
  context.start_playback(pubsub::Time(10, 0), pubsub::Time(21, 0));
  
  auto msg = pubsub::msg::IntSharedPtr(new pubsub::msg::Int);
  msg->value = 1;
  pub->publish(*msg, pubsub::Time(10, 0));
  msg->value = 2;
  pub->publish(*msg, pubsub::Time(20, 0));
  pub.reset();
  
  // wait for system to run until end
  context.join();
  
  EXPECT(received.size() == 12);
  EXPECT(!received_msgs[0]);// we want the first timer run to be "dry" with no messages
  EXPECT(received_msgs[1]);// next one should have a message
  EXPECT(received_msgs[1]->value == 1);
  // it should only run once with the last message
  EXPECT(received_msgs[10]->value == 1);
  EXPECT(received_msgs[11]->value == 2);
});

TEST(test_validation, []()
{
  // Test that invalid configurations throw
  Context context;

  struct Data
  {
    pubsub::msg::Int::SharedPtr msg, msg2;
    std::vector<pubsub::msg::Int::SharedPtr> vec, vec2;
  };

  {
    auto pb_node = std::make_unique<PipelineTimer<Data>>("sim", 1.0);
    // todo maybe dont even give the option to a timer block to do this
    pb_node->subscribe(&Data::msg, "/cmd", false);
    EXPECT_THROWS_MESSAGE(pb_node->subscribe(&Data::msg2, "/cmd2", true),
                  "Cannot configure a topic as driving with a timer block.");

    // test throw if someone creates a timer with an invalid rate
    EXPECT_THROWS_TYPE(std::make_unique<PipelineTimer<Data>>("sim", 0.0), std::invalid_argument);
    EXPECT_THROWS_TYPE(std::make_unique<PipelineTimer<Data>>("sim", -1.0), std::invalid_argument);

    // should throw if duplicate subs are added, either with same topic or same destination
    EXPECT_THROWS_TYPE(pb_node->subscribe(&Data::msg2, "/cmd", false), std::runtime_error);
    EXPECT_THROWS_TYPE(pb_node->subscribe(&Data::msg, "/cmd2", false), std::runtime_error);

    // now check for the same with vectors. first check for duplicate name
    EXPECT_THROWS_TYPE(pb_node->subscribe(&Data::vec, 0, "/cmd", false), std::runtime_error);

    // should throw if subscriber offset and index is the same
    pb_node->subscribe(&Data::vec, 0, "/cmd10", false);
    pb_node->subscribe(&Data::vec, 1, "/cmd11", false);
    pb_node->subscribe(&Data::vec2, 1, "/cmd12", false);
    EXPECT_THROWS_TYPE(pb_node->subscribe(&Data::vec, 1, "/cmd13", false), std::runtime_error);

    // should throw when node is added if multiple things publish to the same topic
    auto node1 = std::make_unique<PipelineTimer<Data>>("sim2", 1.0);
    node1->advertise<pubsub::msg::Int>("/topic");

    auto node2 = std::make_unique<PipelineTimer<Data>>("sim3", 1.0);
    node2->advertise<pubsub::msg::Int>("/topic");

    context.add_node(std::move(node1));
    EXPECT_THROWS_TYPE(context.add_node(std::move(node2)), std::runtime_error);
  }
});

TEST(test_placeholders, []()
{
  // Test that placeholder maintainance works as expected
  Context context;

  auto& streams = context._streams();

  // add a dummy block
  struct Data
  {
    pubsub::msg::Int::SharedPtr msg;
  };
  auto pb_node = std::make_unique<PipelineBlock<Data>>("test");
  pb_node->set_context(&context);

  auto& s1 = streams["/data"];
  s1.topic = "/data";
  s1.driven_topics.push_back("/driven");
  s1.subscribers.push_back(pb_node->data);
  auto& s2 = streams["/driven"];
  s2.topic = "/driven";
  s2.subscribers.push_back(pb_node->data);

  struct Holder: public HolderBase
  {
    int data;

    void* get() override { return &data; }

    HolderBase* clone() override { return new Holder(*this); }
  };

  // now just enqueue some messages and inspect the state
  s1.enqueue_holder(pubsub::Time(1), new Holder());

  // this should have enqueued a message and a placeholder at the same time
  EXPECT(s1.samples.size() == 1);
  EXPECT(s1.samples.begin()->message);
  EXPECT(s1.samples.begin()->time == pubsub::Time(1));
  EXPECT(s2.samples.size() == 1);
  EXPECT(!s2.samples.begin()->message);
  EXPECT(s2.samples.begin()->time == pubsub::Time(1));

  // Now lets enqueue another placeholder.
  s1.enqueue_holder(pubsub::Time(2), new Holder());
  EXPECT(s2.samples.size() == 2);
  EXPECT(!s2.samples.rbegin()->message);
  EXPECT(s2.samples.rbegin()->time == pubsub::Time(2));

  // Now if I enqueue an actual message, it should remove it and the placeholder before it
  s2.enqueue_holder(pubsub::Time(2), new Holder());
  EXPECT(s2.samples.size() == 1);
  EXPECT(s2.samples.begin()->message);
  EXPECT(s2.samples.begin()->time == pubsub::Time(2));

  // Now enqueue some more placeholders then enqueue an end.
  // This should remove any trailing placeholders (effectively all placeholders since placeholders can only be at the end)
  s1.enqueue_holder(pubsub::Time(3), new Holder());
  s1.enqueue_holder(pubsub::Time(4), new Holder());
  s1.enqueue_holder(pubsub::Time(5), new Holder());
  EXPECT(s2.samples.size() == 4);
  s2.enqueue_end();
  EXPECT(s2.samples.size() == 2);
  EXPECT(s2.samples.rbegin()->is_end);
});

TEST(test_simulation_loop, []()
{
  // Test a simple simulation loop works
  Context context;
  
  struct Data
  {
    pubsub::msg::Int::SharedPtr msg;
  };
  
  std::vector<pubsub::Time> received;
  int position = 0;
  
  // The simulator is just a simple block that takes in a control message and runs on a timer
  {
    auto pb_node = std::make_unique<PipelineTimer<Data>>("sim", 1.0);
    pb_node->subscribe(&Data::msg, "/cmd", false);
    auto ipub = pb_node->advertise<pubsub::msg::Int>("/pose");
    pb_node->update([ipub, &position, &received](const Data& data, pubsub::Time time)
    {
      pubsub::msg::Int out;
      out.value = position + (data.msg ? data.msg->value : 0);
      position = out.value;
      printf("sim loop x: %li %s\n", out.value, data.msg ? "had cmd" : "no cmd");
      received.push_back(time);
      ipub.publish(out, time);
    });
    context.add_node(std::move(pb_node));
  }

  // Create the subscriber block
  {
    auto pb_node = std::make_unique<PipelineBlock<Data>>("test");
    pb_node->subscribe(&Data::msg, "/pose", true);
    auto ipub = pb_node->advertise<pubsub::msg::Int>("/cmd");
    pb_node->update([ipub, &context] (const Data& data, pubsub::Time time)
    {
      printf("control loop x: %li\n", data.msg->value);
      
      // just a very simple controller trying to aim at 5
      pubsub::msg::Int out;
      out.value = data.msg->value < 5 ? 1 : 0;
      ipub.publish(out, time);
      
      // completion condition
      if (data.msg->value == 5)
      {
        printf("hit completion condition stopping\n");
        context.stop();
      }
    });
    // make sure ipub goes out of scope here
    context.add_node(std::move(pb_node));
  }
  
  MockNode mock(context);
  auto pub = std::make_shared<Publisher>(mock.advertise("/data"));
  
  // Actually start receiving nodes
  context.start_playback(pubsub::Time(1), pubsub::Time(11, 0));
  
  // nothing subscribes to these, but they force timer updates between 0.999999 and 10.999999 seconds
  auto pose = pubsub::msg::IntSharedPtr(new pubsub::msg::Int);
  pose->value = 0;
  pub->publish(*pose, pubsub::Time(1, 0));
  pub->publish(*pose, pubsub::Time(10, 0));
  
  // make all publishers go out of scope, this automatically ends timers too
  pub.reset();
  
  // wait for system to run until end
  context.join();
  
  EXPECT(position == 5);
  EXPECT(received.size() == 7);
});

TEST(test_abort, []()
{
  // Test that aborting a context works in a simple case
  Context context;
  
  struct Data
  {
    pubsub::msg::Int::SharedPtr msg;
  };
  
  std::vector<pubsub::Time> received;
  {
    auto pb_node = std::make_unique<PipelineTimer<Data>>("sim", 1.0);
    pb_node->subscribe(&Data::msg, "/cmd", false);
    pb_node->update([&received, &context](const Data& data, pubsub::Time time)
    {
      received.push_back(time);
      context.abort();
    });
    context.add_node(std::move(pb_node));
  }
  
  MockNode mock(context);
  auto pub = std::make_shared<Publisher>(mock.advertise("/data"));
  
  // Actually start receiving nodes
  context.start_playback(pubsub::Time(1), pubsub::Time(11, 0));
  
  // nothing subscribes to these, but they force timer updates between 0.999999 and 10.999999 seconds
  auto pose = pubsub::msg::IntSharedPtr(new pubsub::msg::Int);
  pose->value = 0;
  pub->publish(*pose, pubsub::Time(1, 0));
  pub->publish(*pose, pubsub::Time(10, 0));
  
  // make all publishers go out of scope, this automatically ends timers too
  pub.reset();
  
  // wait for system to run until end
  context.join();

  EXPECT(received.size() == 1);
});

TEST(test_abort2, []()
{
  // Test that aborting a context works in a more complex case, a waiting PipelineBlock
  Context context;
  
  struct Data
  {
    pubsub::msg::Int::SharedPtr msg;
  };
  
  std::vector<pubsub::Time> received;
  {
    auto pb_node = std::make_unique<PipelineBlock<Data>>("sim");
    pb_node->subscribe(&Data::msg, "/data", true);
    pb_node->update([&received](const Data& data, pubsub::Time time)
    {
      received.push_back(time);
    });
    context.add_node(std::move(pb_node));
  }
  
  // Advertise but don't publish on the requested topic so that it gets stuck waiting
  MockNode mock(context);
  auto pub = std::make_shared<Publisher>(mock.advertise("/data"));
  
  // Actually start receiving nodes
  context.start_playback(pubsub::Time(0), pubsub::Time(100, 0));
  
  // IMPORTANT: make sure the pub stays in scope until after the join in this case
  // want to make sure abort actually does the job
  context.abort();
  
  // wait for system to run until end
  context.join();

  EXPECT(received.size() == 0);
});

TEST(test_simulation_loop_2, []()
{
  // Test a more complex simulation loop works
  Context context;

  struct Data
  {
    pubsub::msg::Int::SharedPtr msg, msg2;
  };

  std::vector<pubsub::Time> received, received2, received3;
  int position = 0;

  // The simulator is just a simple block that takes in a control message and runs on a timer
  {
    auto pb_node = std::make_unique<PipelineTimer<Data>>("sim", 1.0);
    pb_node->subscribe(&Data::msg, "/cmd2", false);
    auto ipub = pb_node->advertise<pubsub::msg::Int>("/pose");
    pb_node->update([ipub, &position, &received](const Data& data, pubsub::Time time)
    {
      pubsub::msg::Int out;
      out.value = position + (data.msg ? data.msg->value : 0);
      position = out.value;
      printf("sim loop x: %li %s\n", out.value, data.msg ? "had cmd" : "no cmd");
      received.push_back(time);
      ipub.publish(out, time);
    });
    context.add_node(std::move(pb_node));
  }

  // Create the subscriber block
  {
    auto pb_node = std::make_unique<PipelineBlock<Data>>("test");
    pb_node->subscribe(&Data::msg, "/pose", true);
    auto ipub = pb_node->advertise<pubsub::msg::Int>("/cmd");
    pb_node->update([ipub, &received2] (const Data& data, pubsub::Time time)
    {
      printf("control loop x: %li\n", data.msg->value);

      // just a very simple controller trying to aim at 5
      pubsub::msg::Int out;
      out.value = data.msg->value < 5 ? 1 : 0;
      ipub.publish(out, time);
      received2.push_back(time);
    });
    // make sure ipub goes out of scope here
    context.add_node(std::move(pb_node));
  }

  // Create the subscriber block
  {
    auto pb_node = std::make_unique<PipelineBlock<Data>>("test2");
    pb_node->subscribe(&Data::msg, "/cmd", true);
    pb_node->subscribe(&Data::msg2, "/fake", true);// try with two topics to validate multiple driving
    auto ipub = pb_node->advertise<pubsub::msg::Int>("/cmd2");
    pb_node->update([ipub, &received3] (const Data& data, pubsub::Time time)
    {
      printf("control loop 2: %li\n", data.msg->value);
      ipub.publish(data.msg, time);
      received3.push_back(time);
    });
    // make sure ipub goes out of scope here
    context.add_node(std::move(pb_node));
  }

  MockNode mock(context);
  auto pub = std::make_shared<Publisher>(mock.advertise("/data"));

  // Actually start receiving nodes
  context.start_playback(pubsub::Time(1), pubsub::Time(11, 0));

  // nothing subscribes to these, but they force timer updates between 0.999999 and 10.999999 seconds
  auto pose = pubsub::msg::IntSharedPtr(new pubsub::msg::Int);
  pose->value = 0;
  pub->publish(*pose, pubsub::Time(1, 0));
  pub->publish(*pose, pubsub::Time(10, 0));

  // make all publishers go out of scope, this automatically ends timers too
  pub.reset();

  // wait for system to run until end
  context.join();

  EXPECT(position == 5);
  EXPECT(received.size() == 12);
  EXPECT(received2.size() == 12);
  EXPECT(received3.size() == 12);
});

CREATE_MAIN_ENTRY_POINT();
