CMAKE_FLAGS = -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang
all : debug

builds/debug/cmake_install.cmake :
	rm -rf builds/debug
	cmake -Bbuilds/debug -H. ${CMAKE_FLAGS} -DCMAKE_BUILD_TYPE=Debug

compile_commands.json : builds/debug/compile_commands.json
	rm -f compile_commands.json
	ln -s builds/debug/compile_commands.json .

debug : builds/debug/cmake_install.cmake compile_commands.json
	cmake --build builds/debug -- -j8

test : all
	builds/debug/tester

