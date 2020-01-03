# CMakeList.txt : CMake project for PubSubCMake, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
include_directories("../")

add_library (pubsub_cpp
  "Node.cpp"
  )

find_package( Threads )
target_link_libraries( pubsub_cpp ${CMAKE_THREAD_LIBS_INIT} )
