cmake_minimum_required(VERSION 3.6)
project(prime_watch_external)

set(CMAKE_CXX_STANDARD 11)

add_definitions(-DPRIME_DUMP_JSON=1)
add_subdirectory(PrimeMemoryDumping)
include_directories(PrimeMemoryDumping/common)
include_directories(PrimeMemoryDumping/)

set(SOURCE_FILES main.cpp
        PrimeMemoryDumping/prime1/Prime1JsonDumper.cpp
        PrimeMemoryDumping/common/MemoryBuffer.cpp)

if (WIN32 OR CYGWIN)
    message("Windows target")
    set(SOURCE_FILES ${SOURCE_FILES} windump.cpp)
endif ()

add_executable(prime_watch_external ${SOURCE_FILES})
