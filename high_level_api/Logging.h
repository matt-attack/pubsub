#pragma once

#include "../high_level_api/Node.h"
#include "../msg/std_msgs__String.msg.h"

#include <string>

namespace pubsub
{

class Logger
{
	pubsub::Publisher<std_msgs::String> pub_;
public:
	Logger(Node& node) : pub_(node, "/log")
	{

	}

	void log(const char* file, int line, const char* message)
	{
		std::string out;
		out += message;
		out += ' ';
		out += file;
		out += ':';
		out += std::to_string(line);

		std_msgs::String msg;
		msg.value = (char*)out.c_str();
		pub_.publish(msg);
		msg.value = 0;
	}
};

#define PUBSUB_INFO(logger, message) \
	logger.log(__FILE__, __LINE__, message)
}