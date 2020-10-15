#include "wrap.h"

#include "defines.h"
#include "machine.h"
#include "script.h"
#include <peglib.h>

#include <coreutils/log.h>
#include <cstdio>
#include <string>

using namespace std::string_literals;

SVWrap::SVWrap(peg::SemanticValues const& s) : sv(s) {}

const char* SVWrap::c_str() const
{
    if(!tokSet) {
        tok = std::string(sv.token());
        tokSet = true;
    }
    return tok.c_str();
}

std::any SVWrap::operator[](size_t i) const
{
    return sv[i];
}
std::string const& SVWrap::token() const
{
    if(!tokSet) {
        tok = std::string(sv.token());
        tokSet = true;
    }
    return tok;
}

std::string_view SVWrap::token_view() const
{
    return sv.token();
}

const std::string& SVWrap::name() const
{
    return sv.name();
}

std::type_info const& SVWrap::type(size_t i) const
{
    return sv[i].type();
}

size_t SVWrap::size() const
{
    return sv.size();
}

std::pair<size_t, size_t> SVWrap::line_info() const
{
    return sv.line_info();
}

template <>
std::vector<std::string> SVWrap::transform() const
{
    return sv.transform<std::string>();
}
template <>
std::vector<std::string_view> SVWrap::transform() const
{
    return sv.transform<std::string_view>();
}
template <>
std::vector<double> SVWrap::transform() const
{
    return sv.transform<double>();
}

template <>
std::vector<std::any> SVWrap::transform() const
{
    return sv.transform<std::any>();
}

ParserWrapper::ParserWrapper(const char *s)
    : p(std::make_unique<peg::parser>(s))
{
    if(!(*p)) {
        fprintf(stderr, "Error:: Illegal grammar\n");
        exit(0);
    }
    p->log = [&](size_t line, size_t col, std::string const& msg) {
        LOGI("%d:%s", line, msg);
        currentError.message = msg;
        if (currentError.line <= 0) {
            currentError.line = line;
            currentError.column = col;
        }
        currentError.message = msg;
    };
}
ParserWrapper::~ParserWrapper() = default;

void ParserWrapper::packrat() const
{
    p->enable_packrat_parsing();
}

Error ParserWrapper::parse(std::string_view source, const char* file,
                           size_t line)
{
    try {
        currentError.line = 0;
        currentError.failed = false;
        if (file != nullptr) {
            currentError.file = file;
        }
        auto rc = p->parse_n(source.data(), source.length(), file);
        if(!rc) {
            currentError.failed = true;
        }
        if (currentError.line > 0 && line > 0) {
            currentError.line += (line - 1);
        } else {
            currentError.line = 0;
        }
    } catch (peg::parse_error& e) {
        fmt::print("Parse error: {}\n", e.what());
        currentError.message = e.what();
        currentError.line = line;
    }
    return currentError;
}

Error ParserWrapper::parse(std::string_view source, size_t line)
{
    return parse(source, nullptr, line);
}

Error ParserWrapper::parse(std::string_view source, std::string const& file)
{
    return parse(source, file.c_str(), 1);
}

void ParserWrapper::enter(
    const char* name,
    std::function<void(const char*, size_t, std::any&)> const& fn) const
{
    (*p)[name].enter = fn;
}

void ParserWrapper::leave(
    const char* name, std::function<void(const char*, size_t, size_t, std::any&,
                                         std::any&)> const& fn) const
{
    (*p)[name].leave = fn;
}

void ParserWrapper::action(const char* name,
                           std::function<std::any(SVWrap const&)> const& fn)
{
    (*p)[name] = [fn, this](peg::SemanticValues const& sv) -> std::any {
        SVWrap s(sv);
        try {
            return fn(s);
        } catch (dbz_error&) {
            LOGW("DBZ");
            return std::any();
        } catch (parse_error& e) {
            LOGI("Caught %s", e.what());
            throw peg::parse_error(e.what());
        } catch (script_error& e) {
            std::string w = e.what();
            auto r = w.find("]:");
            auto colon = w.find(':', r + 2);
            if (r != std::string::npos && colon != std::string::npos) {
                auto ln = std::stol(w.substr(r + 2), nullptr, 10);
                currentError.line = ln + sv.line_info().first - 1;
                w = "lua: "s + w.substr(colon + 2);
            }
            throw peg::parse_error(w.c_str());
        } catch (assert_error& e) {
            throw peg::parse_error(e.what());
        } catch (machine_error& e) {
            throw peg::parse_error(e.what());
        } catch (sym_error& e) {
            throw peg::parse_error(e.what());
        } catch (std::bad_any_cast&) {
            throw peg::parse_error("Data type error");
        } catch (utils::io_exception& e) {
            throw peg::parse_error(e.what());
        }
    };
}
