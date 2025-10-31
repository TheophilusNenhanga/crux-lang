cmake_minimum_required(VERSION 3.28)

set(CMAKE_C_COMPILER gcc)

project(crux-lang C)

set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_compile_options(-g -O0 -Wswitch-enum -Wimplicit-fallthrough)

	if (UNIX)
		set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address")
		set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=undefined")
	endif ()

	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fno-omit-frame-pointer")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fstack-protector-strong")

	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wextra")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wformat=2 -Wformat-security")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wnull-dereference -Wstack-protector")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Warray-bounds -Wshift-overflow")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Release")
	add_compile_options(-O3)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Perf")
	add_compile_options(-O2 -g -fno-omit-frame-pointer -fopt-info-vec-missed)
endif()

file(GLOB SOURCES
				"src/*.c"
				"src/stdlib/*.c"
				"src/vm/*.c"
)

file(GLOB HEADERS
			"include/*.h"
			"include/stdlib/*.h"
)

include_directories(include)

add_executable(crux ${SOURCES} ${HEADERS})

if (UNIX)
	target_link_libraries(crux PRIVATE m)
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Debug" AND UNIX)
	target_link_options(crux PRIVATE -fsanitize=address -fsanitize=undefined)
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Profile")
	target_compile_options(crux PRIVATE -pg)
	target_link_options(crux PRIVATE -pg)
endif ()
