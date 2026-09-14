#pragma once

#include <string>
#include <vector>

namespace kadath {

// Filesystem / misc helpers shared across modules.

// Per-user data directory (created lazily). Linux: $XDG_CONFIG_HOME/kadath or
// ~/.config/kadath. Windows: %APPDATA%\kadath. Can be overridden with the
// KADATH_DATA_DIR environment variable (useful for testing / portable builds).
std::string dataDir();

// Directory containing the current executable (trailing slash stripped).
std::string exeDir();

// Create a directory (and parents) if missing. Returns false on failure.
bool ensureDir(const std::string& path);

std::string readFile(const std::string& path);
bool writeFile(const std::string& path, const std::string& content);

// Returns true if the file exists.
bool fileExists(const std::string& path);

} // namespace kadath