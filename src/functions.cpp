#include "assembler.h"
#include <cmath>
#include <coreutils/file.h>

static uint8_t translate(uint8_t c)
{
    if (c >= 'a' && c <= 'z') return c - 'a' + 1;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 0x41;
    return c;
}

void initFunctions(Assembler& a)
{
    auto& syms = a.getSymbols();

    syms.at<Symbols>("Math").at<Number>("Pi") = M_PI;

    // Allowed data types:
    // * Any arithmetic type, but they will always be converted to/from double
    // * `std::vector<uint8_t>` for binary data
    // * `Symbols` for returning struct like things
    // * `std::vector<std::any> const&` as single argument.

    a.registerFunction("sqrt", [](double f) { return std::sqrt(f); });

    a.registerFunction("sin", [](double f) { return std::sin(f); });
    a.registerFunction("cos", [](double f) { return std::cos(f); });
    a.registerFunction("tan", [](double f) { return std::tan(f); });
    a.registerFunction("asin", [](double f) { return std::asin(f); });
    a.registerFunction("acos", [](double f) { return std::acos(f); });
    a.registerFunction("atan", [](double f) { return std::atan(f); });

    a.registerFunction("min",
                       [](double a, double b) { return std::min(a, b); });
    a.registerFunction("max",
                       [](double a, double b) { return std::max(a, b); });
    a.registerFunction("pow",
                       [](double a, double b) { return std::pow(a, b); });
    a.registerFunction("len",
                       [](std::vector<uint8_t> const& v) { return v.size(); });
    a.registerFunction("round", [](double a) { return std::round(a); });

    a.registerFunction("compare",
                       [](std::vector<uint8_t> const& v0,
                          std::vector<uint8_t> const& v1) { return v0 == v1; });

    a.registerFunction("load", [&](std::string_view name) {
        auto p = utils::path(name);
        if (p.is_relative()) {
            p = a.getCurrentPath() / p;
        }
        utils::File f{p};
        return f.readAll();
    });

    a.registerFunction("word", [](std::vector<uint8_t> const& data) {
        return data[0] | (data[1] << 8);
    });

    a.registerFunction("to_cbm", [](std::vector<uint8_t> const& data) {
        std::vector<uint8_t> res;
        for (auto& d : data) {
            res.push_back(translate(d));
        }
        return res;
    });

    a.registerFunction("zeroes", [](int32_t sz) {
        std::vector<uint8_t> res(sz);
        std::fill(res.begin(), res.end(), 0);
        return res;
    });

    a.registerFunction("bytes", [](std::vector<std::any> const& args) {
        std::vector<uint8_t> res;
        res.reserve(args.size());
        for (auto const& a : args) {
            res.push_back(number<uint8_t>(a));
        }
        return res;
    });

    a.registerFunction("str", [](double n) {
        auto s = std::to_string((int64_t)n);
        std::string_view sv = s;
        persist(sv);
        return sv;
    });

    a.registerFunction("load_sid",
                       [](std::string_view name) { return loadSid(name); });
    a.registerFunction("load_png", [&](std::string_view name) {
        auto p = utils::path(name);
        if (p.is_relative()) {
            p = a.getCurrentPath() / p;
        }
        return loadPng(p.string());
    });
}
