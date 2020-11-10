#include <coreutils/log.h>
#include "ansiconsole.h"
//#include <coreutils/utils.h>

#include <array>
#include <algorithm> 

namespace bbs {

using namespace std;


static uint8_t c64pal2 [] = {
	0xFF, 0xFF, 0xFF, // WHITE
	0x68, 0x37, 0x2B, // RED
	0x58, 0x8D, 0x43, // GREEN
	0x35, 0x28, 0x79, // BLUE
	0x6F, 0x4F, 0x25, // ORANGE
	0x00, 0x00, 0x00, // BLACK
	0x43, 0x39, 0x00, // BROWN
	0x9A, 0x67, 0x59, // LIGHT_READ
	0x44, 0x44, 0x44, // DARK_GREY
	0x6C, 0x6C, 0x6C, // GREY
	0x9A, 0xD2, 0x84, // LIGHT_GREEN
	0x6C, 0x5E, 0xB5, // LIGHT_BLUE
	0x95, 0x95, 0x95, // LIGHT_GREY
	0x6F, 0x3D, 0x86, // PURPLE
	0xB8, 0xC7, 0x6F, // YELLOW
	0x70, 0xA4, 0xB2, // CYAN
};


static uint8_t xtermpal [] = {
0,0,0,
205,0,0,
0,205,0,
205,205,0,
0,0,238,
205,0,205,
0,205,205,
229,229,229,
127,127,127,
255,0,0,
0,255,0,
255,255,0,
92,92,255,
255,0,255,
0,255,255,
255,255,255,
  0,   0,   0,	
  0,   0,  95,	
  0,   0, 135,	
  0,   0, 175,	
  0,   0, 215,	
  0,   0, 255,	
  0,  95,   0,	
  0,  95,  95,	
  0,  95, 135,	
  0,  95, 175,	
  0,  95, 215,	
  0,  95, 255,	
  0, 135,   0,	
  0, 135,  95,	
  0, 135, 135,	
  0, 135, 175,	
  0, 135, 215,	
  0, 135, 255,	
  0, 175,   0,	
  0, 175,  95,	
  0, 175, 135,	
  0, 175, 175,	
  0, 175, 215,	
  0, 175, 255,	
  0, 215,   0,	
  0, 215,  95,	
  0, 215, 135,	
  0, 215, 175,	
  0, 215, 215,	
  0, 215, 255,	
  0, 255,   0,	
  0, 255,  95,	
  0, 255, 135,	
  0, 255, 175,	
  0, 255, 215,	
  0, 255, 255,	
 95,   0,   0,	
 95,   0,  95,	
 95,   0, 135,	
 95,   0, 175,	
 95,   0, 215,	
 95,   0, 255,	
 95,  95,   0,	
 95,  95,  95,	
 95,  95, 135,	
 95,  95, 175,	
 95,  95, 215,	
 95,  95, 255,	
 95, 135,   0,	
 95, 135,  95,	
 95, 135, 135,	
 95, 135, 175,	
 95, 135, 215,	
 95, 135, 255,	
 95, 175,   0,	
 95, 175,  95,	
 95, 175, 135,	
 95, 175, 175,	
 95, 175, 215,	
 95, 175, 255,	
 95, 215,   0,	
 95, 215,  95,	
 95, 215, 135,	
 95, 215, 175,	
 95, 215, 215,	
 95, 215, 255,	
 95, 255,   0,	
 95, 255,  95,	
 95, 255, 135,	
 95, 255, 175,	
 95, 255, 215,	
 95, 255, 255,	
135,   0,   0,	
135,   0,  95,	
135,   0, 135,	
135,   0, 175,	
135,   0, 215,	
135,   0, 255,	
135,  95,   0,	
135,  95,  95,	
135,  95, 135,	
135,  95, 175,	
135,  95, 215,	
135,  95, 255,	
135, 135,   0,	
135, 135,  95,	
135, 135, 135,	
135, 135, 175,	
135, 135, 215,	
135, 135, 255,	
135, 175,   0,	
135, 175,  95,	
135, 175, 135,	
135, 175, 175,	
135, 175, 215,	
135, 175, 255,	
135, 215,   0,	
135, 215,  95,	
135, 215, 135,	
135, 215, 175,	
135, 215, 215,	
135, 215, 255,	
135, 255,   0,	
135, 255,  95,	
135, 255, 135,	
135, 255, 175,	
135, 255, 215,	
135, 255, 255,	
175,   0,   0,	
175,   0,  95,	
175,   0, 135,	
175,   0, 175,	
175,   0, 215,	
175,   0, 255,	
175,  95,   0,	
175,  95,  95,	
175,  95, 135,	
175,  95, 175,	
175,  95, 215,	
175,  95, 255,	
175, 135,   0,	
175, 135,  95,	
175, 135, 135,	
175, 135, 175,	
175, 135, 215,	
175, 135, 255,	
175, 175,   0,	
175, 175,  95,	
175, 175, 135,	
175, 175, 175,	
175, 175, 215,	
175, 175, 255,	
175, 215,   0,	
175, 215,  95,	
175, 215, 135,	
175, 215, 175,	
175, 215, 215,	
175, 215, 255,	
175, 255,   0,	
175, 255,  95,	
175, 255, 135,	
175, 255, 175,	
175, 255, 215,	
175, 255, 255,	
215,   0,   0,	
215,   0,  95,	
215,   0, 135,	
215,   0, 175,	
215,   0, 215,	
215,   0, 255,	
215,  95,   0,	
215,  95,  95,	
215,  95, 135,	
215,  95, 175,	
215,  95, 215,	
215,  95, 255,	
215, 135,   0,	
215, 135,  95,	
215, 135, 135,	
215, 135, 175,	
215, 135, 215,	
215, 135, 255,	
215, 175,   0,	
215, 175,  95,	
215, 175, 135,	
215, 175, 175,	
215, 175, 215,	
215, 175, 255,	
215, 215,   0,	
215, 215,  95,	
215, 215, 135,	
215, 215, 175,	
215, 215, 215,	
215, 215, 255,	
215, 255,   0,	
215, 255,  95,	
215, 255, 135,	
215, 255, 175,	
215, 255, 215,	
215, 255, 255,	
255,   0,   0,	
255,   0,  95,	
255,   0, 135,	
255,   0, 175,	
255,   0, 215,	
255,   0, 255,	
255,  95,   0,	
255,  95,  95,	
255,  95, 135,	
255,  95, 175,	
255,  95, 215,	
255,  95, 255,	
255, 135,   0,	
255, 135,  95,	
255, 135, 135,	
255, 135, 175,	
255, 135, 215,	
255, 135, 255,	
255, 175,   0,	
255, 175,  95,	
255, 175, 135,	
255, 175, 175,	
255, 175, 215,	
255, 175, 255,	
255, 215,   0,	
255, 215,  95,	
255, 215, 135,	
255, 215, 175,	
255, 215, 215,	
255, 215, 255,	
255, 255,   0,	
255, 255,  95,	
255, 255, 135,	
255, 255, 175,	
255, 255, 215,	
255, 255, 255,	
  8,   8,   8,	
 18,  18,  18,	
 28,  28,  28,	
 38,  38,  38,	
 48,  48,  48,	
 58,  58,  58,	
 68,  68,  68,	
 78,  78,  78,	
 88,  88,  88,	
 98,  98,  98,	
108, 108, 108,	
118, 118, 118,	
128, 128, 128,	
138, 138, 138,	
148, 148, 148,	
158, 158, 158,	
168, 168, 168,	
178, 178, 178,	
188, 188, 188,	
198, 198, 198,	
208, 208, 208,	
218, 218, 218,	
228, 228, 228,	
238, 238, 238
};

static int xterm2c64[16];

AnsiConsole::AnsiConsole(Terminal &terminal) : Console(terminal) {
	//resize(width, height);
	for(int i=0; i<16; i++) {
		int best = 256*256*256;
		int bestj = 0;
		for(int j=1; j<256; j++) {
			auto rd = c64pal2[i*3] - xtermpal[j*3];
			auto gd = c64pal2[i*3+1] - xtermpal[j*3+1];
			auto bd = c64pal2[i*3+2] - xtermpal[j*3+2];
			auto v = rd*rd + gd*gd + bd*bd;
			if(v < best) {
				best = v;
				bestj = j;
			}

		}

		xterm2c64[i] = bestj;
		//LOGD("Best match %d,%d,%d -> %d %d %d", c64pal2[i*3], c64pal2[i*3+1], c64pal2[i*3+2], xtermpal[bestj*3], xtermpal[bestj*3+1], xtermpal[bestj*3+2]);

		//auto *p = &c64pal[i*3];
		//const string &s = utils::format("\x1b]4;%d;#%02x%02x%02x\x07", 160 + i, p[0], p[1], p[2]);
		//LOGD(s);
		//outBuffer.insert(outBuffer.end(), s.begin(), s.end());
	}
	impl_clear();
	impl_color(fgColor, bgColor);
	impl_gotoxy(0,0);

	//impl_gotoxy(0,0);
	//flush();
}

AnsiConsole::~AnsiConsole() {
	impl_showcursor(true);
	terminal.write(outBuffer, outBuffer.size());
}

void AnsiConsole::putChar(Char c) {

	if(c < 0x80)
		outBuffer.push_back(c);
	else if(c < 0x800) {
		outBuffer.push_back(0xC0 | (c >> 6));
		outBuffer.push_back(0x80 | (c & 0x3F));
		//ato l = outBuffer.size();
		//LOGD("Translated %02x to %02x %02x", c, (int)outBuffer[l-2], (int)outBuffer[l-1]);
	} else /*if (c < 0x10000) */ {
		outBuffer.push_back(0xE0 | (c >> 12));
		outBuffer.push_back(0x80 | (c >> 6));
		outBuffer.push_back(0x80 | (c & 0x3F));
	}
	curX++;
	if(curX >= width) {
		curX -= width;
		curY++;
	}
}

bool AnsiConsole::impl_scroll_screen(int dy) {
	const auto s = dy > 0 ? utils::format("\x1b[%dS",dy) : utils::format("\x1b[%dT", -dy);
	outBuffer.insert(outBuffer.end(), s.begin(), s.end());
	return true;
}

void AnsiConsole::impl_color(int fg, int bg) {

	//int af = ansiColors[fg];
	//int ab = ansiColors[bg];

	//LOGD("## BG %d\n", ab);
	//const string &s = utils::format("\x1b[%d;%d%sm", af + 30, ab + 40, hl ? ";1" : "");
	//uint8_t *p = &xtermpal[xterm2c64[fg] * 3];
	//LOGD("COL %d %d -> %d %d", fg, bg, xterm2c64[fg], xterm2c64[bg]);
	const auto s = utils::format("\x1b[38;5;%d;48;5;%dm", xterm2c64[fg], xterm2c64[bg]);
	/*auto r0 = c64pal2[fg*3];
	auto g0 = c64pal2[fg*3+1];
	auto b0 = c64pal2[fg*3+2];
	auto r1 = c64pal2[bg*3];
	auto g1 = c64pal2[bg*3+1];
	auto b1 = c64pal2[bg*3+2];
	const auto s = utils::format("\x1b[38;2;%d;%d;%dm\x1b[48;2;%d;%d;%dm", r0, g0, b0, r1, g1, b1);
*/
	//uint8_t *fp = &c64pal[fg*3];
	//uint8_t *bp = &c64pal[bg*3];
	//const string &s = utils::format("\x1b[38;2;%d;%d;%dm\x1b[48;2;%d;%d;%dm", fp[0], fp[1], fp[2], bp[0], bp[1], bp[2]);
	outBuffer.insert(outBuffer.end(), s.begin(), s.end());			
};

void AnsiConsole::impl_clear() {
	for(auto x : vector<uint8_t> { '\x1b', '[', '2', 'J' })
		outBuffer.push_back(x);
}


void AnsiConsole::impl_gotoxy(int x, int y) {
	// Not so smart for now
	//LOGD("gotoxy %d,%d", x,y);
	// 6-8 bytes
	const auto s = utils::format("\x1b[%d;%dH", y+1, x+1);
	outBuffer.insert(outBuffer.end(), s.begin(), s.end());
	curX = x;
	curY = y;
}

void AnsiConsole::impl_showcursor(bool show) {
	const auto s = utils::format("\x1b[?25%c", show ? 'h' : 'l');
	outBuffer.insert(outBuffer.end(), s.begin(), s.end());
}

class Matcher {
public:
	Matcher(std::queue<uint8_t> &buffer) : buffer(buffer) {}

	bool match(const vector<int> &pattern) {
		for(auto i : pattern) {
			if(i == buffer.front()) {
				buffer.pop();
			} else
				return false;
		}
		return true;
	}

	void addPattern(const vector<int> &pattern) {
	}

	//int findPattern() {
	//}



private:
	std::queue<uint8_t> buffer;

};

//match(0x1b, 0x5b, 0x50)


int AnsiConsole::impl_handlekey() {

	Matcher matcher(inBuffer);

	//addPattern({0x1b, 0x5b, 0x50}, KEY_F1);
	//if(matcher.match({0x1b, 0x50})) {
	//}

	auto c = inBuffer.front();
	if(c >= 0x80)
		return get_utf8();
	inBuffer.pop();
	if(c != 0x1b) {	
		LOGD("Normal key %d", (int)c);
		if(c == 13 || c == 10) {
			if(c == 13) {
				auto c2 = inBuffer.front();
				if(c2 == 10) {
					inBuffer.pop();
				}
			}
			return KEY_ENTER;
		} else if(c == 0x7f)
			return KEY_BACKSPACE;
		else if(c == 0x7e)
			return KEY_DELETE;
		return c;
	} else {

		if(inBuffer.size() > 0) {
			auto c2 = inBuffer.front();
			inBuffer.pop();
			auto c3 = inBuffer.front();
			inBuffer.pop();

			LOGD("ESCAPE key %02x %02x %02x", c, c2, c3);

			if(c2 == 0x5b || c2 == 0x4f) {
				switch(c3) {
				case 0x50:
					return KEY_F1;
				case 0x51:
					return KEY_F2;
				case 0x52:
					return KEY_F3;
				case 0x53:
					return KEY_F4;
				case 0x31:
					if(inBuffer.size() >= 2) {
						auto c4 = inBuffer.front();
						inBuffer.pop();
						auto c5 = inBuffer.front();
						inBuffer.pop();
						if(c5 == 126) {
							switch(c4) {
							case '5':
								return KEY_F5;
							case '7':
								return KEY_F6;
							case '8':
								return KEY_F7;
							case '9':
								return KEY_F8;
							}
						}
					}
					break;
				case 0x33:
					if(!inBuffer.empty() && inBuffer.front() == 126)
						inBuffer.pop();
					return KEY_DELETE;
				case 0x48:
					return KEY_HOME;
				case 0x46:
					return KEY_END;
				case 0x44:
					return KEY_LEFT;
				case 0x43:
					return KEY_RIGHT;
				case 0x41:
					return KEY_UP;
				case 0x42:
					return KEY_DOWN;
				case 0x35:
					inBuffer.pop();
					return KEY_PAGEUP;
				case 0x36:
					inBuffer.pop();
					return KEY_PAGEDOWN;
					
				}
			}
		}
		return c;
	}
}

}
