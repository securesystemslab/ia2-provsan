include(CheckCXXCompilerFlag)

include_directories(..)

# Add static library with object file
macro(add_provsan_lib library)
    add_library(${library} STATIC ${ARGN})
    set_target_properties(${library} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        FOLDER "Compiler-rt Tests")
endmacro()

function(get_provsan_lib_for_arch arch lib)
    set(tgt_name "RTPROVSANUntrusted.test.${arch}")
    set(${lib} "${tgt_name}" PARENT_SCOPE)
endfunction()

add_custom_target(ProvSanUnitTests)
set_target_properties(ProvSanUnitTests PROPERTIES
    FOLDER "Compiler-rt Tests")

set(PROVSAN_UNITTEST_CFLAGS
    ${PROVSAN_UNTRUSTED_CFLAGS}
    ${COMPILER_RT_UNITTEST_CFLAGS}
    ${COMPILER_RT_GTEST_CFLAGS}
    -I${COMPILER_RT_SOURCE_DIR}/include
    -I${COMPILER_RT_SOURCE_DIR}/lib
    -I${COMPILER_RT_SOURCE_DIR}/lib/mpk_untrusted)

set(PROVSAN_TEST_ARCH ${X86_64})
set(LINK_FLAGS
    -lstdc++
    -lpthread
    -lm
    -ldl
    -lrt)

set(PROVSAN_RTL_HEADERS)
foreach (header ${PROVSAN_HEADERS})
    list(APPEND PROVSAN_TRL_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/../${header})
endforeach()

add_mpk_untrusted_lib("RTProvSanUntrusted.test.${arch}"
    $<TARGET_OBJECTS:RTMPKUntrusted.${arch}>
    $<TARGET_OBJECTS:RTSanitizerCommon.${arch}>
    $<TARGET_OBJECTS:RTSanitizerCommonLibc.${arch}>)

macro(add_mpk_unittest testname)
    cmake_parse_arguments(TEST "" "" "SOURCES;HEADERS" ${ARGN})
    message(STATUS "Building Source : ${TEST_SOURCES}")
    foreach(arch ${MPK_TEST_ARCH})
        get_mpk_untrusted_lib_for_arch(${arch} MPKUNTRUSTED_COMMON_LIB)
        set(MPKUnitTestObjects)
        generate_compiler_rt_tests(MPKUnitTestObjects MPKUnitTests
            "${testname}-${arch}-Test" ${arch}
            SOURCES ${TEST_SOURCES} ${COMPILER_RT_GTEST_SOURCE}
            RUNTIME ${MPKUNTRUSTED_COMMON_LIB}
            COMPILE_DEPS ${TEST_HEADERS} ${MPK_RTL_HEADERS}
            DEPS gtest mpk_untrusted
            CFLAGS ${MPK_UNITTEST_CFLAGS}
            LINK_FLAGS ${LINK_FLAGS})
        message(STATUS "Making test : ${testname} as ${testname}-${arch}-Test")
    endforeach()
endmacro()

add_subdirectory(sig)
add_subdirectory(unit)
