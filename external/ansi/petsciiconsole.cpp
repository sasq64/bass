#include <coreutils/log.h>
#include "petsciiconsole.h"

#include <array>
#include <algorithm> 

namespace bbs {

using namespace std;

/** PETSCII->ASCII 0x20 -> 0xFF Lower case**/
static int petsciiTable[] = {
	0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
	0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,

	0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
	0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
//40
	0x0040, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
	0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,
//50
	0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
	0x0078, 0x0079, 0x007A, 0x005B, 0x00A3, 0x005D, 0x2191, 0x2190,
//60
	0x2500, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
	0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
//70
	0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
	0x0058, 0x0059, 0x005A, 0x253c, 0x1fb8c, 0x2502, 0x1fb96, 0x1fb98,
//80
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x000A, 0x0000, 0x0000,
//90
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
//A0
	0x00A0, 0x258C, 0x2584, 0x2594, 0x2581, 0x258F, 0x2592, 0x2595,
	0x0000, 0x25A8, 0x0000, 0x251C, 0x0000, 0x2514, 0x2510, 0x2582,
//B0
	0x250C, 0x2534, 0x252C, 0x2524, 0x258E, 0x258D, 0x1fb88, 0x1fb82,
	0x1fb83, 0x2583, 0x2713, 0x2596, 0x259D, 0x2518, 0x2598, 0x259a,
//C0
	0x2501, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
	0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
//D0
	0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
	0x0058, 0x0059, 0x005A, 0x253C, 0x0000, 0x2502, 0x1fb96, 0x1fb98,
//E0
	0x00A0, 0x258C, 0x2584, 0x2594, 0x2581, 0x258F, 0x2592, 0x2595,
	0x1fb8f, 0x1fb99, 0x1fb87, 0x251C, 0x2f97, 0x2514, 0x2510, 0x2582,
//F0
	0x250C, 0x2534, 0x252C, 0x2524, 0x258E, 0x258D, 0x1fb88, 0x1fb82,
	0x1fb83, 0x2583, 0x2713, 0x2596, 0x259D, 0x2518, 0x2598, 0x1fb96
};

static vector<uint8_t> petsciiColors = { 5,
 28, 30, 31, 129, 144, 149, 150, 151, 152, 153, 154, 155, 156, 158, 159 };

PetsciiConsole::PetsciiConsole(Terminal &terminal) : Console(terminal) {
	resize(40, 25);
	impl_clear();
	impl_color(fgColor, bgColor);
	impl_gotoxy(0,0);
	int i = 0x20;
	for(auto c : petsciiTable) {
		unicodeToPetscii[c] = i;
		i++;
	}

	wstring s = L"\t╋┃━┏┓┗┛{}_";
	vector<uint8_t> v { 0x20,0xdd,0xdb,0x60,0xb0,0xae,0xad,0xbd,0xb3,0xab,0xa4 };
	for(unsigned i=0; i<v.size(); i++) {
		unicodeToPetscii[s[i]] = v[i];
	}
}


void PetsciiConsole::putChar(Char c) {


	if(curX == 39) {
		if(c == '\"') c = 0xa9; // NOTE: No fix, just avoids a bigger mess.
		outBuffer.push_back(DEL);
		outBuffer.push_back(c & 0xff);
		outBuffer.push_back(LEFT);
		Tile &t = grid[curY*width+38];
		c = t.c;
		if(c == '\"') c = 0xa9; // NOTE: No fix, just avoids a bigger mess.
		if(t.fg != grid[curY*width+39].fg || t.bg != grid[curY*width+39].bg) {
			impl_color(t.fg, t.bg);
			outBuffer.push_back(INS);
			outBuffer.push_back(c & 0xff);
			impl_color(fgColor, bgColor);
		} else {
			outBuffer.push_back(INS);
			outBuffer.push_back(c & 0xff);
		}
	} else {	
		if(c == '\"' && curX == 38) c = 0xa9; // NOTE: No fix, just avoids a bigger mess.

		outBuffer.push_back(c & 0xff);		
		if(c == '\"') {
			// Handle quotes. ""
			// NOTE: Quotes in last column are NOT handled currently :(
			uint8_t nextc = grid[curY*width+curX+1].c & 0xff;
			outBuffer.push_back(c & 0xff);
			outBuffer.push_back(LEFT);
			if(nextc != '\"') {
				outBuffer.push_back(nextc);
				outBuffer.push_back(LEFT);
			}
		}

		curX++;
		if(curX >= width) {
			curX -= width;
			curY++;
	}	}
}

void PetsciiConsole::impl_translate(Char &c) {
	c = unicodeToPetscii[c];
	if(c == 0) c = '?';
}

void PetsciiConsole::impl_color(int fg, int bg) {
	if(bg >= 0 && bg != BLACK) {
		outBuffer.push_back(RVS_ON);
		outBuffer.push_back(petsciiColors[bg]);
	} else if(fg >= 0) {
		outBuffer.push_back(RVS_OFF);
		outBuffer.push_back(petsciiColors[fg]);
	}
}

void PetsciiConsole::impl_clear() {
	outBuffer.push_back(147);
}

void PetsciiConsole::impl_gotoxy(int x, int y) {


	int dx = curX-x;
	if(dx < 0) dx = -dx;
	int dy = curY-y;
	if(dy < 0) dy = -dy;

	if(y < dy) {
		outBuffer.push_back(HOME);
		curX = curY = 0;
		dx = x;
		dy = y;
	}

	// dx dy is the distance to move the cursor

	// If its closer to move from the edge, do a line feed
	if(40 - x < dx || curX - x > x) {
		if(curY == height-1) {
			outBuffer.push_back(UP);
		} else
			curY++;	
		outBuffer.push_back(SHIFT_RETURN);
		if(bgColor != BLACK) {
			outBuffer.push_back(RVS_ON);
		}
		curX = 0;
		if(40 - x < dx && curY > 0) {
			outBuffer.push_back(LEFT);
			curY--;
			curX = 39;
		}
	}

	while(y > curY) {
		outBuffer.push_back(DOWN);
		curY++;
	}
	while(y < curY) {
		outBuffer.push_back(UP);
		curY--;
	}

	while(x > curX) {
		outBuffer.push_back(RIGHT);
		curX++;
	}

	while(x < curX) {
		outBuffer.push_back(LEFT);
		curX--;
	}
}

int PetsciiConsole::impl_handlekey() {
	auto k = inBuffer.front();
	inBuffer.pop();
	switch(k) {
	case 13 :
		return KEY_ENTER;
	case DEL :
		return KEY_BACKSPACE;
	case DOWN :
		return KEY_DOWN;
	case UP :
		return KEY_UP;
	case LEFT :
		return KEY_LEFT;
	case RIGHT :
		return KEY_RIGHT;
	case HOME:
		return KEY_HOME;
	case CLEAR:
		return KEY_END;
	case STOP:
		return KEY_ESCAPE;
	default:
		if(k >= F1 && k <= F8) {
			k -= F1;
			return KEY_F1 + (k * 2 % 8) + (k / 4); // A little bit of trickery to convert :)
		}
	}
	if(k >= 0x20 && (k <= 0x7f || k >= 0xa0)) {		
		int k2 = petsciiTable[k-0x20];
		//LOGD("%02x became %04x (%c)", k, k2, k2);
		return k2;
	}
	return KEY_UNKNOWN;
}

bool PetsciiConsole::impl_scroll_screen(int dy) {
	//const auto s = dy > 0 ? utils::format("\x1b[%dS",dy) : utils::format("\x1b[%dT", -dy);
	//outBuffer.insert(outBuffer.end(), s.begin(), s.end());
	auto steps = dy + height - curY -1;
	LOGD("DY %d HEIGHT %d curY %d -> STEPS %d", dy, height, curY, steps);
	while(steps--)
		outBuffer.push_back(DOWN);
	steps = height - curY - 1;
	LOGD("DY %d HEIGHT %d curY %d -> STEPS %d", dy, height, curY, steps);
	if(curY < steps) {
		outBuffer.push_back(HOME);
		outBuffer.push_back(DOWN);
		return true;
	}
	while(steps--)
		outBuffer.push_back(UP);
	return true;
}

}
