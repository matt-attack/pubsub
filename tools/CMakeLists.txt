# CMakeList.txt : CMake project for PubSubCMake, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
include_directories("../")

add_executable (info "pubsub.cpp")
target_link_libraries(info pubsub)
add_dependencies(info pubsub)


add_executable(pubsub_gen "generator.cpp")

#add_custom_target(test2_run
#  COMMAND generator.exe
#  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
#  COMMENT "run generated test2 in ${CMAKE_CURRENT_SOURCE_DIR}"
#  SOURCES ${test2_SOURCES}
#)


add_executable(pubsubtest "PubSubTest.cpp")
target_link_libraries(pubsubtest pubsub)
#add_dependencies(pubsubtest pubsub test2_run)

# TODO: Add tests and install targets if needed.
