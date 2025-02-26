
#include <pubsub/Joy.msg.h>
#include <pubsub/Costmap.msg.h>
#include <pubsub/Path2D.msg.h>
#include <pubsub/String.msg.h>
#include <pubsub/Test.msg.h>

#include "mini_mock.hpp"

TEST(test_joy_serialization, []() {
	// try serializing then deserializing a message to make sure it all matches
  pubsub::msg::Joy msg;
	msg.buttons = 0x12345678;
	for (int i = 0; i < 8; i++)
		msg.axes[i] = i;
	ps_msg_t in = msg.Encode();

	auto* out = pubsub::msg::Joy::Decode(ps_get_msg_start(in.data));
	free(in.data);

	EXPECT(out->buttons == msg.buttons);
	for (int i = 0; i < 8; i++)
	{
		EXPECT(out->axes[i] == msg.axes[i]);
	}

	free(out);
});

TEST(test_string_cpp, []() {
  std::string value = "hi";
	// test C++ strings
	{
	  pubsub::msg::String msg;
	  EXPECT(msg.value.data() == 0)
	  EXPECT(msg.value == "");
	  msg.value = "apples";
	  EXPECT(msg.value == "apples");
	  EXPECT(msg.value == std::string("apples"));
	  EXPECT(strcmp(msg.value.c_str(), "apples") == 0);
	  msg.value = value;
	  EXPECT(msg.value == value);
	  EXPECT(msg.value == value.c_str());
	  EXPECT(strcmp(msg.value.c_str(), value.c_str()) == 0);
	  
	  // make sure the C++ string object is the same size as a pointer
	  EXPECT(sizeof(msg.value) == sizeof(char*));
	}
});

TEST(test_fixed_string_cpp, []() {
	// test C++ fixed strings
	{
	  FixedString<5> string;
	  string = "hi";
	  
	  EXPECT(string == "hi");
	  EXPECT(string == std::string("hi"));
	  EXPECT(strcmp(string.c_str(), "hi") == 0);
	  EXPECT(sizeof(string) == 5);
	}
	
	// test what happens when you assign too much
	{
	  FixedString<5> string;
	  EXPECT_THROWS([&](){
	    string = "hello paul";
	  }, "Too big.");
	}
});

TEST(test_array_vector_cpp, []() {
	// test C++ array vectors
	{
	  ArrayVector<uint32_t> vector;
	  EXPECT(sizeof(vector) == 4 + sizeof(char*));
	  EXPECT(vector.size() == 0);
	  vector.resize(4);
	  EXPECT(vector.size() == 4)
	  
	  for (auto& item: vector)
	  {
	    item = 55;
	  }
	  
	  EXPECT(vector[0] == 55);
	  EXPECT(vector[3] == 55);
	}
});

TEST(test_costmap_c_cpp, []() {
	// verify that the C type matches the C++ one memory wise
  pubsub::msg::Costmap msg;
	msg.width = 100;
	msg.height = 200;
	msg.resolution = 1.0;
	msg.left = 23;
	msg.bottom = 312;
	msg.data.resize(msg.width*msg.height);
	for (int i = 0; i < msg.data.size(); i++)
		msg.data[i] = i;

	EXPECT(sizeof(msg) == sizeof(pubsub__Costmap));

	auto cmsg = (pubsub__Costmap*)&msg;
	EXPECT(msg.data.size() == cmsg->data_length);
	EXPECT(msg.width == cmsg->width);
	for (int i = 0; i < msg.data.size(); i++)
	{
		if (msg.data[i] != cmsg->data[i])
			EXPECT(false);
	}
});

TEST(test_costmap_serialization, []() {
	// try serializing then deserializing a message to make sure it all matches
  pubsub::msg::Costmap msg;
	msg.width = 100;
	msg.height = 200;
	msg.resolution = 1.0;
	msg.left = 23;
	msg.bottom = 312;
	msg.data.resize(msg.width*msg.height);
	for (int i = 0; i < msg.data.size(); i++)
		msg.data[i] = i;

	ps_msg_t in = msg.Encode();

	auto* out = pubsub::msg::Costmap::Decode(ps_get_msg_start(in.data));
	free(in.data);

	EXPECT(out->width == msg.width);
	EXPECT(out->height == msg.height);
	EXPECT(out->resolution == msg.resolution);
	EXPECT(out->left == msg.left);
	EXPECT(out->bottom == msg.bottom);
	EXPECT(out->data.size() == msg.data.size());
	for (int i = 0; i < msg.data.size(); i++)
	{
		if (out->data[i] != msg.data[i])
			EXPECT(false);
	}
  delete out;
});

TEST(test_path2d_serialization, []() {
	// try serializing then deserializing a message to make sure it all matches
  pubsub::msg::Path2D msg;
	msg.frame = 100;
	msg.points.resize(123);
	for (int i = 0; i < msg.points.size(); i++)
	{
		msg.points[i].x = i*2;
		msg.points[i].y = i*2 + 1;
	}

	ps_msg_t in = msg.Encode();

	auto* out = pubsub::msg::Path2D::Decode(ps_get_msg_start(in.data));
	free(in.data);

	EXPECT(out->frame == msg.frame);
	EXPECT(out->points.size() == msg.points.size());
	for (int i = 0; i < msg.points.size(); i++)
	{
		if (out->points[i].x != msg.points[i].x)
			EXPECT(false);
		if (out->points[i].y != msg.points[i].y)
			EXPECT(false);
	}
	delete out;
});

TEST(test_path2d_copy, []() {
	// make sure copying a message works
  pubsub::msg::Path2D msg;
	msg.frame = 100;
	msg.points.resize(123);
	for (int i = 0; i < msg.points.size(); i++)
	{
		msg.points[i].x = i*2;
		msg.points[i].y = i*2 + 1;
	}

	pubsub::msg::Path2D msg2 = msg;

	EXPECT(msg2.frame == msg.frame);
	EXPECT(msg2.points.size() == msg.points.size());
	for (int i = 0; i < msg.points.size(); i++)
	{
		if (msg2.points[i].x != msg.points[i].x)
			EXPECT(false);
		if (msg2.points[i].y != msg.points[i].y)
			EXPECT(false);
	}
});

TEST(test_path2d_foreach, []() {
	// try serializing then deserializing a message, making sure the foreach loop over it works as expected
  pubsub::msg::Path2D msg;
	msg.frame = 100;
	msg.points.resize(3);
	for (int i = 0; i < msg.points.size(); i++)
	{
		msg.points[i].x = i*2;
		msg.points[i].y = i*2 + 1;
	}

	ps_msg_t in = msg.Encode();

	auto* out = pubsub::msg::Path2D::Decode(ps_get_msg_start(in.data));
	free(in.data);

	EXPECT(out->frame == msg.frame);
	EXPECT(out->points.size() == msg.points.size());
	int iters = 0;
	for (const auto& pt: msg.points)
	{
		EXPECT(pt.x == msg.points[iters].x)
		EXPECT(pt.y == msg.points[iters].y)
		iters++;
	}
	EXPECT(iters == msg.points.size());
	delete out;
});

void compare_struct(const pubsub::msg::Test::Struct& a, const pubsub::msg::Test::Struct& b)
{
	EXPECT(a.test_array_string == b.test_array_string);
	EXPECT(a.i1 == b.i1);
	EXPECT(a.i2 == b.i2);
	EXPECT(a.f3 == b.f3);
	EXPECT(a.d4 == b.d4);
	for (int i = 0; i < 3; i++)
    EXPECT(a.a5[i] == b.a5[i]);
}

void fill_struct(pubsub::msg::Test::Struct& a)
{
  a.test_array_string = "ok";
  a.i1 = rand();
  a.i2 = rand();
  a.f3 = rand()/100.0;
  a.d4 = rand()/100.0;
  for (int i = 0; i < 3; i++)
    a.a5[i] = rand()/100.0;
  a.test_enum = pubsub::msg::Test::TYPE_INT;
}

TEST(test_complex_message, []() {
	// try serializing then deserializing a message, making sure everything matches expectation
	EXPECT(sizeof(pubsub::msg::Test) == sizeof(pubsub__Test));
	EXPECT(sizeof(pubsub::msg::Test::Struct) == 56);
  pubsub::msg::Test msg;
  msg.test_int = rand();
  msg.test_string = "THIS IS A STRING";
  fill_struct(msg.test_struct);
  fill_struct(msg.test_structs[0]);
  fill_struct(msg.test_structs[1]);
  msg.test_struct_array.resize(4);
  for (int i = 0; i < 4; i++)
    fill_struct(msg.test_struct_array[i]);
  msg.test_bitmask[0] = pubsub::msg::Test::TYPE2_FLAG1;
  msg.test_bitmask[1] = pubsub::msg::Test::TYPE2_FLAG2;
  msg.test_bitmask[2] = pubsub::msg::Test::TYPE2_FLAG1 | pubsub::msg::Test::TYPE2_FLAG2;

	ps_msg_t in = msg.Encode();
  EXPECT(in.len == 420);// 56*7 + 4 + 4 + 3 + 17 = 392 + 11 + 17
  
  // Make sure deserialize iterators work correctly
  struct ps_deserialize_iterator iter = ps_deserialize_start(ps_get_msg_start((const char*)in.data), pubsub::msg::Test::GetDefinition());
	const struct ps_msg_field_t* field; uint32_t length; const void* ptr;
	std::vector<std::string> fields;
	while (ptr = ps_deserialize_iterate(&iter, &field, &length))
	{
	  fields.push_back(field->name);
	  
	  // check some values
	  if (strcmp(field->name, "test_bitmask") == 0)
	  {
	    EXPECT(*(uint8_t*)ptr == msg.test_bitmask[0]);
	  }
	  if (strcmp(field->name, "test_int") == 0)
	  {
	    EXPECT(*(uint32_t*)ptr == msg.test_int);
	  }
	}
	EXPECT(fields.size() == 6);
	EXPECT(fields[0] == "test_int");
	EXPECT(fields[5] == "test_bitmask");
	
	auto* out = pubsub::msg::Test::Decode(ps_get_msg_start(in.data));
	free(in.data);
	
	EXPECT(msg.test_int == out->test_int);
	EXPECT(msg.test_string == out->test_string);
  compare_struct(msg.test_struct, out->test_struct);
  compare_struct(msg.test_structs[0], out->test_structs[0]);
  compare_struct(msg.test_structs[1], out->test_structs[1]);
  EXPECT(msg.test_struct_array.size() == out->test_struct_array.size())
  for (int i = 0; i < 4; i++)
    compare_struct(msg.test_struct_array[i], out->test_struct_array[i]);
  for (int i = 0; i < 3; i++)
    EXPECT(msg.test_bitmask[i] == out->test_bitmask[i]);
});

CREATE_MAIN_ENTRY_POINT();
