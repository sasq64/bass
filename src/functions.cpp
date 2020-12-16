#include "assembler.h"
#include "chars.h"
#include "parser.h"
#include "png.h"

#include <cmath>
#include <coreutils/algorithm.h>
#include <coreutils/file.h>
#include <lodepng.h>

template <typename Out, typename In>
std::vector<Out> convert_vector(std::vector<In> const& in)
{
    std::vector<Out> out;
    std::transform(std::cbegin(in), std::cend(in), std::back_inserter(out),
                   [](In const& a) { return static_cast<Out>(a); });
    return out;
}

Image to_image(AnyMap const& img)
{
    Image image;
    image.bpp = number<int32_t>(img.at("bpp"));
    image.width = number<int32_t>(img.at("width"));
    image.height = number<int32_t>(img.at("height"));
    image.pixels = std::any_cast<std::vector<uint8_t>>(img.at("pixels"));
    auto colors = std::any_cast<std::vector<Number>>(img.at("colors"));
    image.colors = convert_vector<uint32_t>(colors);
    return image;
}

AnyMap from_image(Image const& image)
{
    AnyMap res;
    res["width"] = num(image.width);
    res["bpp"] = num(image.bpp);
    res["height"] = num(image.height);
    res["pixels"] = image.pixels;
    res["colors"] = convert_vector<Number>(image.colors);
    return res;
}

void initFunctions(Assembler& a)
{
    auto& syms = a.getSymbols();

    syms.set("Math.Pi", M_PI);
    syms.set("M_PI", M_PI);

    // Allowed data types:
    // * Any arithmetic type, but they will always be converted to/from double
    // * `std::vector<uint8_t>` for binary data
    // * `AnyMap` for returning struct like things
    // * `std::vector<std::any> const&` as single argument.

    a.registerFunction("log", [](double f) { return std::log(f); });
    a.registerFunction("exp", [](double f) { return std::exp(f); });
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
    a.registerFunction("trunc", [](double a) { return std::trunc(a); });
    a.registerFunction("abs", [](double a) { return std::abs(a); });

    a.registerFunction(
        "random", []() { return static_cast<double>(std::rand()) / RAND_MAX; });

    a.registerFunction("compare",
                       [](std::vector<uint8_t> const& v0,
                          std::vector<uint8_t> const& v1) { return v0 == v1; });

    a.registerFunction("load", [&](std::string_view name) {
        auto p = fs::path(name);
        if (p.is_relative()) {
            p = a.getCurrentPath() / p;
        }
        try {
            utils::File f{p.string()};
            return f.readAll();
        } catch (utils::io_exception&) {
            throw parse_error(fmt::format("Could not load {}", name));
        }
    });

    a.registerFunction("to_array", [&](int n, Assembler::Macro const& m) {
            std::vector<Number> v(n);
            Assembler::Call call;
            for(int i=0; i<n; i++) {
                call.args = { any_num(i) }; 
                v[i] = number(a.applyDefine(m, call));
            }
            return v;
    });

    a.registerFunction("word", [](std::vector<uint8_t> const& data) {
        Check(data.size() >= 2, "Need at least 2 bytes");
        return data[0] | (data[1] << 8);
    });

    a.registerFunction("big_word", [](std::vector<uint8_t> const& data) {
        Check(data.size() >= 2, "Need at least 2 bytes");
        return data[1] | (data[0] << 8);
    });

    a.registerFunction("translate", [](std::vector<uint8_t> const& data) {
        std::vector<uint8_t> res;
        res.reserve(data.size());
        for (auto d : data) {
            res.push_back(translateChar(d));
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

    a.registerFunction("to_upper", [](std::string_view sv) {
        auto s = std::string(sv);
        for (auto& c : s) {
            c = toupper(c);
        }
        return persist(s);
    });

    a.registerFunction("to_lower", [](std::string_view sv) {
        auto s = std::string(sv);
        for (auto& c : s) {
            c = tolower(c);
        }
        return persist(s);
    });

    a.registerFunction("str", [](double n) {
        auto s = std::to_string(static_cast<int64_t>(n));
        std::string_view sv = s;
        persist(sv);
        return sv;
    });

    a.registerFunction("index_tiles",
                       [&](std::vector<uint8_t> const& pixels, int32_t size) {
                           AnyMap result;
                           auto pixelCopy = pixels;
                           std::vector<uint8_t> v = indexTiles(pixelCopy, size);
                           result["indexes"] = v;
                           result["tiles"] = pixelCopy;
                           return result;
                       });

    a.registerFunction("layout_tiles", [&](const std::vector<uint8_t>& pixels,
                                           int32_t stride, int32_t w, int32_t h,
                                           int32_t gap) {
        std::vector<uint8_t> v = layoutTiles(pixels, stride, w, h, gap);
        return v;
    });

    a.registerFunction(
        "layout_image", [&](AnyMap const& img, int32_t w, int32_t h) {
            auto image = to_image(img);

            auto stride = image.width * image.bpp / 8;
            auto tw = w * image.bpp / 8;
            auto th = h;

            image.pixels = layoutTiles(image.pixels, stride, tw, th, 0);
            image.width = w;
            image.height = (int32_t)(image.pixels.size() / tw);
            return from_image(image);
        });

    a.registerFunction(
        "save_png", [&](std::string_view name, AnyMap const& img) {
            auto error = savePng(std::string(name), to_image(img));
            if (error != 0) {
                throw parse_error(fmt::format("Could not save '{}'", name));
            }
            return img;
        });

    a.registerFunction("convert_palette",
                       [&](std::vector<Number> const& colors, int) {
                           return convertPalette(colors);
                       });

    a.registerFunction("change_bpp", [&](AnyMap const& img, int32_t bpp) {
        auto image = to_image(img);
        changeImageBpp(image, bpp);
        return from_image(image);
    });

    a.registerFunction("remap_image",
                       [&](AnyMap const& img, std::vector<Number> const& pal) {
                           auto image = to_image(img);
                           remap_image(image, convert_vector<uint32_t>(pal));
                           return from_image(image);
                       });

    a.registerFunction("load_png", [&](std::string_view name) {
        auto p = fs::path(name);
        if (p.is_relative()) {
            p = a.getCurrentPath() / p;
        }

        auto image = loadPng(p.string());

        if (image) {
            return from_image(image);
        }
        throw parse_error("Could not load png");
    });

    // TODO: Remove
    a.registerFunction("to_monochrome",
                       [&](std::vector<uint8_t> const& pixels) {
                           std::vector<uint8_t> result(pixels.size() / 8);
                           uint8_t* out = result.data();
                           int32_t v = 0;
                           int32_t s = 7;
                           for (auto const& c : pixels) {
                               v |= ((c & 1) << s);
                               s--;
                               if (s == -1) {
                                   s = 7;
                                   *out++ = v;
                                   v = 0;
                               }
                           }
                           return result;
                       });
}
