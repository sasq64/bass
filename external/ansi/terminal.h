#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace bbs {

class Terminal
{

public:

    virtual size_t write(std::string_view source) = 0;
    virtual bool read(std::string& target) = 0;

    virtual void open() {}
    virtual void close() {}

    virtual int width() const { return -1; }
    virtual int height() const { return -1; }
    virtual std::string term_type() const { return ""; }

    virtual ~Terminal() = default;
};

std::unique_ptr<Terminal> create_local_terminal();

} // namespace bbs
