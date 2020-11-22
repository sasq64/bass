#pragma once

#include <string>
#include <cstdint>

enum class Translation
{
    Ascii,
    PetsciiUpper,
    PetsciiLower,
    ScreencodeUpper,
    ScreencodeLower
};

void setTranslation(Translation t);
void setTranslation(std::string_view t);
void setTranslation(char32_t c, uint8_t p);

uint8_t translateChar(uint32_t c);

