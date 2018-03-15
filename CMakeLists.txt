cmake_minimum_required(VERSION 3.2)

project(PosixFileLib VERSION 0.1 LANGUAGES CXX)

if(NOT LIBRT_LIBRARY)
    message( FATAL_ERROR "librt not found. This library depends on librt. Are you on a Linux system?" )
endif()

find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)

add_subdirectory (PosixFileLib)
add_subdirectory (PosixFileLibTestApp)


