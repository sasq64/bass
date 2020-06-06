cmake_minimum_required(VERSION 3.5)
project(apone.coreutils)


if(NOT TARGET fmt::fmt)
    add_subdirectory(../fmt fmt)
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(SOURCE_FILES src/coreutils/log.cpp)
add_library(coreutils ${SOURCE_FILES})
target_link_libraries(coreutils PUBLIC fmt::fmt)
target_compile_definitions(coreutils PUBLIC )
target_include_directories(coreutils INTERFACE src)

set_property(TARGET coreutils PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET coreutils PROPERTY CXX_STANDARD 14)
set_property(TARGET coreutils PROPERTY CXX_STANDARD_REQUIRED ON)

add_library(apone::coreutils ALIAS coreutils)
