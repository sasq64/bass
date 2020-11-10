
#include "editor.h"

#include <coreutils/log.h>
#include <coreutils/split.h>
//#include <coreutils/utils.h>
#include <coreutils/utf8.h>

namespace bbs {

using namespace std;
using namespace utils;

LineEditor::LineEditor(Console& console, int width, int pwChar)
    : console(console), width(width), xpos(0), xoffset(0), pwChar(pwChar)
{
    startX = console.getCursorX();
    startY = console.getCursorY();
    fg = console.getFg();
    bg = console.getBg();
    if (this->width == 0)
        this->width = console.getWidth() - startX - 1;
    maxlen = 256;
    xpos = 0;
    line = L"";
}

LineEditor::LineEditor(Console& console, function<int(int)> filter, int width,
                       int pwChar)
    : LineEditor(console, width)
{
    filterFunction = filter;
}

void LineEditor::setString(const std::string& text)
{
    line = utf8_decode(text);
    if (xpos > (int)line.size())
        xpos = (int)line.size();
    refresh();
    console.flush(true);
}

void LineEditor::setString(const std::wstring& text)
{
    line = text;
    if (xpos > (int)line.size())
        xpos = (int)line.size();
}

void LineEditor::setXY(int x, int y)
{
    if (x < 0)
        x = console.getCursorX();
    if (y < 0)
        y = console.getCursorY();
    startX = x;
    startY = y;
}

void LineEditor::setCursor(int pos)
{
    xpos = pos;
    if (xpos > (int)line.size())
        xpos = line.size();
}

int LineEditor::update(int msec)
{

    auto c = console.getKey(msec);
    if (putKey(c)) {
        refresh();
        console.flush(true);
    }
    return c;
}

bool LineEditor::putKey(int c)
{
    if (filterFunction) {
        auto rc = filterFunction(c);
        if (rc == 0)
            return c;
        else
            c = rc;
    }

    switch (c) {
    case Console::KEY_BACKSPACE:
        if (xpos > 0) {
            xpos--;
            line.erase(xpos, 1);
        }
        break;
    case Console::KEY_DELETE:
        if (xpos < (int)line.length()) {
            line.erase(xpos, 1);
        }
        break;
    case Console::KEY_LEFT:
        if (xpos > 0)
            xpos--;
        break;
    case Console::KEY_HOME:
        xpos = 0;
        break;
    case Console::KEY_END:
        xpos = line.length();
        break;
    case Console::KEY_RIGHT:
        if (xpos < (int)line.length())
            xpos++;
        break;
    case Console::KEY_ESCAPE:
        xpos = 0;
        line = L"";
        break;
    default:
        if (c < 0x10000) {
            if ((int)line.length() < maxlen) {
                line.insert(xpos++, 1, c);
            }
        } else
            return false;
        break;
    }

    auto endX = startX + width;
    auto cursorX = startX + xpos - xoffset;
    auto dx = startX - cursorX;

    if (dx > 0) {
        xoffset -= dx;
        cursorX += dx;
    }
    dx = cursorX - endX;
    if (dx >= 0) {
        xoffset += dx;
        cursorX -= dx;
    }

    // refresh();
    // console.moveCursor(cursorX, startY);
    return true;
}

void LineEditor::refresh()
{

    int ll = (int)line.length();
    auto scroll = width / 4;
    if (scroll < 4)
        scroll = 4;

    auto d = 2;
    if (ll - xoffset <= width)
        d = 1;

    // LOGD("xpos %d xoffset %d ll %d", xpos, xoffset, ll);
    if (xpos - xoffset > width - d) {
        xoffset = xpos - width + scroll - 1;
        // LOGD("xoffset %d", xoffset);
    } else if (xpos - xoffset < 1) {
        xoffset = xpos - scroll;
        // LOGD("xoffset %d", xoffset);
    }
    if (xoffset < 0)
        xoffset = 0;

    auto cursorX = xpos + startX - xoffset;

    console.fill(bg, startX, startY, width, 1);
    auto l = line.substr(xoffset, width);
    if (pwChar > 0) {
        for (auto& c : l)
            c = pwChar;
    }
    console.put(startX, startY, l, fg, bg);
    if (xoffset > 0)
        console.put(startX, startY, '$', Console::YELLOW);
    if ((int)line.length() - xoffset > width)
        console.put(startX + width - 1, startY, '$', Console::YELLOW);

    // console.flush(false);
    console.moveCursor(cursorX, startY);
    console.showCursor(true);
}

// ABC

string LineEditor::getResult()
{
    return utils::utf8_encode(line);
}

wstring LineEditor::getWResult()
{
    return line;
}
//

SimpleEditor::SimpleEditor(Console& console) : console(console) {}

FullEditor::FullEditor(Console& console) : console(console)
{
    console.clear();
    console.moveCursor(0, 0);
    lineEd = make_unique<LineEditor>(console);
    lineNo = 0;
    yscroll = 0;
    startX = 0;
    startY = 1;
    commentLines = 0;
    width = console.getWidth();
    // height = console.getHeight() - 2;
    height = console.getHeight() - 1;

    lineEd->setXY(startX, startY);
    lineEd->setWidth(width);
    lines.push_back(L"");

    // console.fill(Console::RED, 0, 0, 2, 0);
    // console.fill(Console::RED, 19, 0, 2, 0);

    console.fill(Console::BLUE, 0, 0, 0, 1);
    console.put(1, 0, format("%02d/%02d", lineNo + 1, lines.size()),
                Console::WHITE, Console::BLUE);
    console.put(-10, 0, "F7 = Save", Console::WHITE, Console::BLUE);
    console.flush();
}

void FullEditor::setComment(const std::string& text)
{
    lines = split(utf8_decode(text), wstring(L"\n"));
    commentLines = lines.size();
    redraw(true, 0);
}

void FullEditor::redraw(bool full, int cursor)
{

    if (lineEd->getOffset() > 0) {
        lineEd->setCursor(0);
        lineEd->refresh();
    }

    int scroll = height / 4;
    if (scroll < 3)
        scroll = 3;

    // auto startLine = lineNo-yscroll-1;
    // if(startLine < 0) startLine = 0;
    auto startLine = 0;

    if (lineNo - yscroll >= height) {
        // Scroll down
        yscroll++; // = lineNo - height + 1;
        console.fill(Console::BLUE, 0, 1, 0, 1);
        console.put(-10, 0, "F7 = Save", Console::WHITE, Console::BLUE);
        console.flush();
        console.scrollScreen(1);
        startLine = 0;
        full = true;
        LOGD("yscroll %d", yscroll);
    } else if (lineNo - yscroll < 0) {
        // Scroll up
        yscroll = lineNo - scroll;
        startLine = 0;
        full = true;
        LOGD("yscroll %d", yscroll);
    }

    if (yscroll < 0)
        yscroll = 0;

    if (full) {
        for (int i = startLine; i < height; i++) {
            console.fill(Console::BLACK, startX, i + startY, width, 1);
            if (i + yscroll >= (int)lines.size())
                break;
            if (i < commentLines)
                console.setColor(0xc);
            console.put(startX, i + startY,
                        lines[i + yscroll].substr(0, width));
            console.setColor(0);
            if ((int)lines[i + yscroll].length() > width)
                console.put(startX + width - 1, i + startY, '$',
                            Console::YELLOW);
        }
    }
    console.flush();

    lineEd->setXY(startX, lineNo + startY - yscroll);
    lineEd->setString(lines[lineNo]);
    if (cursor >= 0)
        lineEd->setCursor(cursor);
    lineEd->refresh();
}

int FullEditor::update(int msec)
{
    auto xpos = lineEd->getCursor();
    auto rc = lineEd->update(msec);

    if (rc == Console::KEY_TIMEOUT)
        return rc;

    // auto line = lineEd->getWResult();
    int lcount = (int)lines.size();

    switch (rc) {
    case Console::KEY_ENTER: {
        auto x = lineEd->getCursor();
        auto l = lineEd->getWResult();
        auto l0 = l.substr(0, x);
        auto l1 = l.substr(x);
        lines[lineNo] = l0;
        lineNo++;
        lines.insert(lines.begin() + lineNo, l1);
        redraw(true, 0);
    } break;
    case Console::KEY_BACKSPACE:
        if (xpos == 0 && lineNo > 0) {
            auto l = lineEd->getWResult();
            lines.erase(lines.begin() + lineNo);
            lineNo--;
            auto len = lines[lineNo].length();
            lines[lineNo] = lines[lineNo] + l;
            redraw(true, len);
            // console.fill(Console::BLACK, startX, startY + lines.size(),
            // width, 1);
        }
        break;
    case Console::KEY_DELETE:
        if (xpos == lineEd->getLength() && lineNo < (int)lines.size() - 1) {

            auto l = lineEd->getWResult();
            lineEd->setString(l + lines[lineNo + 1]);
            lines.erase(lines.begin() + lineNo + 1);
            redraw(true);
            // console.fill(Console::BLACK, startX, startY +lines.size(), width,
            // 1);
        }
        break;
    case Console::KEY_LEFT:
        if (xpos == 0 && lineNo > 0) {
            lines[lineNo--] = lineEd->getWResult();
            redraw(false, lines[lineNo].length());
        }
        break;
    case Console::KEY_RIGHT:
        if (xpos == lineEd->getLength() && lineNo < (int)lines.size() - 1) {
            lines[lineNo++] = lineEd->getWResult();
            redraw(false, 0);
        }
        break;
    case Console::KEY_UP:
        if (lineNo > 0) {
            lines[lineNo--] = lineEd->getWResult();
            redraw(false);
        }
        break;
    case Console::KEY_PAGEUP:
    case Console::KEY_F1:
        lines[lineNo] = lineEd->getWResult();
        yscroll -= height;
        lineNo -= height;
        if (lineNo < 0)
            lineNo = 0;
        if (yscroll < 0)
            yscroll = 0;
        redraw(true);
        break;
    case Console::KEY_PAGEDOWN:
    case Console::KEY_F3:
        lines[lineNo] = lineEd->getWResult();
        yscroll += height;
        lineNo += height;
        if (lineNo > lcount - 1)
            lineNo = lcount - 1;
        if (yscroll > lcount - height)
            yscroll = lcount - height;
        redraw(true);
        break;
    case Console::KEY_DOWN:
        if (lineNo < (int)lines.size() - 1) {
            lines[lineNo++] = lineEd->getWResult();
            redraw(false);
        }
        break;
    }

    console.put(1, 0, format("%02d/%02d", lineNo + 1, lines.size()),
                Console::WHITE, Console::BLUE);
    console.flush();

    return rc;
}

std::string FullEditor::getResult()
{
    auto text = join(lines.begin(), lines.end(), L'\n');
    return utf8_encode(text);
}

void FullEditor::setString(const std::string& text)
{
    lines = split(utf8_decode(text), wstring(L"\n"));
    redraw(true, 0);
}

void FullEditor::setString(const std::wstring& text)
{
    lines = split(text, wstring(L"\n"));
    redraw(true, 0);
}

void FullEditor::refresh()
{
    redraw(true);
}

} // namespace bbs
