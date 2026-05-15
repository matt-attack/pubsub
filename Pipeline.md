# PubSub Pipeline

A framework for constructing "compute graphs" typical of which you would find in robots. It allows both live operation as well as deterministic simulation and replay with the same compute graph implementations.

Together, this enables you to write reliable tests for your robotics software that otherwise would have been slow or fragile.

This repository just contains the core library. You can find full featured examples and integrations with PubViz and Rucksack in another repo which is coming soon.

# Overview

Pipeline breaks up compute graphs into Streams and Blocks. A Stream can either be made up of a series of messages over time like you would find on a typical PubSub topic or a stream of time based triggers. A Block listens to and is "driven" by one or more streams calling callbacks that can result in the output of data to additional Streams. Each Block has its own thread where callbacks are executed.

The framework is not quite as flexable as you would find with typical "nodelets". But it is these restrictions that allow you to fearlessly write testable production code.

## Learning by Example

How the framework works is best shown through example.

### Basic Blocks

Below you can find one of the simplest possible Blocks. It takes in data on the stream `/input` and upon receipt, calls the provided update callback.

```cpp
  struct Data {
    pubsub::msg::Int::SharedPtr msg;
  };
  auto block = std::make_unique<pubsub::pipeline::Block<Data>>("receiver");
  // Tell the framework we want messages on /input to be stored in Data::msg
  block->subscribe(&Data::msg, "/input", true);
  block->set_update([](const Data& data, pubsub::Time time)
  {
    assert(data.input);
    printf("I received: %li at time %f\n", data.msg->value, time.toSec());
  });
```

However, this Block is essentially a dead-end for a Pipeline as it has no output Stream. Blocks like this can be useful for building tests or passing data out of the system but are rather atypical.

A more typical Block will take in one or more Streams and at least one output Stream. You can see an example of one below:

```cpp
  struct Data {
    pubsub::msg::Int::SharedPtr input;
  };
  auto block = std::make_unique<pubsub::pipeline::Block<Data>>("incrementer");
  // Tell the framework we want messages on /input to be stored in Data::msg
  block->subscribe(&Data::input, "/input", true);
  // Create a publisher on /output that we will use in the update callback
  auto pub = block->advertise<pubsub::msg::Int>("/output");
  block->set_update([pub](const Data& data, pubsub::Time time)
  {
    assert(data.input);
    auto output = *data.input;
    output.value += 1;
    pub.publish(output.msg, time);
  });
```

This now outputs a message on `/output` for each message on `/input` at the same timestamps.

### Basic Timers

The above examples only respond to input messages. What if we want to run code at a given rate, much like a ros::Timer?

For that you would use a Timer block which you can see an example of below:

```cpp
  struct Data {};// This block does not listen for any messages, just timer events.
  auto block = std::make_unique<pubsub::pipeline::Block<Data>>("timer", 10.0);// run at 10 Hz
  auto pub = block->advertise<pubsub::msg::Int>("/output");
  int counter = 0;
  block->set_update([pub, counter](const Data& data, pubsub::Time time) mutable
  {
    // Increment our output each time
    pubsub::msg::Int output;
    output.value = counter++;
    pub.publish(output, time);
    printf("I published: %li at time %f\n", output.value, time.toSec());
  });
```

This block runs every 100ms and publishes an incrementing number.

### Intermediate Blocks

The examples above are enough to build a basic pipeline that branches outwards. However, a typical system will need to take into account data from multiple sources. For example to produce a costmap one would need both perception and localization data.

In order to build something like this one you need to add multiple subscribers on your Block. However, you may have to stop and think how you want it to work. Notice how the calls to `block->subscribe()` above had the last argument as true in the examples above? This indicates that the topic is a "driving" one and receipt of that message will result in the update callback being executed. Each non-timer Block most have at least one of these to function. 

An important detail is that driving topics are mutually exclusive, meaning you only receive one each update. In example below the asserts will never fail:

```cpp
  struct Data {
    pubsub::msg::Int::SharedPtr input1, input2;
  };
  auto block = std::make_unique<pubsub::pipeline::Block<Data>>("adder");
  block->subscribe(&Data::input1, "/input1", true);
  block->subscribe(&Data::input2, "/input2", true);
  block->set_update([pub](const Data& data, pubsub::Time time)
  {
    if (data.input1)
    {
      assert(!data.input2);
      printf("Got input1: %i\n", data.input1->value);
    }
    if (data.input2)
    {
      assert(!data.input1);
      printf("Got input2: %i\n", data.input2->value);
    }
  });
```

Having multiple driving topics like above can be useful if you want to perform an operation each time either message arrives. However, in many cases you only want to perform an operation on arrival of some key message and just need to know the latest value of other messages. To do this you can use non-driving topics:

```cpp
  struct Data {
    pubsub::msg::Int::SharedPtr input, aux;
  };
  auto block = std::make_unique<pubsub::pipeline::Block<Data>>("adder");
  block->subscribe(&Data::input, "/input", true);// Driving
  block->subscribe(&Data::aux, "/aux", false);// Non-driving
  block->set_update([pub](const Data& data, pubsub::Time time)
  {
    assert(data.input);
    // There's no guarantee you'll have data on non-driving topics each update
    if (!data.aux)
    {
      printf("Got input: %i\n", data.input->value);
    }
    else 
    {
      printf("Got input: %i and had aux: %i\n", data.input->value, data.aux->value);
    }
  });
```

In this example it will always print out each value on `/input` and, if there was one, the value on `/aux` that came most recently before it.


### Intermediate Timers

Just like with Blocks, timers can also have subscribers. However, with Timers none of them can be driving. You can see an example of one before:

```cpp
  struct Data {
    pubsub::msg::Int::SharedPtr input;
  };
  auto block = std::make_unique<pubsub::pipeline::Block<Data>>("timer", 10.0);// run at 10 Hz
  block->subscribe(&Data::input, "/input");
  auto pub = block->advertise<pubsub::msg::Int>("/output");
  block->set_update([pub, counter](const Data& data, pubsub::Time time) mutable
  {
    // Again, there's no guarantee you'll have data on /input (a non-driving topic) each update
    if (data.input)
    {
      printf("Most recent input: %i at time %f\n", data.input->value, time.toSec());
    }
    else
    {
      printf("No value on /input yet at time %f\n", time.toSec());
    }
  });
```

This example will print out the latest value on the `/input` stream every 100ms.

### Running a Pipeline

Once you've built and instantiated all of your pipeline Blocks you need to somehow link them together and finally execute them. To do this you use a `Context` object like the example below:

```cpp
  pubsub::pipeline::Context context;
  // Create and add your blocks
  auto block1 = make_my_block1();
  context.add(std::move(block1));
  auto block2 = make_my_block2();
  context.add(std::move(block2));

  pubsub::Time start_time(10, 0);
  pubsub::Time end_time(20, 0);

  // Execute the pipeline from t=10 seconds to t=20 seconds
  context.start_playback(start_time, end_time);

  // Wait for the pipeline to finish execution
  context.join();
```

You would use an approach like above for running your system in playback, test, or simulation mode. Often when you are doing this you'd like to drive the execution using either recorded or programatically generated message. You can do this like below using the `MockBlock`:

```cpp
  pubsub::pipeline::Context context;
  // Create and add your blocks
  auto block1 = make_my_block1();
  context.add(std::move(block1));
  auto block2 = make_my_block2();
  context.add(std::move(block2));

  pubsub::Time start_time(10, 0);
  pubsub::Time end_time(20, 0);
  
  MockBlock mock(context);
  auto pub = std::make_shared<Publisher>(mock.advertise("/data"));

  // Execute the pipeline from t=10 seconds to t=20 seconds
  context.start_playback(start_time, end_time);
  
  // Publish driving messages between start and end time
  auto msg = pubsub::msg::IntSharedPtr(new pubsub::msg::Int);
  msg->value = 1;
  pub->publish(*msg, pubsub::Time(10, 0));
  msg->value = 2;
  pub->publish(*msg, pubsub::Time(15, 0));
  
  // Finally make the all mock publishers go out of scope when you are finished with them
  // otherwise the pipeline will never exit as it does not know if there are messages left
  pub.reset();

  // Wait for the pipeline to finish execution
  context.join();
```

### Running a Pipeline Live

Generally you also want to be able to run your pipeline live in real-time on an actual robot. To that you need to instantiate the `Context` with a node to use for publishing and subscribing to messages and start it slightly differently:

```cpp
  pubsub::pipeline::Context context(new pubsub::Node("/pipeline"));
  // Create and add your blocks
  auto block1 = make_my_block1();
  context.add(std::move(block1));
  auto block2 = make_my_block2();
  context.add(std::move(block2));

  // Execute the pipeline indefinitely
  context.start;

  // Wait for the pipeline to finish execution
  context.join();
```

