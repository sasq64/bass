#include "src/emulator.h"

#include "gl/texture.hpp"

#include "context.hpp"
#include "pixel_console.hpp"
#include "font.hpp"
#include "image.hpp"
#include "machine.hpp"
#include "system.hpp"
#include "vec2.hpp"

#include <array>
#include <filesystem>
#include <format>
#include <fstream>
#include <istream>

namespace fs = std::filesystem;

template <typename Result = std::string, typename Stream>
static Result read_all(Stream& in)
{
    Result contents;
    auto here = in.tellg();
    in.seekg(0, std::ios::end);
    contents.resize(in.tellg() - here);
    in.seekg(here, std::ios::beg);
    in.read(reinterpret_cast<char*>(contents.data()), contents.size());
    in.close();
    return contents;
}

static std::vector<uint8_t> read_file(fs::path name)
{
    std::ifstream jpg_file;
    jpg_file.open(name, std::ios::binary);
    return read_all<std::vector<uint8_t>>(jpg_file);
}

// clang-format off
constexpr static std::array modeTemplate = {
        "",
        "A",

        "#$%02x",
        "$%04x",

        "$%02x",
        "$%02x,x",
        "$%02x,y",
        "($%02x,x)",
        "($%02x),y",
        "($%02x)",

        "($%04x)",
        "$%04x",
        "$%04x,x",
        "$%04x,y",
        "$%x"
};
// clang-format on


template <typename Policy>
std::string disassemble(sixfive::Machine<Policy>& m, uint32_t* pc)
{
    auto code = m.read_mem(*pc);
    (*pc)++;
    auto const& instructions = sixfive::Machine<Policy>::getInstructions();
    typename sixfive::Machine<Policy>::Opcode opcode{};

    std::string name = "???";
    for (auto const& i : instructions) {
        for (auto const& o : i.opcodes) {
            if (o.code == code) {
                name = i.name;
                opcode = o;
                break;
            }
        }
    }
    auto sz = opSize(opcode.mode);
    int32_t arg = 0;
    if (sz == 3) {
        arg = m.read_mem(*pc) | (m.read_mem((*pc)+1) << 8);
        *pc += 2;
    } else if (sz == 2) {
        arg = m.read_mem((*pc)++);
    }

    if (opcode.mode == sixfive::Mode::REL) {
        arg = (static_cast<int8_t>(arg)) + *pc;
    }

    char temp[256];
    snprintf(temp, sizeof(temp), modeTemplate.at(static_cast<int>(opcode.mode)), arg);
    return name + " " + temp;
}


struct CheckPolicy : public sixfive::DefaultPolicy {

    sixfive::Machine<CheckPolicy>& machine;

    static constexpr int Read_AccessMode = sixfive::Banked;
    static constexpr int Write_AccessMode = sixfive::Banked;

    explicit CheckPolicy(sixfive::Machine<CheckPolicy>& m) : machine(m) {}

    static constexpr void ioWrite(CheckPolicy& p, uint8_t val)
    {
        printf("WRITE %02x to $01\n", val);
    }

    int ops = 0;
    static bool eachOp(CheckPolicy& dp)
    {
        auto& m = dp.machine;
        static int lastpc = -1;
        const auto [a, x, y, sr, sp, pc] = m.regs();
        if (m.regPC() == lastpc) {
            printf("STALL @ %04x A %02x X %02x Y %02x SR %02x SP %02x\n",
                   lastpc, a, x, y, sr, sp);
            for (int i = 0; i < 256; i++)
                printf("%02x ", m.get_stack(i));
            printf("\n");

            return true;
        }
        lastpc = m.regPC();
        uint32_t p = lastpc;
        //auto res = disassemble(m, &p);
        //printf("%04x : [%02x %02x %02x %02x] %s\n", lastpc, a, x, y, sr, res.c_str());
        dp.ops++;
        return false;
    }

    static inline bool doTrace = false;
};



int main()
{
    auto sys = create_glfw_system();

    int cols = 40;
    int rows = 25;

    auto screen = sys->init_screen({
        .screen = ScreenType::Window,
        .display_width = cols * 8 * 2,
        .display_height = rows * 8 * 2});

    auto [realw, realh] = screen->get_size();
    auto context = std::make_shared<pix::Context>(realw, realh, 0);
    context->vpscale = screen->get_scale();

    sys->init_input();

    std::vector<uint32_t> pixels(realw*realh);
    for (auto y = 0; y<realh ; y++) {
        for (auto x = 0; x<realw ; x++) {
            pixels[x+y*realw] = 0xff00ffff;
        }
    }

    auto bg = gl_wrap::TexRef(realw, realh);
    bg.tex->update(pixels.data());

    auto font = std::make_shared<TileSet>(FreetypeFont::unscii);
    auto console = std::make_shared<PixConsole>(cols, rows, font);

    for (auto y = 0; y<rows ; y++) {
        for (auto x = 0; x<cols ; x++) {
            console->put_char(x, y, '0' + x + y);
        }
    }

    sixfive::Machine<CheckPolicy> m;

    auto kernal = read_file("kernal.c64");
    auto basic = read_file("basic.c64");
    m.map_rom(0xe0, kernal.data(), std::ssize(kernal));
    m.map_rom(0xa0, basic.data(), std::ssize(basic));
    auto boot = m.read_mem(0xfffc) | (m.read_mem(0xfffd)<<8);
    printf("Booting %04x\n", boot);
    m.setPC(boot);
    m.run(15000);

    while(true) {
        if(!sys->run_loop()) { break; }
        context->set_target();
        context->blit(bg,  {0,0}, Vec2f{bg.tex->size()});
        console->render();
        screen->swap();
    }

}
