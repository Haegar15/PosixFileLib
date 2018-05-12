project(PosixFileLib VERSION 0.1 LANGUAGES CXX)

add_library(PosixFileLib
    src/PosixAsyncFile.cpp
)

target_include_directories(PosixFileLib
    PUBLIC
        include
    PRIVATE
        src
)

target_link_libraries(PosixFileLib
    PUBLIC
        Boost::boost
    PRIVATE
        ${LIBRT_LIBRARY}
        Boost::system
        Threads::Threads
)
