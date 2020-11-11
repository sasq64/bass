cmake_minimum_required(VERSION 3.10)
project(bass VERSION 0.5)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_USE_MATH_DEFINES -D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING)
    set(WARN_EXT /W2)
else()
    set(THREAD_LIB pthread)
    set(THREAD_OPTIONS -pthread)
endif()

# Warning level setup #######################################################

# GCC/Clang common warnings
set(GCC_CLANG_WARNINGS -Werror -Wall -Wextra -Wnon-virtual-dtor -pedantic
-Wno-unused-variable -Wno-unused-parameter -Wno-unused-function)

set(MSVC_WARNINGS /W3)

if(FUZZ)
    set(CMAKE_CXX_FLAGS  ${CMAKE_CXX_FLAGS} -fsanitize=fuzzer,address)
    set(CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address)
endif()

# GCC specific warnings
set(GCC_WARNINGS
        -Wduplicated-branches
        -Wduplicated-cond
        -Wlogical-op
        -Wcast-align
        -Wno-missing-field-initializers)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(WARNINGS ${WARNINGS} ${GCC_WARNINGS} ${GCC_CLANG_WARNINGS})
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(WARNINGS ${WARNINGS} ${CLANG_WARNINGS} ${GCC_CLANG_WARNINGS})
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    set(WARNINGS ${WARNINGS} ${MSVC_WARNINGS})
endif()


add_subdirectory(external/fmt)
add_subdirectory(external/CLI11)
add_subdirectory(external/coreutils)
add_subdirectory(external/lua)
add_subdirectory(external/sol3)
add_subdirectory(external/ansi)

add_library(lodepng STATIC external/lodepng/lodepng)
target_include_directories(lodepng PUBLIC external/lodepng)
target_compile_options(lodepng PRIVATE ${WARN_EXT})

add_library(catch INTERFACE)
target_include_directories(catch INTERFACE external)

add_library(sha512 STATIC src/sha512.c)

add_library(badlib STATIC
    src/assembler.cpp src/grammar.cpp src/functions.cpp
    src/machine.cpp src/wrap.cpp src/meta.cpp
    src/png.cpp src/script.cpp src/script_functions.cpp)

target_compile_definitions(badlib PUBLIC SOL_USING_CXX_LUA USE_FMT)
target_compile_options(badlib PUBLIC ${WARNINGS})
target_include_directories(badlib PRIVATE external/peglib PUBLIC src)
target_link_libraries(badlib PUBLIC coreutils
        PRIVATE lua lodepng fmt ${THREAD_LIB} PUBLIC sol2::sol2 sha512)

if(FUZZ)
    add_executable(bazz src/fuzz.cpp)
    target_link_libraries(bazz PRIVATE badlib)
else()
    add_executable(bass src/main.cpp src/text_emu.cpp)
    target_link_options(bass PRIVATE ${THREAD_OPTIONS})
    target_link_libraries(bass PRIVATE badlib CLI11::CLI11 ansi)

    add_executable(gtest src/gtest.cpp src/grammar.cpp)
    target_link_options(gtest PRIVATE ${THREAD_OPTIONS})
    target_include_directories(gtest PRIVATE external/peglib)
    target_link_libraries(gtest PRIVATE coreutils CLI11::CLI11 ${THREAD_LIB})


    add_executable(sectiontest EXCLUDE_FROM_ALL src/testmain.cpp src/sectiontest.cpp src/machine.cpp)
    target_include_directories(sectiontest PRIVATE src)
    target_link_libraries(sectiontest PRIVATE catch coreutils fmt)

    add_executable(symtest EXCLUDE_FROM_ALL src/testmain.cpp src/symtest.cpp)
    target_compile_definitions(symtest PUBLIC USE_FMT)
    target_include_directories(symtest PRIVATE src)
    target_link_libraries(symtest PRIVATE catch fmt coreutils)

    add_executable(tester src/testmain.cpp src/asmtest.cpp)
    target_link_libraries(tester PRIVATE catch fmt badlib)

endif(FUZZ)
