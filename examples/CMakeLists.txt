

# Add source to this project's executable.
include_directories("../")

add_executable(simple_pub "simple_pub.c")
target_link_libraries(simple_pub pubsub pubsub_msgs)
add_dependencies(simple_pub pubsub pubsub_msgs)

add_executable(simple_sub "simple_sub.c")
target_link_libraries(simple_sub pubsub pubsub_msgs)
add_dependencies(simple_sub pubsub pubsub_msgs)

add_executable(binding_test "binding_test.c")
target_link_libraries(binding_test pubsub_shared pubsub_msgs)
add_dependencies(binding_test pubsub_shared pubsub_msgs)

add_executable(simple_pub_cpp "simple_pub.cpp")
target_link_libraries(simple_pub_cpp pubsub_cpp pubsub_msgs)
add_dependencies(simple_pub_cpp pubsub_cpp pubsub_msgs)
set_property(TARGET simple_pub_cpp PROPERTY CXX_STANDARD 11)

add_executable(simple_logger_cpp "simple_logger.cpp")
target_link_libraries(simple_logger_cpp pubsub_cpp pubsub_msgs)
add_dependencies(simple_logger_cpp pubsub_cpp pubsub_msgs)
set_property(TARGET simple_logger_cpp PROPERTY CXX_STANDARD 11)

add_executable(simple_sub_cpp "simple_sub.cpp")
target_link_libraries(simple_sub_cpp pubsub_cpp pubsub_msgs)
add_dependencies(simple_sub_cpp pubsub_cpp pubsub_msgs)
set_property(TARGET simple_sub_cpp PROPERTY CXX_STANDARD 11)

add_executable(read_param "read_param.c")
target_link_libraries(read_param pubsub pubsub_msgs)
add_dependencies(read_param pubsub pubsub_msgs)

#install(TARGETS pubsubtest
#  ARCHIVE DESTINATION "s"
#  LIBRARY DESTINATION "s"
#  RUNTIME DESTINATION "s"
#)
