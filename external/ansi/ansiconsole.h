#pragma once

#include "console.h"

namespace bbs {

class AnsiConsole : public Console
{
public:
    AnsiConsole(Terminal& terminal);
    ~AnsiConsole();

    void putChar(Char c) override;

    virtual const std::string name() const override { return "ansi"; }

protected:
    virtual bool impl_scroll_screen(int dy) override;
    virtual void impl_color(int fg, int bg) override;
    virtual void impl_gotoxy(int x, int y) override;
    virtual int impl_handlekey() override;
    virtual void impl_clear() override;
    virtual void impl_showcursor(bool show) override;
    // virtual void impl_translate(Char &c) override;
};

} // namespace bbs
