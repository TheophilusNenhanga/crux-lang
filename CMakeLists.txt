cmake_minimum_required(VERSION 3.28)
project(StellaC C)

set(CMAKE_C_STANDARD 11)


if (CMAKE_BUILD_TYPE)
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -O3")
    endif ()
    if (CMAKE_BUILD_TYPE STREQUAL "Profile")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
    endif ()
endif ()

add_executable(StellaC
        main.c
        common.h
        chunk.h
        chunk.c
        memory.h
        memory.c
        debug.h
        debug.c
        value.c
        value.h
        vm.c
        vm.h
        compiler.c
        compiler.h
        scanner.c
        scanner.h
        object.c
        table.c
        table.h
        object.h
        std/stl_time.h
        std/stl_time.c
        std/collections.h
        std/collections.c
        main.c
        panic.c
        panic.h
        std/error.c
        std/error.h
        std/string.c
        std/string.h
        std/std.c
        std/std.h
        std/array.c
        std/array.h
        std/tables.c
        std/tables.h
        file_handler.c
        file_handler.h
        std/stl_math.c
        std/stl_math.h
        std/stl_io.c
        std/stl_io.h
)
