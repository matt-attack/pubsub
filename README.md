# pubsub (name pending)
A lightweight publisher subscriber system inspired by ROS. Intended for use on both embedded and full blown PCs.

After having worked with ROS for the past few years, I have found a great appreciation for its tooling and simplicity. 
This is an attempt to bring that kind of simplicity to more platforms, with simpler tooling and few (ideally none) dependency requirements.

I believe one of the ROS developers themselves said that you should not pay for features you do not use. 
This is a principal that I am getting behind for this project.

## What is this package?

This contains the implementation of the core of the middleware as well as simple debugging tools. It also contains higher level C++ wrappers that implement additional functionality (such as timers).

It handles networking, discovery, advertisement and code generation for serializing messages.

It can be used directly or wrapped up into larger apis giving closer to ROS like interfaces.

## Why do I care?

Middlewares like this and ROS help make it easy to develop standard interfaces for your programs to communicate. 
This helps facilitate code re-use by enabling you to write more modular code. 

As a bonus, this architecture can come in handy for debugging and visualization as it enables outside access to data flowing through your system.

## Why not just use ROS?

If you are in it for the ecosystem, definitely use ROS!

If you just want something small with the same functionality as ROS and easily works on most platforms, this could be a good option!

## Features

* Multicast or broadcast Publisher/Subscriber discovery
* Code generation for message serialization/deserialization
* UDP based best-effort networking (up to ~1500 bytes)
  * Optional publisher side per subscriber downsampling to conserve CPU/Network resources
* TCP based ROS-like networking for larger or more reliable data streams
* Message introspection tools to publish and subscribe to messages without having their definitions locally
  * A la "rostopic"
* System introspection tools to view connections between "nodes"
  * A la "rosnode"
* Simple C API
  * Easy to import into other languages
  * Lightweight enough for true embedded systems
* Higher Level C++ API
  * Provides zero-copy intraprocess message passing
  * Adds Thread-safety

## Features to Come

* Large UDP message support
* Master based discovery (better scalable than multicast)
* Topic remapping in C++
* Documentation
* A real name

## Supported Platforms

Anything with POSIX sockets
- Windows
- Linux (tested on Ubuntu variants)
- Arduino (tested on ESP32)

## Dependencies

A POSIX networking stack and the C or C++ standard library.

## Install and Build Instructions

These instructions work on both Windows and Linux as long as you have git installed

```bash
# Likely not necessary if you've done stuff like this before, not necessary (or possible) on Windows
sudo apt-get install build-essential git

# Clone the repository and change directory into it
git clone https://github.com/matt-attack/pubsub.git
cd pubsub

# Build the respository
cmake . -B build
cmake --build build
```

## Running Tests

You can use `ctest` to run tests on the library. To do that, build the package, then navigate to the `tests` directory in your build directory. There you can run `ctest` to execute them.

## Usage Instructions

### Introspection Tool (pubsub)

PubSub comes with a runtime introspection tool used to view connections in the system and look at data streaming within.

See the help text below for a list of commands it supports.

```
Usage: pubsub <verb> <subverb> (arg1) (arg2)
 Verbs:
   topic -
      list
      hz (topic name)
      bw (topic name)
      info (topic name)
      show (topic name)
      echo (topic name)
      pub (topic name) (message)
   node -
      list
      info (node name)
   param -
      set (param) (value)
```

### Message Generation

Add your msg files to your project in the `msg` folder then tell CMake to generate message files for them like below:

```
project("your_project")

# your CMake here

find_package( pubsub_msg_gen )

generate_messages(FILES 
  YourMessage.msg
)
```

See the `msg` folder for standard messages to use as examples.

This creates a target named `your_project_msgs` as well as generates `your_project_msgs/YourMessage.msg.h`.

To use the messages, simply include the generated file and add `your_project_msgs` to your link libraries list.


### Sending Messages

See `simple_pub.cpp` and `simple_pub.c` examples.

### Receiving Messages

See `simple_sub.cpp` and `simple_sub.c` examples.
