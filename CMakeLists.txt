cmake_minimum_required(VERSION 3.6)
project(prime_watch_external)

set(CMAKE_CXX_STANDARD 11)

add_definitions(-DPRIME_DUMP_JSON=1)
add_definitions(-DPW_USE_LOOPBACK)
add_subdirectory(PrimeMemoryDumping)
include_directories(PrimeMemoryDumping/common)
include_directories(PrimeMemoryDumping/)

set(SOURCE_FILES main.cpp common.cpp
        PrimeMemoryDumping/prime1/Prime1JsonDumper.cpp
        PrimeMemoryDumping/common/MemoryBuffer.cpp)

if (WIN32 OR CYGWIN)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    message("Windows target")
    set(SOURCE_FILES ${SOURCE_FILES} windump.cpp)
endif ()

SET(BUILD_SHARED_LIBRARIES OFF)
SET(CMAKE_EXE_LINKER_FLAGS "-static")
add_executable(prime_watch_external ${SOURCE_FILES})

if (WIN32)
    target_link_libraries(prime_watch_external wsock32 ws2_32)
endif ()