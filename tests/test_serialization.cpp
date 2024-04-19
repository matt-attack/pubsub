
#include <pubsub/Joy.msg.h>
#include <pubsub/Costmap.msg.h>
#include <pubsub/Path2D.msg.h>

#include "mini_mock.hpp"

TEST(test_joy_serialization, []() {

	// try serializing then deserializing a message to make sure it all matches
    pubsub::msg::Joy msg;
	msg.buttons = 0x12345678;
	for (int i = 0; i < 8; i++)
		msg.axes[i] = i;
	ps_msg_t in = msg.Encode();

	auto* out = pubsub::msg::Joy::Decode(ps_get_msg_start(in.data));

	EXPECT(out->buttons == msg.buttons);
	for (int i = 0; i < 8; i++)
	{
		EXPECT(out->axes[i] == msg.axes[i]);
	}

	free(out);
});

TEST(test_costmap_serialization, []() {
	// try serializing then deserializing a message to make sure it all matches
    pubsub::msg::Costmap msg;
	msg.width = 100;
	msg.height = 200;
	msg.resolution = 1.0;
	msg.left = 23;
	msg.bottom = 312;
	msg.data_length = msg.width*msg.height;
	msg.data = (uint8_t*)malloc(msg.data_length);
	for (int i = 0; i < msg.data_length; i++)
		msg.data[i] = i;

	ps_msg_t in = msg.Encode();

	auto* out = pubsub::msg::Costmap::Decode(ps_get_msg_start(in.data));

	EXPECT(out->width == msg.width);
	EXPECT(out->height == msg.height);
	EXPECT(out->resolution == msg.resolution);
	EXPECT(out->left == msg.left);
	EXPECT(out->bottom == msg.bottom);
	EXPECT(out->data_length = msg.data_length);
	for (int i = 0; i < msg.data_length; i++)
	{
		EXPECT(out->data[i] == msg.data[i]);
	}
	free(out->data);
	free(out);
});

TEST(test_path2d_serialization, []() {
	// try serializing then deserializing a message to make sure it all matches
    pubsub::msg::Path2D msg;
	msg.frame = 100;
	msg.points_length = 123;
	msg.points = (pubsub__Path2D_Point*)malloc(sizeof(pubsub__Path2D_Point)*msg.points_length);
	for (int i = 0; i < msg.points_length; i++)
	{
		msg.points[i].x = i*2;
		msg.points[i].y = i*2 + 1;
	}

	ps_msg_t in = msg.Encode();

	auto* out = pubsub::msg::Path2D::Decode(ps_get_msg_start(in.data));

	EXPECT(out->frame == msg.frame);
	EXPECT(out->points_length = msg.points_length);
	for (int i = 0; i < msg.points_length; i++)
	{
		EXPECT(out->points[i].x == msg.points[i].x);
		EXPECT(out->points[i].y == msg.points[i].y);
	}
	free(out->points);
	free(out);
});

CREATE_MAIN_ENTRY_POINT();
