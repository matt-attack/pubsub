
# Add source to this project's executable.
include_directories("../")

# Build the debugging tool
add_executable (info "pubsub.cpp")
target_link_libraries(info pubsub)
add_dependencies(info pubsub)
set_property(TARGET info PROPERTY CXX_STANDARD 11)

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
	  COMMAND $<TARGET_FILE:pubsub_msg_gen> ${_file} ${PROJECT_NAME} ${CMAKE_CURRENT_BINARY_DIR}/include/${PROJECT_NAME}/${_file}.h
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${ARG_DIRECTORY}/
      DEPENDS ${PROJECT_SOURCE_DIR}/${ARG_DIRECTORY}/${_file} pubsub_msg_gen
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/include/${PROJECT_NAME}/${_file}.h
    )

    set(MESSAGE_DEPS ${MESSAGE_DEPS} ${CMAKE_CURRENT_BINARY_DIR}/include/${PROJECT_NAME}/${_file}.h)
  endforeach()

  add_custom_target(${ARG_TARGET}_mkdir ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/include/${PROJECT_NAME}/)

  add_custom_target(${ARG_TARGET}_all ALL
    DEPENDS ${MESSAGE_DEPS} ${ARG_TARGET}_mkdir
  )

  add_library(${ARG_TARGET} INTERFACE)
  add_dependencies(${ARG_TARGET} ${ARG_TARGET}_mkdir ${ARG_TARGET}_all)
  target_include_directories(${ARG_TARGET} INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/include/)
endmacro()

generate_messages(FILES 
  Joy.msg
  String.msg 
  Float.msg
  Image.msg
  Costmap.msg
  PointCloud.msg
  Marker.msg
  Path.msg
  Parameters.msg
  Path.msg
  Pose.msg
  GPS.msg
  Log.msg
  Path2D.msg
  Odom.msg
)


# Build the test program
add_executable(pubsubtest "PubSubTest.cpp")
target_link_libraries(pubsubtest pubsub pubsub_msgs)
add_dependencies(pubsubtest pubsub pubsub_msgs)
set_property(TARGET pubsubtest PROPERTY CXX_STANDARD 11)

add_executable(throughput_test "throughput_test.cpp")
target_link_libraries(throughput_test pubsub pubsub_cpp pubsub_msgs)
add_dependencies(throughput_test pubsub pubsub_cpp pubsub_msgs)
set_property(TARGET throughput_test PROPERTY CXX_STANDARD 11)
