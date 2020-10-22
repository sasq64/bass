#include "wrap.h"

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
const std::string& SemanticValues::name() const
{
    return ast->name;
}

Parser::Parser(const char* s)
    : p(std::make_unique<peg::parser>(s))
{
    if (!(*p)) {
        fprintf(stderr, "Error:: Illegal grammar\n");
        exit(0);
    }
    p->enable_ast<peg::AstBase<BassNode>>();
    p->log = [&](size_t line, size_t col, std::string const& msg) {
        LOGI("ERROR %d/%d %s", line, col, msg);
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
        fn(node);
        forAllNodes(node, fn);
    }
    fn(root);
}

AstNode Parser::parse(std::string_view source, std::string_view file)
{
    try {
        AstNode ast;
        bool rc = p->parse_n(source.data(), source.length(), ast);
        if (rc) {

            //ast = peg::AstOptimizer(true, exceptions).optimize(ast);

            forAllNodes(ast, [source, file, this](auto const& node) {
                node->source = source;
                node->file_name = file;
                auto it = postActions.find(node->name);
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
        auto it0 = preActions.find(ast->name);
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

void Parser::leave(
    const char* name, std::function<void(const char*, size_t, size_t, std::any&,
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
