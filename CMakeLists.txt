cmake_minimum_required(VERSION 3.2)

project(PosixFileLib VERSION 0.1 LANGUAGES CXX)

find_package(Boost REQUIRED)
find_package(Threads REQUIRED)

add_subdirectory (PosixFileLib)
add_subdirectory (PosixFileLibTestApp)


