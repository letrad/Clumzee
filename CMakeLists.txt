cmake_minimum_required(VERSION 3.25)
project(Clumzee)

set(CMAKE_CXX_STANDARD 17)

add_executable(Clumzee main.cpp)

target_include_directories(Clumzee PRIVATE libs)

