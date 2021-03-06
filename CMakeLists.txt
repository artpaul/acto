cmake_minimum_required (VERSION 3.1)

project (acto LANGUAGES CXX)
  find_package (Threads REQUIRED)

  option (ACTO_BUILD_SAMPLES "Set to ON to build samples" OFF)

  # Determine whether this is a standalone project or
  # included by other projects.
  if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set (CMAKE_CXX_STANDARD 14)
    set (CMAKE_CXX_STANDARD_REQUIRED ON)
    set (CMAKE_CXX_EXTENSIONS OFF)

    if (MSVC)
      set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")
    else ()
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -Wextra -Wshadow -Wsign-compare -Wshadow -Wwrite-strings -Wpointer-arith -Winit-self -Wconversion -Wno-sign-conversion")
    endif ()

    set (ACTO_BUILD_SAMPLES ON)
  elseif (NOT CMAKE_CXX_STANDARD)
    set (CMAKE_CXX_STANDARD 14)
    set (CMAKE_CXX_STANDARD_REQUIRED ON)
  endif ()

  # Library sources.
  add_subdirectory (lib)

  # Build samples.
  if (ACTO_BUILD_SAMPLES)
    add_subdirectory (samples)
  endif ()
