#include <cstdint>
#include <vector>
#include <string_view>

struct Image
{
    int32_t width{};
    int32_t height{};
    std::vector<uint8_t> pixels;
    std::vector<uint32_t> colors;
    uint32_t bpp{};
    explicit operator bool() const { return bpp != 0; }
};


Image loadPng(std::string_view const& name);
std::vector<uint8_t> layoutTiles(std::vector<uint8_t> const& pixels, int stride, int w, int h);
std::vector<uint8_t> indexTiles(std::vector<uint8_t>& pixels, int size);
std::vector<uint8_t> convertPalette(std::vector<uint32_t> const& colors32);
