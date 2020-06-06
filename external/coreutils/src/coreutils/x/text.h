#pragma once
#include <string>
#include <vector>

namespace utils {

inline std::string rstrip(const std::string& x, char c = ' ')
{
    auto l = x.length();
    if (c == 10) {
        while (l > 1 && (x[l - 1] == 10 || x[l - 1] == 13))
            l--;
    } else {
        while (l > 1 && x[l - 1] == c)
            l--;
    }
    if (l == x.length())
        return x;
    return x.substr(0, l);
}

inline std::string lstrip(const std::string& x, char c = ' ')
{
    size_t l = 0;
    while (x[l] && x[l] == c)
        l++;
    if (l == 0)
        return x;
    return x.substr(l);
}

inline std::string lrstrip(const std::string& x, char c = ' ')
{
    size_t l = 0;
    while (x[l] && x[l] == c)
        l++;
    size_t r = l;
    while (x[r] && x[r] != c)
        r++;
    return x.substr(l, r - l);
}

inline std::vector<std::string> text_wrap(const std::string& t, int width,
                                          int initialWidth)
{
    std::vector<std::string> lines;
    size_t start = 0;
    size_t end = width;
    int subseqWidth = width;
    if (initialWidth >= 0)
        width = initialWidth;

    std::string text = t;
    for (auto& c : text) {
        if (c == 0xa)
            c = ' ';
    }

    // Find space from right
    while (true) {
        if (end > text.length()) {
            lines.push_back(text.substr(start));
            break;
        }
        auto pos = text.rfind(' ', end);
        if (pos != std::string::npos && pos > start) {
            lines.push_back(text.substr(start, pos - start));
            start = pos + 1;
        } else {
            lines.push_back(text.substr(start, width));
            start += width;
        }
        width = subseqWidth;
        end = start + width;
    }
    return lines;
}
} // namespace utils
