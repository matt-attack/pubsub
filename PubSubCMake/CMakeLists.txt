# CMakeList.txt : CMake project for PubSubCMake, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
include_directories("../")

add_executable (info "pubsub.cpp")
target_link_libraries(info pubsub)
add_dependencies(info pubsub)
# TODO: Add tests and install targets if needed.
