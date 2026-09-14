#include "util.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifndef _WIN32
#include <unistd.h>
#include <limits.h>
#else
#include <windows.h>
#endif

namespace kadath {

std::string dataDir() {
    if (const char* overrideDir = std::getenv("KADATH_DATA_DIR"); overrideDir && *overrideDir) {
        return overrideDir;
    }
#ifdef _WIN32
    if (const char* appdata = std::getenv("APPDATA"); appdata && *appdata) {
        return std::string(appdata) + "/kadath";
    }
    return "kadath";
#else
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) {
        return std::string(xdg) + "/kadath";
    }
    if (const char* home = std::getenv("HOME"); home && *home) {
        return std::string(home) + "/.config/kadath";
    }
    return ".config/kadath";
#endif
}

std::string exeDir() {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n > 0) {
        std::string p(buf, n);
        auto slash = p.find_last_of("\\/");
        return slash == std::string::npos ? "." : p.substr(0, slash);
    }
    return ".";
#else
    char buf[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf));
    if (n > 0) {
        std::string p(buf, (size_t)n);
        auto slash = p.find_last_of('/');
        return slash == std::string::npos ? "." : p.substr(0, slash);
    }
    return ".";
#endif
}

bool ensureDir(const std::string& path) {
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    return !ec;
}

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool writeFile(const std::string& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(content.data(), (std::streamsize)content.size());
    out.close();
    return (bool)out;
}

bool fileExists(const std::string& path) {
    return std::filesystem::exists(path);
}

} // namespace kadath