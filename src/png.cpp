#include "defines.h"

#include <coreutils/bitmap.h>

#include <lodepng.h>

#include <coreutils/file.h>
#include <coreutils/log.h>
#include <cstdint>
#include <string>

std::vector<uint8_t> convertPalette(size_t colors, const unsigned char* pal)
{
    std::vector<uint8_t> pal12;
    for (size_t i = 0; i < colors; i++) {
        auto r = pal[i * 4];
        auto g = pal[i * 4 + 1];
        auto b = pal[i * 4 + 2];
        uint16_t c = ((r & 0xf0) << 4) | (g & 0xf0) | (b >> 4);
        pal12.push_back(c & 0xff);
        pal12.push_back(c >> 8);
    }
    return pal12;
}

struct Tiles
{
    std::vector<uint8_t> indexes;
    std::vector<uint8_t> tiles;
};

struct Image
{
    std::vector<uint8_t> pixels;
    std::vector<uint8_t> colors;
};

Image load_png(std::string_view const& name)
{
    unsigned w{};
    unsigned h{};

    utils::File f{name};
    auto data = f.readAll();

    LodePNGState state;
    lodepng_state_init(&state);
    state.info_raw.colortype = LCT_PALETTE;
    state.info_raw.bitdepth = 8;
    state.info_raw.palette = nullptr;
    state.decoder.color_convert = 0;
    unsigned char* o{};
    auto error = lodepng_decode(&o, &w, &h, &state, data.data(), data.size());
    std::unique_ptr<unsigned char> out{o};

    auto colors = state.info_png.color.palettesize;
    auto* pal = state.info_png.color.palette;

    return {};
}

image::bitmap8 fromMonochrome(int32_t w, int32_t h, uint8_t* pixels)
{
    image::bitmap8 bm{w, h};

    uint8_t* ptr = bm.data();
    int32_t v = 0;
    int32_t s = 0;
    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            if (s == 0) {
                s = 7;
                v = *pixels++;
            }
            *ptr++ = (v >> s) & 1;
            s--;
        }
    }
    return bm;
}

std::vector<uint8_t> toMonochrome(image::bitmap8 const& bm)
{
    uint8_t const* ptr = bm.data();
    std::vector<uint8_t> result(bm.width() * bm.height() / 8);
    uint8_t* pixels = result.data();
    int32_t v = 0;
    int32_t s = 7;
    for (int32_t y = 0; y < bm.height(); y++) {
        for (int32_t x = 0; x < bm.width(); x++) {
            auto c = *ptr++;
            v |= ((c & 1) << s);
            s--;
            if (s == -1) {
                s = 7;
                *pixels++ = v;
                v = 0;
            }
        }
    }
    return result;
}

std::vector<uint8_t> layoutTiles(std::vector<uint8_t> const& pixels, int stride,
                                 int w, int h)
{
    std::vector<uint8_t> result(pixels.size());
    uint8_t* target = result.data();
    auto* src = pixels.data();
    int line = 0;

    LOGI("%d bytes at %p, stride %d", pixels.size(), (void*)pixels.data(),
         stride);

    while (true) {
        if ((line + 1) * h * stride > (int64_t)pixels.size()) {
            break;
        }
        
        auto* start = pixels.data() + line * h * stride;
        src = start;
        while (src - start < stride) {
            LOGI("Reading from %p", (void*)src);
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    *target++ = *src++;
                }
                src = (src - w) + stride;
            }
            src = src - stride * h + w;
        }
        LOGI("Next Line");
        line++;
    }
    result.resize(target - result.data());
    return result;
}

AnyMap loadPng(std::string_view const& name)
{
    unsigned w{};
    unsigned h{};
    AnyMap res;

    utils::File f{name};
    auto data = f.readAll();

    LodePNGState state;
    lodepng_state_init(&state);
    state.info_raw.colortype = LCT_PALETTE;
    state.info_raw.bitdepth = 8;
    state.info_raw.palette = nullptr;
    state.decoder.color_convert = 0;
    unsigned char* o{};
    auto error = lodepng_decode(&o, &w, &h, &state, data.data(), data.size());
    std::unique_ptr<unsigned char> out{o};

    auto colors = state.info_png.color.palettesize;
    auto* pal = state.info_png.color.palette;

    if (error == 0) {
        auto pal12 = convertPalette(colors, pal);

        LOGD("Loaded %dx%d file, %d colors", w, h, colors);

        auto bpp = state.info_raw.bitdepth;
        res["width"] = num(w);
        res["bpp"] = bpp;
        res["height"] = num(h);
        res["pixels"] =
            std::vector<uint8_t>(out.get(), out.get() + w * h * bpp / 8);
        res["colors"] = pal12;

        image::bitmap8 bm;
        if (bpp == 1) {
            bm = fromMonochrome(w, h, out.get());
        } else {
            bm = image::bitmap8(w, h, out.get());
        }

        std::unordered_map<uint32_t, int> tiles_crc{};
        std::vector<uint8_t> indexes;
        std::vector<uint8_t> tiles;
        tiles.resize(8 * 8, 0);

        int splitSize = 16;

        int count = 0;
        for (auto const& tile : bm.split(splitSize, splitSize)) {
            auto crc = tile.crc();
            auto it = tiles_crc.find(crc);
            int index = -1;
            if (it == tiles_crc.end()) {
                index = count;
                tiles_crc[crc] = count++;
                if (bpp == 1) {
                    auto pixels = toMonochrome(tile);
                    tiles.insert(tiles.end(), pixels.begin(), pixels.end());
                } else {
                    tiles.insert(tiles.end(), tile.data(),
                                 tile.data() + splitSize * splitSize);
                }
            } else {
                index = it->second;
            }
            index++;
            indexes.push_back(index & 0xff);
            indexes.push_back((index >> 8) & 0x3);
        }

        res["tiles"] = tiles;
        res["indexes"] = indexes;

        LOGD("%d different tiles (out of %d)", tiles_crc.size(),
             indexes.size() / 2);
    }
    lodepng_state_cleanup(&state);
    return res;
}
