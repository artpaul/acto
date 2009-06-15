
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

FUNCTION (SUBDIRS)
    FOREACH (dir ${ARGV})
        ADD_SUBDIRECTORY (${dir})
    ENDFOREACH(dir)
ENDFUNCTION (SUBDIRS)


PROJECT (ACTO-SOLUTION)

    INCLUDE_DIRECTORIES(multiprog)

    SUBDIRS (
        multiprog
        multiprog/system
        multiprog/core
        multiprog/extension

        samples/cycles
        samples/ping-pong
    )
