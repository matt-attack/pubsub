
# Add source to this project's executable.
include_directories("../")

add_library (pubsub_cpp
  "Node.cpp"
 )
target_link_libraries(pubsub_cpp pubsub)
target_include_directories(pubsub_cpp PUBLIC ../include)
add_dependencies(pubsub_cpp pubsub)
set_property(TARGET pubsub_cpp PROPERTY CXX_STANDARD 11)

find_package( Threads )
target_link_libraries( pubsub_cpp ${CMAKE_THREAD_LIBS_INIT} )
