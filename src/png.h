#pragma once
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

struct Image
{
    int32_t width{};
    int32_t height{};
    std::vector<uint8_t> pixels;
    std::vector<uint32_t> colors;
    uint32_t bpp{};
    explicit operator bool() const { return bpp != 0; }
};

Image loadPng(std::string_view name);
std::vector<uint8_t> layoutTiles(std::vector<uint8_t> const& pixels, unsigned stride,
                                 unsigned w, unsigned h, unsigned gap);
std::vector<double> indexTiles(std::vector<uint8_t>& pixels, int size);
std::vector<uint8_t> convertPalette(std::vector<uint32_t> const& colors32);
std::vector<uint8_t> convertPalette(std::vector<double> const& colors32);

// Return a simple distance between two colors
int32_t dist(uint32_t a, uint32_t z);

// Create a map that maps color indexes in `colors` to the closest color
// in 'newColors'
template <typename T>
std::unordered_map<int, int> remap_palette(std::vector<T> const& colors,
                                           std::vector<T> const& newColors)
{
    std::unordered_map<int, int> convert;

    int i = 0;
    for (auto const& col : colors) {
        auto col24 = static_cast<uint32_t>(col) & 0xff'ff'ff;
        int j = 0;
        int32_t minDelta = 1000;
        int32_t bestColor = -1;
        for (auto const& col2 : newColors) {
            auto new24 = static_cast<uint32_t>(col2) & 0xff'ff'ff;
            auto delta = dist(col24, new24);
            // LOGI("DELTA: %06x - %06x = %d", col24, new24, delta);
            if (delta < minDelta) {
                bestColor = j;
                minDelta = delta;
            }
            j++;
        }
        auto new24 = static_cast<uint32_t>(newColors[bestColor]) & 0xff'ff'ff;
        // LOGI("%d : %06x -> %d : %06x", i, col24, bestColor, new24);
        convert[i] = bestColor;

        i++;
    }
    return convert;
}

std::vector<uint8_t> change_bpp(std::vector<uint8_t> const& pixels, int old_bpp,
                                int new_bpp);

void changeImageBpp(Image& img, int bpp);
void remap_image(Image& image, std::vector<uint32_t> const& colors);

template <typename FN>
void for_all_pixels(std::vector<uint8_t>& pixels, int bpp, FN fn)
{
    int bits = 8 - bpp;
    uint8_t m = 0xff >> bits;

    for (auto& b : pixels) {
        int n = bits;
        while (n >= 0) {
            uint8_t x = (b >> n) & m;
            fn(x);
            b = (b & ~(m << n)) | (x << n);
            n -= bpp;
        }
    }
}

template <typename FN>
void for_all_pixels(std::vector<uint8_t> const& pixels, int bpp, FN fn)
{
    int bits = 8 - bpp;
    uint8_t m = 0xff >> bits;
    for (auto b : pixels) {
        int n = bits;
        while (n >= 0) {
            fn((b >> n) & m);
            n -= bpp;
        }
    }
}

void remap_pixels(std::vector<uint8_t>& pixels, int bpp,
                  std::unordered_map<int, int> const& convert);
int savePng(std::string const& name, Image const& img);
