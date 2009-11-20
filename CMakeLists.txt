
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

FUNCTION (SUBDIRS)
    FOREACH (dir ${ARGV})
        ADD_SUBDIRECTORY (${dir})
    ENDFOREACH(dir)
ENDFUNCTION (SUBDIRS)


PROJECT (ACTO-SOLUTION)

    INCLUDE_DIRECTORIES(multiprog)

    SET(CMAKE_CXX_FLAGS "-std=c++0x")

    SUBDIRS (
        multiprog
        multiprog/system
        multiprog/core
        multiprog/extension
        multiprog/remote
        multiprog/remote/libsocket

        samples/cycles
        samples/ping-pong
        samples/sotest

        tests/thread
    )
