set(ARDUINOJSON_SRC "${REPO_ROOT}/.pio/libdeps/gh_release/ArduinoJson/src")

add_executable(TaskSyncTest
  TaskSyncTest.cpp
  ${REPO_ROOT}/src/util/TaskSync.cpp
  ${REPO_ROOT}/src/PrioritiesStore.cpp
)

target_include_directories(TaskSyncTest PRIVATE
  ${REPO_ROOT}/test/priorities_store/stubs
  ${REPO_ROOT}/src
  ${REPO_ROOT}/src/util
  ${ARDUINOJSON_SRC}
)

target_link_libraries(TaskSyncTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(TaskSyncTest)
