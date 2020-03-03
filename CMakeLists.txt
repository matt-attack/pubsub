# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.5.1)

project ("pubsub")

# Include sub-projects.
add_subdirectory("tools")
add_subdirectory("src")
add_subdirectory("examples")
add_subdirectory("high_level_api")
