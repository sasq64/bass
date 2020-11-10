cmake_minimum_required(VERSION 3.5)
project(cpp-mods.ansi VERSION 1.0)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

set(SOURCE_FILES
   #ansiconsole.cpp
	# console.cpp
    #editor.cpp
    #petsciiconsole.cpp
    terminal.cpp
)

add_library(ansi STATIC ${SOURCE_FILES})
target_include_directories(ansi INTERFACE ..)
target_link_libraries(ansi PUBLIC coreutils)

#add_executable(bbstest test.cpp)
#target_link_libraries(bbstest bbsutils coreutils)
