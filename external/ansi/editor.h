#pragma once

#include "console.h"
#include <functional>

namespace bbs {

class Editor {
};

class LineEditor : public Editor {
public:
	LineEditor(Console &console, int width = 0, int pwChar = -1);
	LineEditor(Console &console, std::function<int(int)> filter, int width = 0, int pwChar = -1);
	int update(int msec); // Convinience function
	bool putKey(int key);
	bool isDone();
	std::string getResult();
	std::wstring getWResult();
	void refresh();

	void setString(const std::string &text);
	void setString(const std::wstring &text);
	void setXY(int x = -1, int y = -1);
	void setCursor(int pos);
	void setWidth(int w) { width = w; }

	void setFilterFunction(std::function<int(int)> filter) {
		filterFunction = filter;
	}

	int getCursor() { return xpos; }
	int getLength() { return line.length(); }
	int getOffset() { return xoffset; }

protected:
	Console &console;
	std::wstring line;

	std::function<int(int)> filterFunction;

	int startX;
	int startY;
	int fg;
	int bg;

	int width;
	int maxlen;

	//bool password;
	//std::string filter;

	int xpos;
	int xoffset;

	int pwChar;
};

class SimpleEditor : public Editor {
public:
	SimpleEditor(Console &console);
	int update(int msec);
	std::string getResult();
private:
	Console &console;
	int lineNo;
	std::vector<std::wstring> lines;
	std::unique_ptr<LineEditor> lineEd;
};

class FullEditor : public Editor {
public:
	FullEditor(Console &console);
	int update(int msec);
	std::string getResult();
	void refresh();

	void setString(const std::string &text);
	void setString(const std::wstring &text);
	void setComment(const std::string &text);

protected:
	void redraw(bool lines, int cursor = -1);

	Console &console;

	int startX;
	int startY;
	int width;
	int height;

	int yscroll;

	int lineNo;
	int commentLines;
	std::vector<std::wstring> lines;
	std::unique_ptr<LineEditor> lineEd;
};

}

