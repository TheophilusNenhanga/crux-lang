cmake_minimum_required(VERSION 3.28)

set(CMAKE_C_COMPILER gcc)

project(crux-lang C)

set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(CMAKE_C_FLAGS_DEBUG "-g -O0")

	if (UNIX)
		set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address")
		set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=undefined")
	endif ()

	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fno-omit-frame-pointer")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fstack-protector-strong")

	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall -Wextra")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wformat=2 -Wformat-security")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wnull-dereference -Wstack-protector")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Warray-bounds -Wshift-overflow")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Release")
	set(CMAKE_C_FLAGS_RELEASE "-O3")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Perf")
	set(CMAKE_C_FLAGS_PERF "-O2 -g -fno-omit-frame-pointer")
endif()

set(SOURCES
				"src/chunk.c"
				"src/compiler.c"
				"src/debug.c"
				"src/file_handler.c"
				"src/main.c"
				"src/memory.c"
				"src/object.c"
				"src/panic.c"
				"src/scanner.c"
				"src/table.c"
				"src/value.c"

				"src/compiler/ast.c"
				"src/compiler/codegen.c"
				"src/compiler/ast_compiler.c"

				"src/vm/vm_run.c"
				"src/vm/vm_helpers.c"

				"src/stdlib/array.c"
				"src/stdlib/core.c"
				"src/stdlib/error.c"
				"src/stdlib/fs.c"
				"src/stdlib/io.c"
				"src/stdlib/math.c"
				"src/stdlib/random.c"
				"src/stdlib/std.c"
				"src/stdlib/string.c"
				"src/stdlib/sys.c"
				"src/stdlib/tables.c"
				"src/stdlib/time.c"
				"src/stdlib/vectors.c"
)

add_executable(crux ${SOURCES})

target_include_directories(crux PRIVATE "include")


if (UNIX)
	target_link_libraries(crux PRIVATE m)
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Debug" AND UNIX)
	target_link_options(crux PRIVATE -fsanitize=address -fsanitize=undefined)
endif ()