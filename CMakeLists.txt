cmake_minimum_required(VERSION 3.10)
project(bass VERSION 0.5)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_USE_MATH_DEFINES)
    set(WARN_EXT /W2)
else()
    set(THREAD_LIB pthread)
endif()

# Warning level setup #######################################################

# GCC/Clang common warnings
set(GCC_CLANG_WARNINGS -Wno-unused-parameter -Wno-unused-variable -Werror -Wall -Wextra -Wnon-virtual-dtor -pedantic)

set(MSVC_WARNINGS /W3)

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

add_library(lodepng STATIC external/lodepng/lodepng)
target_include_directories(lodepng PUBLIC external/lodepng)
target_compile_options(lodepng PRIVATE ${WARN_EXT})

add_library(badlib STATIC
    src/assembler.cpp src/grammar.cpp src/functions.cpp
    src/machine.cpp src/wrap.cpp src/meta.cpp
    src/png.cpp src/script.cpp src/script_functions.cpp)

target_compile_definitions(badlib PUBLIC SOL_USING_CXX_LUA USE_FMT)
target_compile_options(badlib PUBLIC ${WARNINGS})
target_include_directories(badlib PRIVATE external/peglib PUBLIC src)
target_link_libraries(badlib PRIVATE lua lodepng fmt coreutils ${THREAD_LIB} PUBLIC sol2::sol2 )

add_executable(bass main.cpp)
target_link_libraries(bass PRIVATE coreutils badlib CLI11::CLI11)

add_executable(symtest testmain.cpp symtest.cpp)
target_include_directories(symtest PRIVATE src)
target_link_libraries(symtest PRIVATE coreutils fmt)

add_executable(tester testmain.cpp asmtest.cpp)
target_link_libraries(tester PRIVATE coreutils fmt badlib)

