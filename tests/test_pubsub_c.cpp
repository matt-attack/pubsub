
#include <pubsub/Node.h>
#include <pubsub/Publisher.h>
#include <pubsub/Subscriber.h>
#include <pubsub/System.h>
#include <pubsub/TCPTransport.h>

#include <pubsub/String.msg.h>
#include <pubsub/PointCloud.msg.h>

#include "mini_mock.hpp"
/*add test to make sure mismatched messages are detected and not received

also add a test to test subscriber/publisher numbers

add a test for generic message handling

make the pose viewer also be able to view odom in pubviz
(maybe think of a way to view velocities)*/

TEST(test_publish_subscribe_generic, []() {
  struct ps_node_t node;
  ps_node_init(&node, "test_node", "", true);

  struct ps_transport_t tcp_transport;
  ps_tcp_transport_init(&tcp_transport, &node);
  ps_node_add_transport(&node, &tcp_transport);

  struct ps_pub_t string_pub;
  ps_node_create_publisher(&node, "/data", &pubsub__String_def, &string_pub, true);

  // come up with the latched topic
  static struct pubsub__String rmsg;
  rmsg.value = "Hello";
  ps_pub_publish_ez(&string_pub, &rmsg);

  struct ps_sub_t string_sub;

  struct ps_subscriber_options options;
  ps_subscriber_options_init(&options);
  //options.skip = skip;
  options.allocator = 0;
  options.ignore_local = false;

  static bool got_message = false;
  //options.preferred_transport = tcp ? 1 : 0;
  options.cb_raw = [](void* message, unsigned int size, void* data2, const ps_msg_info_t* info)
  {
    got_message = true;
    // todo need to also assert we have the message type
    // which is tricky for udp...
    auto data = (struct pubsub__String*)pubsub__String_decode(message, &ps_default_allocator);
    printf("Got message: %s\n", data->value);
    EXPECT(strcmp(data->value, rmsg.value) == 0);
    pubsub__String_free(data, &ps_default_allocator);
    
    free(message);
  };
  ps_node_create_subscriber_adv(&node, "/data", 0, &string_sub, &options);

  // now spin and wait for us to get the published message
  while (ps_okay() && !got_message)
  {
    ps_node_spin(&node);// todo blocking wait first

    ps_sleep(1);
  }

done:
  EXPECT(got_message);
  ps_node_destroy(&node);
});

void latch_test_cb(bool broadcast, bool tcp)
{
  struct ps_node_t node;
  ps_node_init(&node, "test_node", "", broadcast);

  struct ps_transport_t tcp_transport;
  ps_tcp_transport_init(&tcp_transport, &node);
  ps_node_add_transport(&node, &tcp_transport);

  struct ps_pub_t string_pub;
  ps_node_create_publisher(&node, "/data", &pubsub__String_def, &string_pub, true);

  // come up with the latched topic
  static struct pubsub__String rmsg;
  rmsg.value = "Hello";
  ps_pub_publish_ez(&string_pub, &rmsg);

  static bool got_message = false;
  got_message = false;

  struct ps_sub_t string_sub;
  struct ps_subscriber_options options;
  ps_subscriber_options_init(&options);
  options.preferred_transport = tcp ? 1 : 0;// tcp yo
  options.cb = [](void* message, unsigned int size, void* cb_data, const struct ps_msg_info_t* info) 
  {
    auto data = (struct pubsub__String*)message;
    EXPECT(strcmp(data->value, rmsg.value) == 0);
    pubsub__String_free(data, &ps_default_allocator);
    got_message = true;
  };
  ps_node_create_subscriber_adv(&node, "/data", &pubsub__String_def, &string_sub, &options);


  // now spin and wait for us to get the published message
  while (ps_okay() && !got_message)
  {
    ps_node_spin(&node);
    ps_sleep(1);
  }

  EXPECT(got_message);

  ps_node_destroy(&node);
}

TEST(test_publish_subscribe_latched_cb_multicast, []() {
  latch_test_cb(false, false);
});

TEST(test_publish_subscribe_latched_cb_broadcast, []() {
  latch_test_cb(true, false);
});

TEST(test_publish_subscribe_latched_cb_multicast_tcp, []() {
  latch_test_cb(false, true);
});

TEST(test_publish_subscribe_latched_cb_broadcast_tcp, []() {
  latch_test_cb(true, true);
});

// test sending a very large message
TEST(test_publish_subscribe_large, []() {
  struct ps_node_t node;
  ps_node_init(&node, "test_node", "", true);

  struct ps_transport_t tcp_transport;
  ps_tcp_transport_init(&tcp_transport, &node);
  ps_node_add_transport(&node, &tcp_transport);

  struct ps_pub_t string_pub;
  ps_node_create_publisher(&node, "/data", &pubsub__PointCloud_def, &string_pub, true);

  // come up with the latched topic
  static struct pubsub__PointCloud rmsg;
  rmsg.num_points = 100000000;// 10 million points!
  rmsg.point_type = pubsub::msg::PointCloud::POINT_XYZ;
  rmsg.data_length = rmsg.num_points*4*3;//3 floats per point
  rmsg.data = (uint8_t*)malloc(rmsg.data_length);
  ps_pub_publish_ez(&string_pub, &rmsg);

  struct ps_sub_t string_sub;

  struct ps_subscriber_options options;
  ps_subscriber_options_init(&options);
  options.allocator = 0;
  options.ignore_local = false;

  static bool got_message = false;
  options.preferred_transport = 1;
  options.cb_raw = [](void* message, unsigned int size, void* data2, const ps_msg_info_t* info)
  {
    got_message = true;
    printf("Got message\n");
    // todo need to also assert we have the message type
    // which is tricky for udp...
    auto data = (struct pubsub__PointCloud*)pubsub__PointCloud_decode(message, &ps_default_allocator);
    printf("Decoded message\n");
    EXPECT(data->num_points == rmsg.num_points);
    pubsub__PointCloud_free(data, &ps_default_allocator);
    free(message);
  };
  ps_node_create_subscriber_adv(&node, "/data", 0, &string_sub, &options);

  // now spin and wait for us to get the published message
  while (ps_okay() && !got_message)
  {
    ps_node_spin(&node);// todo blocking wait first

    ps_sleep(1);
  }

done:
  EXPECT(got_message);
  ps_node_destroy(&node);
});

TEST(test_publish_subscribe_latched_skip, []() {
  // test that we still get the latched message even if we want to skip messages
  struct ps_node_t node;
  ps_node_init(&node, "test_node", "", false);

  struct ps_transport_t tcp_transport;
  ps_tcp_transport_init(&tcp_transport, &node);
  ps_node_add_transport(&node, &tcp_transport);

  struct ps_pub_t string_pub;
  ps_node_create_publisher(&node, "/data", &pubsub__String_def, &string_pub, true);

  // come up with the latched topic
  static struct pubsub__String rmsg;
  rmsg.value = "Hello";
  ps_pub_publish_ez(&string_pub, &rmsg);

  static bool got_message = false;
  got_message = false;

  struct ps_sub_t string_sub;
  struct ps_subscriber_options options;
  ps_subscriber_options_init(&options);
  options.skip = 100;
  options.cb = [](void* message, unsigned int size, void* cb_data, const struct ps_msg_info_t* info) 
  {
    auto data = (struct pubsub__String*)message;
    EXPECT(strcmp(data->value, rmsg.value) == 0);
    pubsub__String_free(data, &ps_default_allocator);
    got_message = true;
  };
  ps_node_create_subscriber_adv(&node, "/data", &pubsub__String_def, &string_sub, &options);

  // now spin and wait for us to get the published message
  while (ps_okay() && !got_message)
  {
    ps_node_spin(&node);
    ps_sleep(1);
  }

  EXPECT(got_message);

  ps_node_destroy(&node);
});

TEST(test_publish_subscribe_skip, []() {
  // test that skip works correctly
  struct ps_node_t node;
  ps_node_init(&node, "test_node", "", false);

  struct ps_transport_t tcp_transport;
  ps_tcp_transport_init(&tcp_transport, &node);
  ps_node_add_transport(&node, &tcp_transport);

  struct ps_pub_t string_pub;
  ps_node_create_publisher(&node, "/data", &pubsub__String_def, &string_pub, true);

  // come up with the latched topic
  static struct pubsub__String rmsg;
  rmsg.value = "Hello";
  ps_pub_publish_ez(&string_pub, &rmsg);

  struct ps_sub_t string_sub;
  struct ps_subscriber_options options;
  ps_subscriber_options_init(&options);
  options.skip = 10;
  static int received = 0;
  options.cb = [](void* message, unsigned int size, void* cb_data, const struct ps_msg_info_t* info) 
  {
    auto data = (struct pubsub__String*)message;
    EXPECT(strcmp(data->value, rmsg.value) == 0);
    pubsub__String_free(data, &ps_default_allocator);
    received++;
  };
  ps_node_create_subscriber_adv(&node, "/data", &pubsub__String_def, &string_sub, &options);
  
  // first spin and wait for connection
  while (ps_okay() && ps_pub_get_subscriber_count(&string_pub) == 0)
  {
    ps_node_spin(&node);
    ps_sleep(1);
  }
  
  // now spin and publish
  for (int i = 0; i < 100; i++)
  {
    ps_node_spin(&node);
    ps_pub_publish_ez(&string_pub, &rmsg);
    ps_sleep(1);
  }
  
  // finally count the number of messages
  EXPECT(received == 10);

  ps_node_destroy(&node);
});

CREATE_MAIN_ENTRY_POINT();
