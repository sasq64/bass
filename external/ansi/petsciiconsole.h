#ifndef BBS_PETSCIICONSOLE_H
#define BBS_PETSCIICONSOLE_H

#include "console.h"

#include <unordered_map>

namespace bbs {

class PetsciiConsole : public Console {
public:

	enum {
		STOP = 3,
		WHITE = 5,
		DOWN = 0x11,
		RVS_ON = 0x12,
		HOME = 0x13,
		DEL = 0x14,
		RIGHT = 0x1d,
		RUN = 131,
		F1 = 133,
		F3 = 134,
		F5 = 135,
		F7 = 136,
		F2 = 137,
		F4 = 138,
		F6 = 139,
		F8 = 140,
		SHIFT_RETURN = 0x8d,
		UP = 0x91,
		RVS_OFF = 0x92,
		CLEAR = 0x93,
		INS = 0x94,
		LEFT = 0x9d
	};


	PetsciiConsole(Terminal &terminal);
	virtual void putChar(Char c) override;

	virtual const std::string name() const override { return "petscii"; }
protected:

	virtual void impl_color(int fg, int bg) override;
	virtual void impl_gotoxy(int x, int y) override;
	virtual int impl_handlekey() override;
	virtual void impl_clear() override;
	virtual void impl_translate(Char &c) override;
	virtual bool impl_scroll_screen(int dy) override;

	std::unordered_map<int, int> unicodeToPetscii; 

	int lastBg;
	int lastFg;
};

}

#endif // BBS_PETSCIICONSOLE_H
