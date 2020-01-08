
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
include_directories("../")

# Build the debugging tool
add_executable (info "pubsub.cpp")
target_link_libraries(info pubsub)
add_dependencies(info pubsub)

# Build the serialization generator
add_executable(pubsub_msg_gen "generator.cpp")
set_property(TARGET pubsub_msg_gen PROPERTY CXX_STANDARD 11)

# This is used to generate messages for a package
macro(generate_messages)
  set(options "DIRECTORY" "TARGET")
  cmake_parse_arguments(ARG "" "${options}" "FILES" "" ${ARGN})
  if(ARG_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "generate_messages() called with unused arguments: ${ARG_UNPARSED_ARGUMENTS}")
  endif()

  if(NOT ARG_DIRECTORY)
    set(ARG_DIRECTORY msg)
  endif()

  if (NOT ARG_TARGET)
    set(ARG_TARGET ${PROJECT_NAME}_msgs)
  endif()

  set(MESSAGE_DEPS)
  foreach (_file ${ARG_FILES})
    add_custom_command(
	  COMMAND $<TARGET_FILE:pubsub_msg_gen> ${_file}
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/${ARG_DIRECTORY}/
      DEPENDS ${CMAKE_SOURCE_DIR}/${ARG_DIRECTORY}/${_file} pubsub_msg_gen
      OUTPUT ${CMAKE_SOURCE_DIR}/${ARG_DIRECTORY}/${_file}.h
    )

    set(MESSAGE_DEPS ${MESSAGE_DEPS} ${CMAKE_SOURCE_DIR}/${ARG_DIRECTORY}/${_file}.h)
  endforeach()

  add_custom_target(${ARG_TARGET}
    DEPENDS ${MESSAGE_DEPS} 
  )
endmacro()

generate_messages(FILES 
  std_msgs__Joy.msg
  std_msgs__String.msg 
  std_msgs__Float.msg
)


# Build the test program
add_executable(pubsubtest "PubSubTest.cpp")
target_link_libraries(pubsubtest pubsub)
add_dependencies(pubsubtest pubsub pubsub_msgs)
set_property(TARGET pubsubtest PROPERTY CXX_STANDARD 11)

add_executable(throughput_test "throughput_test.cpp")
target_link_libraries(throughput_test pubsub pubsub_cpp)
add_dependencies(throughput_test pubsub pubsub_cpp pubsub_msgs)
set_property(TARGET throughput_test PROPERTY CXX_STANDARD 11)

# TODO: Add tests and install targets if needed.

install(TARGETS pubsub_msg_gen
  ARCHIVE DESTINATION "s"
  LIBRARY DESTINATION "s"
  RUNTIME DESTINATION "s"
)
