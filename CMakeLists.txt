cmake_minimum_required(VERSION 3.15)

option(ACTO_BUILD_SAMPLES "Set to ON to build samples" OFF)
option(ACTO_BUILD_TESTS "Set to ON to build tests" OFF)

project(acto LANGUAGES CXX)
find_package(Threads REQUIRED)

# Determine whether this is a standalone project or
# included by other projects.
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)

  if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -Wextra -Wshadow -Wsign-compare -Wshadow -Wwrite-strings -Wpointer-arith -Winit-self -Wconversion -Wno-sign-conversion")
  endif()

  set(ACTO_BUILD_SAMPLES ON)
  set(ACTO_BUILD_TESTS ON)
elseif(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

set(ACTO_PUBLIC_INCLUDE_DIR "include/acto")

add_library(acto-lib STATIC)

target_sources(acto-lib
  PUBLIC
    "include/acto/acto.h"
    "include/acto/event.h"
    "include/acto/intrusive.h"
  PRIVATE
    "src/acto.cpp"
    "src/event.cpp"
    "src/runtime.cpp"
    "src/runtime.h"
    "src/worker.cpp"
    "src/worker.h"
)

target_include_directories(acto-lib
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(UNIX)
  target_link_libraries(acto-lib PUBLIC
    pthread
  )
endif()

# Build samples.
if(ACTO_BUILD_SAMPLES)
  add_subdirectory(samples)
endif()

# Build tests.
if(ACTO_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()
