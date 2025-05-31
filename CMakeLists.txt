cmake_minimum_required(VERSION 3.10)
# export CFLAGS=-g -o2
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O2")
project(TLD)


add_subdirectory(build-tldlibrary)
add_subdirectory(build-tlddemoapp)

