#include <coreutils/format.h>

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace conio {

struct Console
{

    struct Attrs
    {
        uint32_t fg;
        uint32_t bg;
        uint16_t flags;
    };

    struct Pos
    {
        int x;
        int y;
        bool operator==(Pos const& other) const
        {
            return x == other.x && y == other.y;
        }
        bool operator!=(Pos const& other) const
        {
            return x != other.x || y != other.y;
        }
    };

    using POS = Pos;
    using ATTRS = Attrs;
    using STRING = std::string;

    virtual void setCursor(POS const& pos, ATTRS const& attrs) {}

    virtual void put(POS const& pos, ATTRS const& attrs,
                     STRING const& text) = 0;
    virtual void flush() {}

    virtual void clear() = 0;
};

class AnsiConsole : public Console
{
public:
    AnsiConsole() { fp = stdout; }

    void put(POS const& pos, ATTRS const& attrs, STRING const& text) override
    {
        commands.emplace(pos, attrs, text);
    }

    void flush() override
    {
        for (auto const& cmd : commands) {
            set_attrs(cmd.attrs);
            gotoxy(cmd.pos);
            print(cmd.text);
        }
        commands.clear();
    }

    void clear() override
    {
        std::string s = utils::format("\x1b[2J");
        fputs(s.c_str(), fp);
    }

private:
    struct Command
    {
        Command(POS pos, ATTRS attrs, STRING text)
            : pos(pos), attrs(attrs), text(text)
        {}
        POS pos;
        ATTRS attrs;
        STRING text;
        bool operator<(Command const& other) const
        {
            return pos.y == other.pos.y ? (pos.x < other.pos.x)
                                        : (pos.y < other.pos.y);
        }
    };

    std::set<Command> commands;

    void set_attrs(ATTRS const& attrs)
    {
        if (currentAttrs.fg != attrs.fg) {
            const auto ft =
                utils::format("\x1b[38;2;%d;%d;%dm", (attrs.fg >> 16),
                              (attrs.fg >> 8) & 0xff, attrs.fg & 0xff);
            fputs(ft.c_str(), fp);
        }

        if (currentAttrs.bg != attrs.bg) {
            const auto bt =
                utils::format("\x1b[48;2;%d;%d;%dm", (attrs.bg >> 16),
                              (attrs.bg >> 8) & 0xff, attrs.bg & 0xff);
            fputs(bt.c_str(), fp);
        }
        currentAttrs = attrs;
    }

    void gotoxy(POS const& pos)
    {
        if (currentPos != pos) {
            const auto s = utils::format("\x1b[%d;%dH", pos.y, pos.x);
            fputs(s.c_str(), fp);
            currentPos = pos;
        }
    }

    void print(STRING const& text)
    {
        fputs(text.c_str(), fp);
        currentPos.x += text.length();
    }

    POS currentPos;
    ATTRS currentAttrs;
    FILE* fp;
};

} // namespace conio
