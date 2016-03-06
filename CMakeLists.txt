CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

FUNCTION (SUBDIRS)
    FOREACH (dir ${ARGV})
        ADD_SUBDIRECTORY (${dir})
    ENDFOREACH(dir)
ENDFUNCTION (SUBDIRS)

PROJECT (ACTO-SOLUTION)

    INCLUDE_DIRECTORIES(multiprog)

    SET(CMAKE_CXX_FLAGS "-std=c++11 -pthread -Wl,--no-as-needed")

    IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        SET( CMAKE_CXX_COMPILER "g++-4.9" )
    ENDIF ()

    SUBDIRS (
        multiprog
        multiprog/util
        multiprog/core
        samples/cycles
        samples/ping-pong
    )
