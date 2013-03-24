CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

FUNCTION (SUBDIRS)
    FOREACH (dir ${ARGV})
        ADD_SUBDIRECTORY (${dir})
    ENDFOREACH(dir)
ENDFUNCTION (SUBDIRS)

PROJECT (ACTO-SOLUTION)

    INCLUDE_DIRECTORIES(multiprog)

    SET(CMAKE_CXX_FLAGS "-std=c++0x")

    IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        SET( CMAKE_CXX_COMPILER "g++-4.7" )
    ENDIF ()

    SUBDIRS (
        multiprog
        multiprog/util
        multiprog/core
        multiprog/remote
        multiprog/remote/libsocket
        samples/cycles
        samples/ping-pong
        samples/sotest
        tests/thread
    )
