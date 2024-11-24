cmake_minimum_required(VERSION 3.22)
project(DistributedIntegral)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(worker worker.cpp)
add_executable(master master.cpp)

find_package(Threads REQUIRED)

include_directories(${CMAKE_SOURCE_DIR})
target_link_libraries(worker PRIVATE pthread)
target_link_libraries(master PRIVATE pthread)
