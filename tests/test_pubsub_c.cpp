
#include <pubsub/Node.h>
#include <pubsub/Publisher.h>
#include <pubsub/Subscriber.h>
#include <pubsub/System.h>
#include <pubsub/TCPTransport.h>

#include <pubsub/String.msg.h>

#include "mini_mock.hpp"
/*add test to make sure mismatched messages are detected and not received

also add a test to test subscriber/publisher numbers

add a way to add a timeout to tests

add a test for generic message handling

make the pose viewer also be able to view odom in pubviz
(maybe think of a way to view velocities)*/

//lets test queue size too

//so for that test, shove over N messages

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
  options.queue_size = 0;
  options.allocator = 0;
  options.ignore_local = false;

  static bool got_message = false;
  //options.preferred_transport = tcp ? 1 : 0;
  options.cb = [](void* message, unsigned int size, void* data2, const ps_msg_info_t* info)
  {
    got_message = true;
    // todo need to also assert we have the message type
    // which is tricky for udp...
    auto data = (struct pubsub__String*)pubsub__String_decode(message, &ps_default_allocator);
    printf("Got message: %s\n", data->value);
    EXPECT(strcmp(data->value, rmsg.value) == 0);
    free(data->value);
    free(data);
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

void latch_test(bool broadcast, bool tcp)
{
  struct ps_node_t node;
  ps_node_init(&node, "test_node", "", broadcast);

  struct ps_transport_t tcp_transport;
  ps_tcp_transport_init(&tcp_transport, &node);
  ps_node_add_transport(&node, &tcp_transport);

  struct ps_pub_t string_pub;
  ps_node_create_publisher(&node, "/data", &pubsub__String_def, &string_pub, true);

  struct ps_sub_t string_sub;

  struct ps_subscriber_options options;
  ps_subscriber_options_init(&options);
  options.preferred_transport = tcp ? 1 : 0;// tcp yo
  ps_node_create_subscriber_adv(&node, "/data", &pubsub__String_def, &string_sub, &options);

  // come up with the latched topic
  struct pubsub__String rmsg;
  rmsg.value = "Hello";
  ps_pub_publish_ez(&string_pub, &rmsg);

  bool got_message = false;

  // now spin and wait for us to get the published message
  while (ps_okay())
  {
    ps_node_spin(&node);// todo blocking wait first

    struct pubsub__String* data;
    while (data = (struct pubsub__String*)ps_sub_deque(&string_sub))
    {
      // user is responsible for freeing the message and its arrays
      printf("Got message: %s\n", data->value);
      EXPECT(strcmp(data->value, rmsg.value) == 0);
      got_message = true;
      free(data->value);
      free(data);//todo use allocator free
      goto done;
    }
    ps_sleep(1);
  }

done:
  EXPECT(got_message);
  ps_node_destroy(&node);
}

TEST(test_publish_subscribe_latched_multicast, []() {
  latch_test(false, false);
});

TEST(test_publish_subscribe_latched_broadcast, []() {
  latch_test(true, false);
});

TEST(test_publish_subscribe_latched_multicast_tcp, []() {
  latch_test(false, true);
});

TEST(test_publish_subscribe_latched_broadcast_tcp, []() {
  latch_test(true, true);
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
    free(data->value);
    free(data);//todo use allocator free
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

CREATE_MAIN_ENTRY_POINT();
