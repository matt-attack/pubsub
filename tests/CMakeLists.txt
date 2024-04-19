enable_testing()

set(tests "test_serialization")
foreach(test_file ${tests})
  set(test_source "${test_file}.cpp")
  message(STATUS "create ${test_file} executable from source: ${test_source}")
  add_executable("${test_file}" "${test_source}")
  target_include_directories("${test_file}" PUBLIC ../include)
  add_dependencies("${test_file}" pubsub pubsub_msgs)
  target_link_libraries("${test_file}" pubsub)

  # Auto populate the tests from test source file
  # Note : CMake must be reconfigured if tests are added/renamed/removed from test source file
  set(TEST_REGEX "^TEST\\(([A-Za-z0-9_]+).*")
  file(STRINGS ${test_source} detected_tests REGEX ${TEST_REGEX})
  foreach(test ${detected_tests})
      STRING(REGEX REPLACE ${TEST_REGEX} "\\1" test ${test})
      message(STATUS "  add test: ${test}")
      add_test(NAME ${test_file}_${test} COMMAND ${test_file} ${test})
  endforeach()
endforeach()
