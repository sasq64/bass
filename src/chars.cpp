#include "chars.h"
#include "defines.h"
#include "petscii.h"
#include "wrap.h"

#include <coreutils/utf8.h>

#include <cstdint>
#include <unordered_map>

static std::unordered_map<int32_t, uint8_t> char_translate;

static std::array translationNames{"ascii", "petscii_upper", "petscii_lower",
                                   "screencode_upper", "screencode_lower"};
Translation currentTranslation = Translation::Ascii;

void setTranslation(Translation t)
{
    int32_t (*ptr)(uint8_t) = nullptr;
    currentTranslation = t;
    switch (t) {
    case Translation::Ascii:
        ptr = [](uint8_t i) -> int32_t { return i; };
        break;
    case Translation::PetsciiUpper:
        ptr = &pet2uni_up;
        break;
    case Translation::PetsciiLower:
        ptr = &pet2uni_lo;
        break;
    case Translation::ScreencodeUpper:
        ptr = &sc2uni_up;
        break;
    case Translation::ScreencodeLower:
        ptr = &sc2uni_lo;
        break;
    }
    char_translate.clear();
    for (int i = 0; i < 128; i++) {

        auto u = ptr(i);
        char_translate[u] = i;
    }
}
void setTranslation(std::string_view name)
{
    for (size_t i = 0; i < translationNames.size(); i++) {
        auto const& n = translationNames[i];
        if (n == name) {
            setTranslation((Translation)i);
            return;
        }
    }
    throw parse_error(fmt::format("Unknown encoding '{}'", name));
}

void setTranslation(char32_t c, uint8_t p)
{
    char_translate[c] = p;
}

uint8_t translateChar(uint32_t c)
{
    auto it = char_translate.find(c);
    if (it != char_translate.end()) {
        return it->second;
    }
    auto s = utils::utf8_encode({c});
    throw parse_error(
        fmt::format("Char '{}' (0x{:x}) not available in '{}'", s, c,
                    translationNames[static_cast<int>(currentTranslation)]));
}

