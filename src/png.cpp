#include "defines.h"

#include <coreutils/bitmap.h>

#include <lodepng.h>

#include <coreutils/file.h>
#include <coreutils/log.h>
#include <cstdint>
#include <string>

Symbols loadPng(std::string_view const& name)
{
    unsigned w{};
    unsigned h{};
    unsigned char* out{};
    Symbols res;

    utils::File f{name};
    auto data = f.readAll();

    LodePNGState state;
    lodepng_state_init(&state);
    state.info_raw.colortype = LCT_PALETTE;
    state.info_raw.bitdepth = 8;
    state.info_raw.palette = nullptr;
    state.decoder.color_convert = 0;
    auto error = lodepng_decode(&out, &w, &h, &state, data.data(), data.size());

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

        res.at<Number>("width") = w;
        res.at<Number>("height") = h;
        res["pixels"] = std::vector<uint8_t>(out, out + w * h);
        res["colors"] = pal12;


        image::bitmap8 bm{w, h, out};

        std::unordered_map<uint32_t, int> tiles_crc{};
        std::vector<uint8_t> indexes;
        std::vector<uint8_t> tiles;
        tiles.resize(8*8, 0);

        int count = 0;
        for(auto const& tile : bm.split(8,8)) {
            auto crc = tile.crc();
            auto it = tiles_crc.find(crc);
            int index = -1;
            if(it == tiles_crc.end()) {
                index = count;
                tiles_crc[crc] = count++;
                tiles.insert(tiles.end(), tile.data(), tile.data() + 8*8);
            } else {
                index = it->second;
            }
            index++;
            indexes.push_back(index & 0xff);
            indexes.push_back((index >> 8) & 0x3);
        }

        res["tiles"] = tiles;
        res["indexes"] = indexes;

        LOGD("%d different tiles (out of %d)", tiles_crc.size(), indexes.size() / 2);


        free(out);
    }
    lodepng_state_cleanup(&state);
    return res;
}

