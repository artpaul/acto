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
        multiprog/util/system
        multiprog/core
        multiprog/extension

        samples/cycles
        samples/ping-pong

        tests/thread
    )
