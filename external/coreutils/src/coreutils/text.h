#pragma once
#include <string>
#include <vector>

namespace utils {

inline static void makeUpper(std::string& s)
{
    for (auto& c : s)
        c = static_cast<char>(toupper(c));
}

inline static std::string toUpper(const std::string& s)
{
    std::string s2 = s;
    makeUpper(s2);
    return s2;
}

inline static void makeLower(std::string& s)
{
    for (auto& c : s)
        c = static_cast<char>(tolower(c));
}

inline static std::string toLower(const std::string& s)
{
    std::string s2 = s;
    makeLower(s2);
    return s2;
}

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

inline bool endsWith(const std::string& name, const std::string& ext)
{
    auto pos = name.rfind(ext);
    return (pos != std::string::npos && pos == name.length() - ext.length());
}

inline bool startsWith(const std::string_view& name, const std::string_view& pref)
{
    if (pref.empty())
        return true;
    return name.find(pref) == 0;
}

} // namespace utils
