cmake_minimum_required(VERSION 3.16)
project(frt_host_tests C CXX)

# Runs the frt wrapper on the FreeRTOS POSIX port.
#
#   cmake -S test/host -B build && cmake --build build && ctest --test-dir build
#
# Set FREERTOS_KERNEL_PATH to use a local FreeRTOS-Kernel checkout, otherwise
# it is downloaded.

option(FRT_SANITIZE "Build with AddressSanitizer and UBSan" ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(FREERTOS_KERNEL_PATH "" CACHE PATH "Path to a FreeRTOS-Kernel checkout")

if(NOT FREERTOS_KERNEL_PATH)
    include(FetchContent)
    FetchContent_Declare(freertos_kernel
        GIT_REPOSITORY https://github.com/FreeRTOS/FreeRTOS-Kernel.git
        GIT_TAG V11.1.0
        GIT_SHALLOW TRUE)
    FetchContent_GetProperties(freertos_kernel)
    if(NOT freertos_kernel_POPULATED)
        FetchContent_Populate(freertos_kernel)
    endif()
    set(FREERTOS_KERNEL_PATH ${freertos_kernel_SOURCE_DIR})
endif()

set(FRT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(PORT_DIR ${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/Posix)

set(KERNEL_SOURCES
    ${FREERTOS_KERNEL_PATH}/tasks.c
    ${FREERTOS_KERNEL_PATH}/queue.c
    ${FREERTOS_KERNEL_PATH}/list.c
    ${FREERTOS_KERNEL_PATH}/timers.c
    ${FREERTOS_KERNEL_PATH}/event_groups.c
    ${FREERTOS_KERNEL_PATH}/stream_buffer.c
    ${FREERTOS_KERNEL_PATH}/portable/MemMang/heap_3.c
    ${PORT_DIR}/port.c
    # Patched copy of ${PORT_DIR}/utils/wait_for_event.c, see the file
    ${CMAKE_CURRENT_SOURCE_DIR}/posix_port_fix/wait_for_event.c)

find_package(Threads REQUIRED)
enable_testing()

function(frt_add_test name static_alloc)
    add_executable(${name}
        test_main.cpp
        support.cpp
        ${FRT_ROOT}/src/frt/manager.cpp
        ${FRT_ROOT}/src/frt/log.cpp
        ${KERNEL_SOURCES})

    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/shim
        ${FRT_ROOT}/src
        ${FREERTOS_KERNEL_PATH}/include
        ${PORT_DIR}
        ${PORT_DIR}/utils)

    target_compile_definitions(${name} PRIVATE
        FRT_HOST
        FRT_TEST_STATIC_ALLOCATION=${static_alloc}
        projCOVERAGE_TEST=0)

    target_compile_options(${name} PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -Werror -Wno-missing-field-initializers>)

    if(FRT_SANITIZE)
        target_compile_options(${name} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${name} PRIVATE -fsanitize=address,undefined)
    endif()

    target_link_libraries(${name} PRIVATE Threads::Threads)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES TIMEOUT 120)
endfunction()

frt_add_test(frt_tests_static 1)
frt_add_test(frt_tests_dynamic 0)
