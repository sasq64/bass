#pragma once

#include "enums.h"
#include "symbol_table.h"

//#include <sol/forward.hpp>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <vector>
#include <any>

using Number = double;

inline std::string const& persist(std::string_view& sv)
{
    static std::unordered_set<std::string> persisted;
    auto const& res = *persisted.emplace(sv).first;
    sv = res;
    return res;
}

template <typename T> inline T number(std::any const& v) {
    return static_cast<T>(std::any_cast<Number>(v));
}

class assert_error : public std::exception
{
public:
    explicit assert_error(std::string m = "Assert failed") : msg(std::move(m))
    {}
    const char* what() const noexcept override { return msg.c_str(); }

private:
    std::string msg;
};

struct Instruction
{
    Instruction(std::string const& op, sixfive::AdressingMode m, double v) :
        opcode(op), mode(m), val((int32_t)v) {}
    std::string opcode;
    sixfive::AdressingMode mode;
    int32_t val;
};


Symbols loadSid(std::string_view const& name);
Symbols loadPng(std::string_view const& name);

enum {
    NO_SUCH_OPCODE = -1,
    ILLEGAL_ADRESSING_MODE = -2
};
