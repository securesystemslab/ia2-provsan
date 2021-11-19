# Build for MPK Untrusted Dynamic Analysis
cmake_minimum_required(VERSION 3.8)
project(ProvenanceSanitizer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads)

option(MPK_STATS "Capture runtime statistics for insturmentation")
option(MPK_ENABLE_LOGGING "Enable Logging for Runtime")

if(MPK_STATS)
    add_definitions(-DMPK_STATS=1)
else()
    add_definitions(-DMPK_STATS=0)
endif()

if(MPK_ENABLE_LOGGING)
    add_definitions(-DMPK_ENABLE_LOGGING=1)
else()
    add_definitions(-DMPK_ENABLE_LOGGING=0)
endif()




set(PROVSAN_SOURCES
    alloc_site_handler.cpp
    provsan_utils.cpp
    provsan_fault_handler.cpp
    provsan_formatter.cpp
    provsan_init.cpp
    )

set(PROVSAN_HEADERS
    alloc_site_handler.h
    provsan_utils.h
    provsan_common.h
    provsan_fault_handler.h
    provsan_formatter.h
    provsan_init.h
    )


set(CMAKE_CXX_FLAGS ${PROVSAN_LINK_FLAGS})
#set(CMAKE_SHARED_LINKER_FLAGS ${PROVSAN_LINK_FLAGS})

add_library(provsan_rt
    SHARED
    ${PROVSAN_SOURCES}
    ${PROVSAN_HEADERS}
    )

#add_subdirectory(tests)
