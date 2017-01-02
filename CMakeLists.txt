CMAKE_MINIMUM_REQUIRED(VERSION 3.1)

FUNCTION (SUBDIRS)
    FOREACH (dir ${ARGV})
        ADD_SUBDIRECTORY (${dir})
    ENDFOREACH(dir)
ENDFUNCTION (SUBDIRS)

PROJECT (ACTO-SOLUTION)

	SET (CMAKE_CXX_STANDARD 14)
	SET (CMAKE_CXX_STANDARD_REQUIRED ON)

    INCLUDE_DIRECTORIES(multiprog)

	IF (UNIX)
		SET (CMAKE_CXX_FLAGS "-pthread -Wl,--no-as-needed")
		SET (CMAKE_EXE_LINKER_FLAGS, "-lrt -lpthread")
	ELSEIF (WIN32)
		#
    ENDIF ()

    SUBDIRS (
        multiprog
        multiprog/util
        multiprog/core
        samples/cycles
        samples/ping-pong
    )
