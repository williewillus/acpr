# declare project
cmake_minimum_required(VERSION 3.0)
project(acpr)

set(CMAKE_CXX_FLAGS "-std=c++14 -Wall")

# find boost
find_package(Boost 1.55 REQUIRED COMPONENTS filesystem system)
INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})

# get headers
INCLUDE_DIRECTORIES("include")

# get sources
file(GLOB SOURCES "src/*.cpp")

add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})
