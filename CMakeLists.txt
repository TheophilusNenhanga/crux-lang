cmake_minimum_required(VERSION 3.28)

set(CMAKE_C_COMPILER gcc)

project(crux-lang C)

set(CMAKE_C_STANDARD 11)

option(CRUX_STACK_SAFETY "Enable stack push/pop safety checks" OFF)
option(CRUX_DEBUG_TRACE_EXECUTION "Enable execution tracing" OFF)
option(CRUX_DEBUG_PRINT_CODE "Print compiled bytecode" OFF)
option(CRUX_DEBUG_LOG_GC "Enable GC logging" OFF)
option(CRUX_DEBUG_STRESS_GC "Enable GC stress mode" OFF)
set(CRUX_VERSION "dev" CACHE STRING "Crux language version")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "ASAN" AND NOT UNIX)
  message(FATAL_ERROR "ASAN builds are only allowed on UNIX-like systems")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "ASAN")
  add_compile_options(-g3 -O0 -Wall -Wextra)

  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fno-omit-frame-pointer")
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fstack-protector-strong")

  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wformat=2 -Wformat-security")
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wnull-dereference -Wstack-protector")
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Warray-bounds -Wshift-overflow")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "ASAN")
  if (UNIX)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=undefined")
  endif ()
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Release")
  add_compile_options(-O3 -fno-delete-null-pointer-checks -flto -flto=auto)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Perf")
  add_compile_options(-O2 -g -fno-omit-frame-pointer -fno-delete-null-pointer-checks -flto -flto=auto)
endif()

file(GLOB SOURCES
    "src/*.c"
    "src/stdlib/*.c"
    "src/vm/*.c"
    "src/memory/*.c"
    "src/compiler/*.c"
)

if (NOT WIN32)
  list(APPEND SOURCES "deps/linenoise/linenoise.c")
endif()

file(GLOB HEADERS
   "include/*.h"
   "include/stdlib/*.h"
   "deps/utf8/utf8.h"
)

if (NOT WIN32)
  list(APPEND HEADERS "deps/linenoise/linenoise.h")
endif()

include_directories(include)

if (NOT WIN32)
  include_directories(deps/linenoise)
endif()
include_directories(deps/utf8)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(crux ${SOURCES} ${HEADERS})

target_compile_definitions(crux PRIVATE
        $<$<BOOL:${CRUX_STACK_SAFETY}>:STACK_SAFETY>
        $<$<BOOL:${CRUX_DEBUG_TRACE_EXECUTION}>:DEBUG_TRACE_EXECUTION>
        $<$<BOOL:${CRUX_DEBUG_PRINT_CODE}>:DEBUG_PRINT_CODE>
        $<$<BOOL:${CRUX_DEBUG_LOG_GC}>:DEBUG_LOG_GC>
        $<$<BOOL:${CRUX_DEBUG_STRESS_GC}>:DEBUG_STRESS_GC>
        CRUX_VERSION="${CRUX_VERSION}"
)

if (UNIX)
  target_link_libraries(crux PRIVATE m)
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "ASAN")
  target_link_options(crux PRIVATE -fsanitize=address -fsanitize=undefined)
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Profile")
  target_compile_options(crux PRIVATE -pg)
  target_link_options(crux PRIVATE -pg)
endif ()

# Print enabled compile flags
message(STATUS "Crux-lang build configuration:")
message(STATUS "  CMake Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Version: ${CRUX_VERSION}")
message(STATUS "  Enabled options:")
if(CRUX_STACK_SAFETY)
  message(STATUS "    - CRUX_STACK_SAFETY: ON")
endif()
if(CRUX_DEBUG_TRACE_EXECUTION)
  message(STATUS "    - CRUX_DEBUG_TRACE_EXECUTION: ON")
endif()
if(CRUX_DEBUG_PRINT_CODE)
  message(STATUS "    - CRUX_DEBUG_PRINT_CODE: ON")
endif()
if(CRUX_DEBUG_LOG_GC)
  message(STATUS "    - CRUX_DEBUG_LOG_GC: ON")
endif()
if(CRUX_DEBUG_STRESS_GC)
  message(STATUS "    - CRUX_DEBUG_STRESS_GC: ON")
endif()
if(NOT CRUX_STACK_SAFETY AND NOT CRUX_DEBUG_TRACE_EXECUTION AND NOT CRUX_DEBUG_PRINT_CODE AND NOT CRUX_DEBUG_LOG_GC AND NOT CRUX_DEBUG_STRESS_GC)
  message(STATUS "    (none)")
endif()
message(STATUS "")
