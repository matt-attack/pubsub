# CMakeList.txt : CMake project for PubSubCMake, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
add_library (pubsub 
  "Node.c"
  "Publisher.c"
  "Subscriber.c"
  "Serialization.c"
  "System.c"
  )


# TODO: Add tests and install targets if needed.
