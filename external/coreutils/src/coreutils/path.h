#pragma once

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <sys/stat.h>

#ifdef _WIN32
#    include <direct.h>
#    include <filesystem>
namespace fs = std::filesystem;

#else
#    include <dirent.h>
#    include <unistd.h>
#endif

#ifndef PATH_MAX
#    define PATH_MAX 4096
#endif

namespace utils {
/**
 * A `path` is defined by a set of _segments_ and a flag
 * that says if the path is _absolute_ or _relative.
 *
 * If the last segment is empty, it is a path without a filename
 * component.
 *
 */

class path
{
    enum Format
    {
        Unknown,
        Win,
        Unix
    };
    Format format = Format::Unknown;
    bool relative = true;
    bool hasRootDir = false;
    std::vector<std::string> segments;
    mutable std::string internal_name;

    void init(std::string const& name)
    {
        segments.clear();
        relative = true;
        hasRootDir = false;
        format = Format::Unknown;
        if (name.empty()) {
            return;
        }
        size_t start = 0;
        relative = true;
        if (!name.empty() && name[0] == '/') {
            format = Format::Unix;
            start++;
            relative = false;
        } else if (name.size() > 1 && name[1] == ':') {
            format = Format::Win;
            segments.push_back(name.substr(0, 2));
            hasRootDir = true;
            start += 2;
            if (name[2] == '\\' || name[2] == '/') {
                relative = false;
                start++;
            }
        }
        for (size_t i = start; i < name.length(); i++) {
            if (name[i] == '/' || name[i] == '\\') {
                if (format == Format::Unknown) {
                    format = name[i] == '/' ? Format::Unix : Format::Win;
                }
                segments.push_back(name.substr(start, i - start));
                start = ++i;
            }
        }
        // if (start < name.length())
        segments.push_back(name.substr(start));
    }

    std::string& segment(int i)
    {
        return i < 0 ? segments[segments.size() + i] : segments[i];
    }

    const std::string& segment(int i) const
    {
        return i < 0 ? segments[segments.size() + i] : segments[i];
    }

public:
    path() = default;
    path(std::string const& name) { init(name); }
    path(const char* name) { init(name); }
    path(std::string_view name) { init(std::string(name)); }

    void set_relative(bool rel) { relative = rel; }

    bool is_absolute() const { return !relative; }
    bool is_relative() const { return relative; }

    path& operator=(const char* name)
    {
        init(name);
        return *this;
    }

    path& operator=(std::string const& name)
    {
        init(name);
        return *this;
    }

    std::vector<std::string> parts() const { return segments; }

    path& operator/=(path const& p)
    {
        if (p.is_absolute()) {
            *this = p;
        } else {
            if (!empty() && segment(-1).empty()) {
                segments.resize(segments.size() - 1);
            }
            segments.insert(std::end(segments), std::begin(p.segments),
                            std::end(p.segments));
        }

        return *this;
    }

    path filename() const
    {
        path p = *this;
        p.relative = true;
        if (!empty()) {
            p.segments[0] = segment(-1);
            p.segments.resize(1);
        }
        return p;
    }

    std::string extension() const
    {
        if (empty()) {
            return "";
        }
        const auto& filename = segment(-1);
        auto dot = filename.find_last_of('.');
        if (dot != std::string::npos) {
            return filename.substr(dot);
        }
        return "";
    }

    std::string stem() const
    {
        if (empty()) {
            return "";
        }
        const auto& filename = segment(-1);
        auto dot = filename.find_last_of('.');
        if (dot != std::string::npos) {
            return filename.substr(0, dot);
        }
        return "";
    }

    void replace_extension(std::string const& ext)
    {
        if (empty()) {
            return;
        }
        auto& filename = segment(-1);
        auto dot = filename.find_last_of('.');
        if (dot != std::string::npos) {
            filename = filename.substr(0, dot) + ext;
        }
    }

    path parent_path() const
    {
        path p = *this;
        if (!empty()) {
            p.segments.resize(segments.size() - 1);
        }
        return p;
    }

    bool empty() const { return segments.empty(); }

    auto begin() const { return segments.begin(); }

    auto end() const { return segments.end(); }

    std::string& string() const
    {
        internal_name.clear();
        auto l = static_cast<int>(segments.size());
        int i = 0;
        std::string separator = (format == Format::Win ? "\\" : "/");
        if (!relative) {
            if (hasRootDir) {
                internal_name = segment(i++);
            }
            internal_name += separator;
        }
        for (; i < l; i++) {
            internal_name = internal_name + segment(i);
            if (i != l - 1) {
                internal_name += separator;
            }
        }
        return internal_name;
    }

    char const* c_str() const { return string().c_str(); }

    operator std::string&() const { return string(); }

    bool operator==(const char* other) const
    {
        return strcmp(other, string().c_str()) == 0;
    }

    friend std::ostream& operator<<(std::ostream& os, const path& p)
    {
        os << std::string(p);
        return os;
    }
};

inline path operator/(path const& a, path const& b)
{
    return path(a) /= b;
}

inline bool exists(path const& p)
{
    struct stat sb{};
    return stat(p.string().c_str(), &sb) >= 0;
}

inline void create_directory(path const& p)
{
#ifdef _WIN32
    _mkdir(p.string().c_str());
#else
    mkdir(p.string().c_str(), 07777);
#endif
}

inline void create_directories(path const& p)
{
    path dir;
    dir.set_relative(p.is_relative());
    for (const auto& part : p) {
        dir = dir / part;
        create_directory(dir);
    }
}

inline bool remove(path const& p)
{
    return (std::remove(p.string().c_str()) != 0);
}

inline bool copy(path const& source, path const& target)
{
    std::ifstream src(source.string(), std::ios::binary);
    std::ofstream dst(target.string(), std::ios::binary);
    dst << src.rdbuf();
    return true;
}

inline path absolute(path const& name)
{
    std::string resolvedPath;
    char* resolvedPathRaw = new char[16384];
#ifdef _WIN32
    char* result = _fullpath(resolvedPathRaw, name.string().c_str(), PATH_MAX);
#else
    char* result = realpath(name.string().c_str(), resolvedPathRaw);
#endif
    resolvedPath = (result != nullptr) ? resolvedPathRaw : name;
    if (result)
        resolvedPath = resolvedPathRaw;
    else
        resolvedPath = name;
    delete[] resolvedPathRaw;

    return path(resolvedPath);
}

#ifdef WIN32

inline std::vector<path> listFiles(path const& r)
{
    std::vector<path> rc;
    fs::path root{r.string()};
    for (auto& p : fs::directory_iterator(root)) {
        rc.emplace_back(p.path().string());
    }
    return rc;
}

#else

inline std::vector<path> listFiles(path const& root)
{
    DIR* dir{};
    struct dirent* ent{};
    std::vector<path> rc;
    if ((dir = opendir(root.c_str())) != nullptr) {
        while ((ent = readdir(dir)) != nullptr) {
            char* p = ent->d_name;
            if (p[0] == '.' && (p[1] == 0 || (p[1] == '.' && p[2] == 0)))
                continue;
            rc.emplace_back(root / ent->d_name);
        }
        closedir(dir);
    }
    return rc;
}

inline void listRecursive(const path& root, std::vector<path>& result,
                          bool includeDirs = false)
{
    DIR* dir{};
    struct dirent* ent{};
    if ((dir = opendir(root.c_str())) != nullptr) {
        while ((ent = readdir(dir)) != nullptr) {
            char* p = ent->d_name;
            if (p[0] == '.' && (p[1] == 0 || (p[1] == '.' && p[2] == 0)))
                continue;
            path f{root / ent->d_name};
            if (ent->d_type == DT_DIR) {
                if (includeDirs) result.push_back(f);
                listRecursive(f, result, includeDirs);
            } else
                result.push_back(f);
        }
        closedir(dir);
    }
}

inline std::vector<path> listRecursive(const path& root,
                                       bool includeDirs = false)
{
    std::vector<path> rc;
    listRecursive(root, rc, includeDirs);
    return rc;
}

#endif

} // namespace utils
