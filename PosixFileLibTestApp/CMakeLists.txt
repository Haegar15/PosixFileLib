

add_executable(PosixFileTest 
    src/posixfile_test.cpp
)

target_link_libraries (PosixFileTest
    PRIVATE
        PosixFileLib
)
