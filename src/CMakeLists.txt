cmake_minimum_required(VERSION 3.28)

set(CMAKE_C_COMPILER gcc)

project(crux-lang C)

set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

# set(CMAKE_C_FLAGS "-Wall")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(CMAKE_C_FLAGS_DEBUG "-g -O0")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Release")
	set(CMAKE_C_FLAGS_RELEASE "-O3")
	set(CMAKE_C_FLAGS_RELEASE "-O3")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Profile")
	set(CMAKE_C_FLAGS_PROFILE "-pg")
endif()

file(GLOB SOURCES
				"*.c"
				"vm/*.c"
				"std/*.c"
)

file(GLOB HEADERS
			"*.h"
			"vm/*.h"
			"std/*.h"
)

add_executable(crux ${SOURCES} ${HEADERS})

if (UNIX)
	find_library(MATH_LIBRARY m)
	target_link_libraries(crux PRIVATE ${MATH_LIBRARY})
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Profile")
	target_compile_options(crux PRIVATE -pg)
	target_link_options(crux PRIVATE -pg)
endif ()
