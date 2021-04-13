cmake_minimum_required (VERSION 3.1)

project (acto LANGUAGES CXX)
  find_package (Threads REQUIRED)

  if (UNIX)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -Wall -Werror")

    link_libraries (pthread)
  elseif (WIN32)
    #
  endif ()

  # Library sources.
  add_subdirectory (lib)

  # Determine whether this is a standalone project or
  # included by other projects.
  if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set (CMAKE_CXX_STANDARD 14)
    set (CMAKE_CXX_STANDARD_REQUIRED ON)
    set (CMAKE_CXX_EXTENSIONS OFF)

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -Wextra -Wshadow -Wsign-compare -Wshadow -Wwrite-strings -Wpointer-arith -Winit-self -Wconversion -Wno-sign-conversion")

    # Build samples.
    add_subdirectory (samples)
  endif ()
