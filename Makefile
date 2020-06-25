CMAKE_FLAGS = -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang
all : debug

build/cmake_install.cmake :
	rm -rf builds/debug
	cmake -Bbuild -H. ${CMAKE_FLAGS} -DCMAKE_BUILD_TYPE=Debug

compile_commands.json : build/compile_commands.json
	rm -f compile_commands.json
	ln -s build/compile_commands.json .

debug : build/cmake_install.cmake compile_commands.json
	cmake --build build -- -j8

test : all
	build/tester

