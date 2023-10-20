
add_library (yab
	"${CMAKE_CURRENT_LIST_DIR}/source/yab/private/yab.cpp"
)

set_property (TARGET yab PROPERTY CXX_STANDARD 20)

set (YAB_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/source")

target_include_directories (yab PRIVATE ${YAB_INCLUDE_DIRECTORIES})

find_package(yaml-cpp CONFIG REQUIRED)
target_link_libraries(yab PUBLIC yaml-cpp::yaml-cpp)
