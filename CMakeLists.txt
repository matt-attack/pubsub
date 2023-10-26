cmake_minimum_required (VERSION 3.5.1)

project ("pubsub")

find_package( Threads )

if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

# Include sub-projects with non-exported targets.
add_subdirectory("tools")
add_subdirectory("examples")

set(PUBSUB_VERBOSE 0)

# Build the core exports
add_library (pubsub 
  "src/Events.c"
  "src/Node.c"
  "src/Publisher.c"
  "src/Subscriber.c"
  "src/Serialization.c"
  "src/System.c"
  "src/UDPTransport.c"
)
if (UNIX)
else()
target_link_libraries(pubsub winmm)
endif()
set_target_properties(pubsub PROPERTIES DEBUG_POSTFIX "d")
target_include_directories(pubsub PUBLIC include/)
if (PUBSUB_VERBOSE)
target_compile_definitions(pubsub PUBLIC PUBSUB_VERBOSE)
endif()

# Build the core exports
add_library (pubsub_shared SHARED
  "src/Events.c"
  "src/Node.c"
  "src/Publisher.c"
  "src/Subscriber.c"
  "src/Serialization.c"
  "src/System.c"
  "src/UDPTransport.c"
  "src/Bindings.c")
if (UNIX)
else()
target_link_libraries(pubsub_shared winmm)
endif()
set_target_properties(pubsub_shared PROPERTIES DEBUG_POSTFIX "d")
target_include_directories(pubsub_shared PUBLIC include/)
add_dependencies(pubsub_shared pubsub_msgs)
if (PUBSUB_VERBOSE)
target_compile_definitions(pubsub_shared PUBLIC PUBSUB_VERBOSE)
endif()

add_library (pubsub_cpp
  "high_level_api/Node.cpp"
 )
target_link_libraries(pubsub_cpp pubsub ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(pubsub_cpp PUBLIC include)
add_dependencies(pubsub_cpp pubsub)
set_property(TARGET pubsub_cpp PROPERTY CXX_STANDARD 11)
set_target_properties(pubsub_cpp PROPERTIES DEBUG_POSTFIX "d")

#set(${PROJECT_NAME}_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/include
#    CACHE INTERNAL "${PROJECT_NAME}: Include Directories" FORCE)

function(my_export_target _target _include_dir)
    file(
        WRITE "${CMAKE_CURRENT_BINARY_DIR}/${_target}Config.cmake"
        "
            include(\"\$\{CMAKE_CURRENT_LIST_DIR\}/${_target}Targets.cmake\")
            set_property(
                TARGET ${_target}
                APPEND PROPERTY
                    INTERFACE_INCLUDE_DIRECTORIES \"${_include_dir}\"
            )
            set(${_target}_INCLUDE_DIRS \"${_include_dir}\")
            set(${_target}_LIBRARIES ${_target})
            macro(generate_messages)
                set(options \"DIRECTORY\" \"TARGET\")
                cmake_parse_arguments(ARG \"\" \"\${options}\" \"FILES\" \"\" \${ARGN})
                if(ARG_UNPARSED_ARGUMENTS)
                    message(FATAL_ERROR \"generate_messages() called with unused arguments: \${ARG_UNPARSED_ARGUMENTS}\")
                endif()

                if(NOT ARG_DIRECTORY)
                    set(ARG_DIRECTORY msg)
                endif()

                if (NOT ARG_TARGET)
                    set(ARG_TARGET \${PROJECT_NAME}_msgs)
                endif()

                set(MESSAGE_DEPS)
                foreach (_file \${ARG_FILES})
                    add_custom_command(
	                    COMMAND pubsub_msg_gen \${_file} \${PROJECT_NAME} ../include/\${PROJECT_NAME}/\${_file}.h
                        WORKING_DIRECTORY \${PROJECT_SOURCE_DIR}/\${ARG_DIRECTORY}/
                        DEPENDS \${PROJECT_SOURCE_DIR}/\${ARG_DIRECTORY}/\${_file} pubsub_msg_gen
                        OUTPUT \${PROJECT_SOURCE_DIR}/\${ARG_DIRECTORY}/../include/\${PROJECT_NAME}/\${_file}.h
                    )

                    set(MESSAGE_DEPS \${MESSAGE_DEPS} \${PROJECT_SOURCE_DIR}/\${ARG_DIRECTORY}/../include/\${PROJECT_NAME}/\${_file}.h)
                endforeach()

                add_custom_target(\${ARG_TARGET}
                    DEPENDS \${MESSAGE_DEPS} 
                )
            endmacro()
        "
    )

    export(TARGETS ${_target} FILE "${CMAKE_CURRENT_BINARY_DIR}/${_target}Targets.cmake")

    # NOTE: The following call can pollute your PC's CMake package registry
    export(PACKAGE ${_target})
endfunction(my_export_target)

my_export_target(pubsub "${CMAKE_CURRENT_SOURCE_DIR}/include")
my_export_target(pubsub_cpp "${CMAKE_CURRENT_SOURCE_DIR}/include")
my_export_target(pubsub_msg_gen "${CMAKE_CURRENT_SOURCE_DIR}/include")
#my_export_target(pubsub_msgs "${CMAKE_CURRENT_SOURCE_DIR}")

install (TARGETS pubsub
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include
)

install (DIRECTORY include/pubsub
  DESTINATION include
)

