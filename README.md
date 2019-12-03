# pubsub (name pending)
A lightweight publisher subscriber system inspired by ROS. Intended for use on both embedded and full blown PCs.

After having worked with ROS for the past few years, I have found a great appreciation for its tooling and simplicity. 
This is an attempt to bring that kind of simplicity to more platforms, with simpler tooling and few (ideally none) dependency requirements.

I believe one of the ROS developers themselves said that you should not pay for features you do not use. 
This is a principal that I am getting behind for this project.

## What is this package?

This contains the C implementation of the core of the middleware as well as simple debugging tools.

It handles networking, topic discovery, advertisement and code generation for serializing messages.

It can be used directly or wrapped up into larger apis giving closer to ROS like interfaces.

## Why do I care?

Middlewares like this and ROS help make it easy to develop standard interfaces for your programs to communicate. 
This helps facilitate code re-use by enabling you to write more modular code. 

As a bonus, this architecture can come in handy for debugging and visualization as it enables outside access to data flowing through your system.

## Features

* Multicast or broadcast Publisher/Subscriber discovery
* UDP based best-effort networking (up to network MTU)
* Message introspection tools to publish and subscribe to messages without having their definitions locally
  * "rostopic"
* System introspection tools to view connections between "nodes"
  * "rosnode"
* Simple C API
  * Easy to import into other languages
* Publisher "latching"
  * Gives you the last published value when you subscribe

## Features to Come

* Large message support (> MTU size)
* Master based discovery (better scalable than multicast)
* TCP based networking
* Zero-copy intraprocess message passing (likely via a language specific wrapper)
* Higher level wrapper enabling topic remapping
* Documentation
* A real name
* Data recording tools (a la rosbag)

## Supported Platforms

Anything with POSIX sockets
- Windows
- Linux
- Arduino (ESP32 tested)

## Dependencies

Your operating system.
