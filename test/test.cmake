
enable_testing()

file(GLOB_RECURSE TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

add_executable(yab_test ${TEST_SOURCES})

set_property(TARGET yab_test PROPERTY CXX_STANDARD 20)

target_include_directories(yab_test PRIVATE ${YAB_INCLUDE_DIRECTORIES})
target_link_libraries(yab_test PRIVATE yab)

find_package(Catch2 3 REQUIRED)
target_link_libraries(yab_test PRIVATE Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(yab_test)