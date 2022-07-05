cmake_minimum_required (VERSION 3.5.1)

project ("pubsub")

find_package( Threads )

# Include sub-projects with non-exported targets.
add_subdirectory("tools")
add_subdirectory("examples")

# Build the core exports
add_library (pubsub 
  "src/Events.c"
  "src/Node.c"
  "src/Publisher.c"
  "src/Subscriber.c"
  "src/Serialization.c"
  "src/System.c"
)
if (UNIX)
else()
target_link_libraries(pubsub winmm)
endif()
target_include_directories(pubsub PUBLIC include/)

# Build the core exports
add_library (pubsub_shared SHARED
  "src/Events.c"
  "src/Node.c"
  "src/Publisher.c"
  "src/Subscriber.c"
  "src/Serialization.c"
  "src/System.c"
  "src/Bindings.c")
if (UNIX)
else()
target_link_libraries(pubsub_shared winmm)
endif()
target_include_directories(pubsub_shared PUBLIC include/)


add_library (pubsub_cpp
  "high_level_api/Node.cpp"
 )
target_link_libraries(pubsub_cpp pubsub ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(pubsub_cpp PUBLIC include)
add_dependencies(pubsub_cpp pubsub)
set_property(TARGET pubsub_cpp PROPERTY CXX_STANDARD 11)

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
            #set_property(TARGET pubsub_msg_gen PROPERTY
            # IMPORTED_LOCATION "../InstallMyExe/bin/myexe")
            set(${_target}_INCLUDE_DIRS \"${_include_dir}\")
            if (UNIX)
            set(${_target}_LIBRARIES \"${CMAKE_CURRENT_BINARY_DIR}/${_target}.lib\")
            else()
            set(${_target}_LIBRARIES \"${CMAKE_CURRENT_BINARY_DIR}/${_target}.lib\" winmm)
            endif()
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

    export(TARGETS ${_target} pubsub_msg_gen FILE "${CMAKE_CURRENT_BINARY_DIR}/${_target}Targets.cmake")

    # NOTE: The following call can pollute your PC's CMake package registry
    #       See comments/alternatives below
    export(PACKAGE ${_target})
endfunction(my_export_target)

my_export_target(pubsub "${CMAKE_CURRENT_SOURCE_DIR}/include")
my_export_target(pubsub_cpp "${CMAKE_CURRENT_SOURCE_DIR}/include")
#my_export_target(pubsub_msgs "${CMAKE_CURRENT_SOURCE_DIR}")
