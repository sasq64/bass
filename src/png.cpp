
#include <coreutils/bitmap.h>
#include <coreutils/file.h>
#include <coreutils/log.h>

#include <lodepng.h>

#include <cstdint>

#include "png.h"

// Return a simple distance between two colors
int32_t dist(uint32_t a, uint32_t z)
{
    int32_t r = (int32_t)((a >> 16) & 0xff) - ((z >> 16) & 0xff);
    int32_t g = (int32_t)((a >> 8) & 0xff) - ((z >> 8) & 0xff);
    int32_t b = (int32_t)(a & 0xff) - (z & 0xff);
    r = r < 0 ? -r : r;
    g = g < 0 ? -g : g;
    b = b < 0 ? -b : b;
    return r + g + b;
}

// Remap the pixel indexes in pixel according to the convert map
void remap_pixels(std::vector<uint8_t>& pixels, int bpp,
                  std::unordered_map<int, int> const& convert)
{

    for_all_pixels(pixels, bpp, [&](uint8_t& px) { px = convert.at(px); });
}

void remap_image(Image& image, std::vector<uint32_t> const& colors)
{

    auto conv = remap_palette(image.colors, colors);
    remap_pixels(image.pixels, image.bpp, conv);
    image.colors = colors;
}

std::vector<uint8_t> change_bpp(std::vector<uint8_t> const& pixels, int old_bpp,
                                int new_bpp)
{
    std::vector<uint8_t> block;
    int bits = 8 - new_bpp;
    // mc = bits = 6
    size_t cpb = 8 / new_bpp;

    std::vector<uint8_t> out;

    for_all_pixels(pixels, old_bpp, [&](uint8_t px) {
        block.push_back(px);
        if (block.size() == cpb) {
            uint8_t res = 0;
            int n = bits;
            for (auto& b : block) {
                res |= (b << n);
                n -= new_bpp;
            }
            block.clear();
            out.push_back(res);
        }
    });
    return out;
}

void changeImageBpp(Image& img, int bpp)
{
    img.pixels = change_bpp(img.pixels, img.bpp, bpp);
    img.bpp = bpp;
}

int savePng(std::string const& name, Image const& img)
{
    std::vector<uint32_t> dump;
    dump.reserve(img.pixels.size() * 8 / img.bpp);
    for_all_pixels(img.pixels, img.bpp, [&](uint8_t px) {
        dump.push_back(0xff000000 | img.colors[px]);
    });

  auto data = (const unsigned char*)img.pixels.data();

  unsigned error;
  LodePNGState state;
  lodepng_state_init(&state);

  auto colors = img.colors;
  for(auto& c : colors) {
      c |= 0xff000000;
  }
  state.info_raw.colortype = LCT_PALETTE;
  state.info_raw.bitdepth = img.bpp;
  state.info_raw.palette = (unsigned char*)colors.data();
  state.info_raw.palettesize = img.colors.size();

  state.info_png.color.colortype = LCT_PALETTE;
  state.info_png.color.bitdepth = img.bpp;
  state.info_png.color.palettesize = img.colors.size();
  state.info_png.color.palette = (unsigned char*)colors.data();

  state.encoder.auto_convert = 0;
  state.encoder.force_palette = 1;

  unsigned char* buffer;
  size_t buffersize;
  lodepng_encode(&buffer, &buffersize, data, img.width, img.height, &state);
  error = state.error;

  if(!error) error = lodepng_save_file(buffer, buffersize, name.c_str());
  free(buffer);

  return error;

}

std::vector<uint8_t> convertPalette(std::vector<uint32_t> const& colors32)
{
    std::vector<uint8_t> pal12;
    for (auto const& col : colors32) {
        uint16_t c =
            ((col << 4) & 0xf00) | ((col >> 8) & 0xf0) | ((col >> 20) & 0xf);
        pal12.push_back(c & 0xff);
        pal12.push_back(c >> 8);
    }
    return pal12;
}

std::vector<uint8_t> convertPalette(std::vector<double> const& colors32)
{
    std::vector<uint8_t> pal12;
    for (auto const& colf : colors32) {
        auto col = static_cast<uint32_t>(colf);
        uint16_t c =
            ((col << 4) & 0xf00) | ((col >> 8) & 0xf0) | ((col >> 20) & 0xf);
        pal12.push_back(c & 0xff);
        pal12.push_back(c >> 8);
    }
    return pal12;
}

Image loadPng(std::string_view const& name)
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
    if (error != 0) {
        return {};
    }
    std::unique_ptr<unsigned char> out{o};

    auto numColors = state.info_png.color.palettesize;
    auto* pal = state.info_png.color.palette;

    if(state.info_png.color.colortype == LCT_GREY) {
        numColors = 1 << state.info_png.color.bitdepth;
        pal = nullptr;
    }

    Image result;
    result.width = w;
    result.height = h;
    result.bpp = state.info_raw.bitdepth;
    result.colors.resize(numColors);
    result.pixels.resize(w * h * result.bpp / 8);
    memcpy(result.pixels.data(), out.get(), result.pixels.size());
    if(pal == nullptr) {
        for(size_t i=0; i<numColors; i++) {
            unsigned c = 255 * i / (numColors-1);
            result.colors[i] = (c << 16) | (c << 8) | c;
        }

    } else {
        memcpy(result.colors.data(), pal, numColors * 4);
    }

    lodepng_state_cleanup(&state);

    return result;
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
                                 int w, int h, int gap)
{

    size_t tileCount = pixels.size() / (w * h);
    size_t extra = gap * tileCount;

    std::vector<uint8_t> result(pixels.size() + extra);
    uint8_t* target = result.data();
    int line = 0;

    while (true) {
        if ((line + 1) * h * stride > static_cast<int64_t>(pixels.size())) {
            break;
        }

        const auto* start = pixels.data() + line * h * stride;
        const auto* src = start;
        while (src - start < stride) {
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    *target++ = *src++;
                }
                src = (src - w) + stride;
            }
            src = src - stride * h + w;
            for (int i = 0; i < gap; i++) {
                *target++ = 0;
                extra--;
            }
        }
        line++;
    }
    result.resize(target - result.data());
    return result;
}

std::vector<uint8_t> indexTiles(std::vector<uint8_t>& pixels, int size)
{
    std::unordered_map<uint32_t, int> tiles_crc{};
    std::vector<uint8_t> indexes;
    std::vector<uint8_t> tiles;
    tiles.resize(8 * 8, 0);

    int count = 0;
    auto it = pixels.begin();
    auto tileCount = pixels.size() / size;
    for (size_t i = 0; i < tileCount; i++) {
        auto crc = crc32(reinterpret_cast<const uint32_t*>(&(*it)), size / 4);
        int index = -1;
        auto tile = tiles_crc.find(crc);
        if (tile == tiles_crc.end()) {
            index = count;
            tiles_crc[crc] = count++;
            it += size;
        } else {
            index = tile->second;
            std::copy(it + size, pixels.end(), it);
            pixels.resize(pixels.size() - size);
        }
        indexes.push_back(index & 0xff);
        indexes.push_back((index >> 8) & 0x3);
    }
    return indexes;
}
