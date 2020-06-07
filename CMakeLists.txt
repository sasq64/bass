cmake_minimum_required(VERSION 3.10)
project(bass VERSION 0.5)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_USE_MATH_DEFINES)
    set(WARN_EXT /W2)
    set(WARN /W3)
else()
    set(THREAD_LIB pthread)
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
    src/png.cpp src/script.cpp)

target_compile_definitions(badlib PUBLIC SOL_USING_CXX_LUA USE_FMT)
target_compile_options(badlib PRIVATE ${WARN})
target_include_directories(badlib PRIVATE external/peglib PUBLIC src)
target_link_libraries(badlib PRIVATE lua lodepng fmt coreutils ${THREAD_LIB} PUBLIC sol2::sol2 )

add_executable(bass main.cpp)
target_link_libraries(bass PRIVATE coreutils badlib CLI11::CLI11)

add_executable(tester testmain.cpp asmtest.cpp symtest.cpp)
target_link_libraries(tester PRIVATE coreutils fmt badlib)

