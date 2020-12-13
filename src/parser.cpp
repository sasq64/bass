#include "parser.h"

#include "defines.h"
#include <peglib.h>

#include <coreutils/log.h>
#include <cstdio>
#include <string>

using namespace std::string_literals;

struct BassNode
{
    std::vector<std::any> v;
    std::string_view source;
    std::string_view file_name;
    ActionFn* action{};
};

AstNode get_child(AstNode node, size_t i)
{
    return node->nodes.size() > i ? node->nodes[i] : nullptr;
}

SemanticValues::SemanticValues(AstNode const& a) : ast(a) {}

std::pair<size_t, size_t> SemanticValues::line_info() const
{
    return {ast->line, ast->column};
}
std::any SemanticValues::operator[](size_t i) const
{
    return ast->v[i];
}
std::string_view SemanticValues::token_view() const
{
    if (!ast->is_token) {
        return ast->source.substr(ast->position, ast->length);
    }
    return ast->token;
}
size_t SemanticValues::size() const
{
    return ast->v.size();
}

std::string_view SemanticValues::name() const
{
    return ast->name;
}

Parser::Parser(const char* s) : p(std::make_unique<peg::parser>(s))
{
    if (useCache) {
        SHA512(reinterpret_cast<const uint8_t*>(s), strlen(s),
               grammarSHA.data());
    }
    if (!(*p)) {
        fprintf(stderr, "Error:: Illegal grammar\n");
        exit(0);
    }
    p->enable_ast<peg::AstBase<BassNode>>();
    p->log = [&](size_t line, size_t, std::string const& msg) {
        if (!haveError) {
            LOGI("Msg %s", msg);
            setError(msg, "", line);
        }
    };
}
Parser::~Parser() = default;

void Parser::packrat() const
{
    p->enable_packrat_parsing();
}

std::any Parser::callAction(SemanticValues& sv, ActionFn const& fn)
{
    try {
        return fn(sv);
    } catch (std::exception& e) {
        setError(e.what(), sv.get_node()->file_name, sv.line_info().first);
        throw;
    }
}

template <typename FN>
void forAllNodes(AstNode const& root, FN const& fn)
{
    for (auto const& node : root->nodes) {
        forAllNodes(node, fn);
    }
    fn(root);
}

template <typename FN>
void forAllNodesTop(AstNode const& root, FN const& fn)
{
    fn(root);
    for (auto const& node : root->nodes) {
        forAllNodesTop(node, fn);
    }
}

// template <typename FN>
// void save(AstNode const& root, FN const& fn)
//{
//    std::vector<uint8_t> data;
//    for (auto const& node : root->nodes) {
//        save(node, fn);
//    }
//    fn(root);
//}

void write(utils::File& f, uint32_t const& t)
{
    if ((t & 0xffff'8000) == 0) {
        f.write<uint16_t>(t);
        return;
    }
    f.write<uint16_t>((t & 0x7fff) | 0x8000);
    f.write<uint16_t>(t >> 15);
}

uint32_t read(utils::File& f)
{
    uint32_t t = f.read<uint16_t>();
    if ((t & 0x8000) == 0) {
        return t;
    }
    t &= 0x7fff;
    t |= (f.read<uint16_t>() << 15);
    return t;
}

void Parser::saveAst(utils::File& f, AstNode const& root)
{
    forAllNodesTop(root, [&f, this](AstNode const& node) {
        write(f, node->line);
        write(f, node->position);
        write(f, node->length);

        auto index = ruleMap[node->name];
        write(f, index);
        write(f, node->nodes.size());
    });
}

AstNode Parser::loadAst(utils::File& f)
{
    auto line = read(f);
    auto column = 0;
    auto pos = read(f);
    auto len = read(f);
    auto index = read(f);
    auto name = ruleNames[index];
    auto count = read(f);

    auto token = currentSource.substr(pos, len);
    auto node = std::make_shared<peg::AstBase<BassNode>>(
        currentFile.c_str(), line, column, name, token, pos, len);

    for (size_t i = 0; i < count; i++) {
        node->nodes.push_back(loadAst(f));
    }

    return node;
}

AstNode Parser::parse(std::string_view source, std::string_view file)
{
    std::array<uint8_t, SHA512_DIGEST_LENGTH> sha; // NOLINT
    SHA512(reinterpret_cast<const uint8_t*>(source.data()), source.size(),
           sha.data());
    auto shaName = hex_encode(sha, 32);
    fs::path home = getHomeDir();
    if (!fs::exists(home / ".basscache")) {
        fs::create_directories(home / ".basscache");
    }
    fs::path target = home / ".basscache" / shaName;

    currentSource = source;
    currentFile = file;

    p->get_rule_names(ruleNames);

    try {
        AstNode ast = nullptr;
        bool rc = false;
        if (useCache && fs::exists(target)) {
            fmt::print("Using cached AST\n");
            utils::File f{target.string()};
            auto id = f.read<uint32_t>();
            if (id != 0xba55a570) {
                fmt::print(stderr, "**Error: Broken AST\n");
                exit(1);
            }
            std::array<uint8_t, 32> gs; //NOLINT
            f.read(gs.data(), 32);
            if(memcmp(gs.data(), grammarSHA.data(), 32) == 0) {
                ast = loadAst(f);
                rc = true;
            } else {
                fmt::print("**Warn: Old grammar in AST\n");
            }
        }

        if(ast == nullptr)
        {
            rc = p->parse_n(source.data(), source.length(), ast);
            if (rc) {
                for (size_t i = 0; i < ruleNames.size(); i++) {
                    ruleMap[ruleNames[i]] = i;
                }
                if (useCache) {
                    utils::File f{target.string(), utils::File::Mode::Write};
                    f.write<uint32_t>(0xba55a570);
                    f.write(grammarSHA.data(), 32);
                    saveAst(f, ast);
                    f.close();
                }
            }
        }
        if (rc) {

            // ast = peg::AstOptimizer(true, exceptions).optimize(ast);
            forAllNodes(ast, [source, file, this](auto const& node) {
                node->source = source;
                node->file_name = file;
                auto it = postActions.find(std::string(node->name));
                if (it != postActions.end()) {
                    node->action = &it->second;
                }
            });
            return ast;
        }
        currentError.file = file;
        return nullptr;
    } catch (peg::parse_error& e) {
        fmt::print("## Unhandled Parse error: {}\n", e.what());
        setError(e.what(), file, 0);
        return nullptr;
    }
}

std::any Parser::evaluate(AstNode const& node)
{
    // LOGI("Evaluate %s", node->name);
    std::string spaces{
        "                                                        "};

    std::function<std::any(AstNode const&, int)> eval =
        [&](AstNode const& ast, int indent) -> std::any {
        bool descend = true;
        auto it0 = preActions.find(std::string(ast->name));
        if (it0 != preActions.end()) {
            SemanticValues sv{ast};
            descend = it0->second(sv);
        }

        if (descend) {
            const auto& nodes = ast->nodes;
            ast->v.clear();
            for (auto const& node : nodes) {
                auto v = eval(node, indent - 2);
                ast->v.push_back(v);
            }
        }
        if (ast->action != nullptr) {
            SemanticValues sv{ast};
            if (tracing) {
                fmt::print("\n{} (line {}): "
                           "'{}'\n-------------------------------------\n",
                           sv.name(), ast->line, sv.token_view());
                for (size_t i = 0; i < sv.size(); i++) {
                    std::any v = sv[i];
                    fmt::print("  {}: {}\n", i, any_to_string(v));
                }
                auto ret = callAction(sv, *ast->action);
                fmt::print(">>  {}\n", any_to_string(ret));
                return ret;
            }
            return callAction(sv, *ast->action);
        }
        if (!ast->v.empty()) {
            return ast->v.front();
        }
        return {};
    };
    return eval(node, 54);
}

void Parser::enter(
    const char* name,
    std::function<void(const char*, size_t, std::any&)> const& fn) const
{
    (*p)[name].enter = fn;
}

void Parser::leave(const char* name,
                   std::function<void(const char*, size_t, size_t, std::any&,
                                      std::any&)> const& fn) const
{
    (*p)[name].leave = fn;
}

void Parser::setError(std::string const& what, std::string_view file,
                      size_t line)
{
    if (!haveError) {
        currentError.message = what;
        currentError.file = file;
        currentError.line = line;
        haveError = true;
    }
}

void Parser::after(const char* name,
                   std::function<std::any(SemanticValues const&)> const& fn)
{
    auto names = p->get_rule_names();
    auto it = std::find(names.begin(), names.end(), name);
    if (it == names.end()) {
        LOGI("Unknown rule %s", name);
    }
    postActions[name] = fn;
}

void Parser::before(const char* name,
                    std::function<bool(SemanticValues const&)> const& fn)
{
    auto names = p->get_rule_names();
    auto it = std::find(names.begin(), names.end(), name);
    if (it == names.end()) {
        throw parse_error("Unknown rule "s + name);
    }
    preActions[name] = fn;
}
