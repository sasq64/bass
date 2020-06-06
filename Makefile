
all :
	make -C build -j


test : all
	build/tester

main.prg : asm/test.asm build/badass
	build/badass -i asm/test.asm

run: main.prg
	../x16-emulator/build/x16-emu -rom rom.bin -prg result.prg -run
