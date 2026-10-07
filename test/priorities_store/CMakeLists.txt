set(ARDUINOJSON_SRC "${REPO_ROOT}/.pio/libdeps/gh_release/ArduinoJson/src")

add_executable(PrioritiesStoreTest
  PrioritiesStoreTest.cpp
  ${REPO_ROOT}/src/PrioritiesStore.cpp
)

target_include_directories(PrioritiesStoreTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/src
  ${ARDUINOJSON_SRC}
)

target_link_libraries(PrioritiesStoreTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(PrioritiesStoreTest)
