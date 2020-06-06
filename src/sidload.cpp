#include "defines.h"

#include <lodepng.h>

#include <coreutils/file.h>
#include <coreutils/log.h>
#include <coreutils/utf8.h>
#include <cstdint>
#include <string>

namespace {

template <typename T>
T get(const std::vector<uint8_t>&, int)
{}

template <>
uint16_t get(const std::vector<uint8_t>& v, int offset)
{
    return (unsigned)(v[offset] << 8u) | v[offset + 1];
}

} // namespace

Symbols loadPng(std::string_view const& name)
{
    unsigned w, h;
    unsigned char* out = nullptr;
    Symbols res;

    utils::File f{name};
    auto data = f.readAll();

    unsigned error;
    LodePNGState state;
    lodepng_state_init(&state);
    state.info_raw.colortype = LCT_PALETTE;
    state.info_raw.bitdepth = 8;
    state.info_raw.palette = nullptr;
    state.decoder.color_convert = 0;
    error = lodepng_decode(&out, &w, &h, &state, data.data(), data.size());

    auto colors = state.info_png.color.palettesize;
    auto* pal = state.info_png.color.palette;

    if (error == 0) {
        std::vector<uint8_t> pal12;
        for (size_t i = 0; i < colors; i++) {
            auto r = pal[i * 4];
            auto g = pal[i * 4 + 1];
            auto b = pal[i * 4 + 2];
            uint16_t c = ((r & 0xf0) << 4) | (g & 0xf0) | (b >> 4);
            pal12.push_back(c & 0xff);
            pal12.push_back(c >> 8);
        }

        LOGD("Loaded %dx%d flle, %d colors", w, h, colors);

        res["width"] = (Number)w;
        res["height"] = (Number)h;
        res["pixels"] = std::vector<uint8_t>(out, out + w * h);
        res["colors"] = pal12;
        free(out);
    }
    lodepng_state_cleanup(&state);
    return res;
}

Symbols loadSid(std::string_view const& name)
{
    utils::File f{name};
    auto data = f.readAll();

    // auto version = get<uint16_t>(data, 0x4);
    auto start = get<uint16_t>(data, 0x6);
    auto loadAdr = get<uint16_t>(data, 0x8);
    auto initAdr = get<uint16_t>(data, 0xa);
    auto playAdr = get<uint16_t>(data, 0xc);

    if (loadAdr == 0) {
        loadAdr = data[start] | (data[start + 1] << 8);
        start += 2;
    }

    Symbols res;
    res["init"] = (Number)initAdr;
    res["init"] = (Number)initAdr;
    res["load"] = (Number)loadAdr;
    res["play"] = (Number)playAdr;
    res["data"] = std::vector<uint8_t>(data.begin() + start, data.end());

    return res;
}
