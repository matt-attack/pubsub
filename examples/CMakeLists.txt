# CMakeList.txt : CMake project for PubSubCMake, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
include_directories("../")

add_executable(simple_pub "simple_pub.c")
target_link_libraries(simple_pub pubsub)
add_dependencies(simple_pub pubsub pubsub_msgs)

add_executable(simple_sub "simple_sub.c")
target_link_libraries(simple_sub pubsub)
add_dependencies(simple_sub pubsub pubsub_msgs)

add_executable(simple_pub_cpp "simple_pub.cpp")
target_link_libraries(simple_pub_cpp pubsub)
add_dependencies(simple_pub_cpp pubsub_cpp pubsub_msgs)

add_executable(simple_logger_cpp "simple_logger.cpp")
target_link_libraries(simple_logger_cpp pubsub)
add_dependencies(simple_logger_cpp pubsub_cpp pubsub_msgs)

add_executable(simple_sub_cpp "simple_sub.cpp")
target_link_libraries(simple_sub_cpp pubsub)
add_dependencies(simple_sub_cpp pubsub_cpp pubsub_msgs)

#install(TARGETS pubsubtest
#  ARCHIVE DESTINATION "s"
#  LIBRARY DESTINATION "s"
#  RUNTIME DESTINATION "s"
#)
