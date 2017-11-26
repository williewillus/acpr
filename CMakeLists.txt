# declare project
cmake_minimum_required(VERSION 3.0)
project(acpr)

set(CMAKE_CXX_FLAGS "-g -std=c++14 -Wall -pedantic")

# find aio libs
find_library(aio_LIBRARIES aio)

# find boost libs
find_package(Boost 1.55 REQUIRED COMPONENTS filesystem system)
# INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})

# get headers
INCLUDE_DIRECTORIES("include")

# get sources
file(GLOB SOURCES "src/*.cpp")

add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES} ${aio_LIBRARIES})
