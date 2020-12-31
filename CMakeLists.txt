cmake_minimum_required(VERSION 3.0)
project(dandelion)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -O2")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -O2")

include_directories(${PROJECT_SOURCE_DIR}/include)
link_directories(${PROJECT_BINARY_DIR}/lib /usr/local/lib /usr/lib)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
SET(CMAKE_EXE_LINKER_FLAGS "-pthread")

aux_source_directory(${PROJECT_SOURCE_DIR}/src SOURCE_FILES)
add_executable(dandelion ${SOURCE_FILES})
 
install(TARGETS dandelion DESTINATION bin)

enable_testing()
add_test(name dandelionTest command dandelion)
add_custom_target(run command dandelion)

