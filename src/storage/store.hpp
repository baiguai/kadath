#pragma once

#include <string>
#include <vector>

namespace kadath {

struct Clip {
    std::string text;
    std::string ts; // ISO-ish "YYYY-MM-DD HH:MM:SS" timestamp string
};

// A bookmark group in the nested tree. Groups may contain clips and subgroups.
struct Group {
    std::string name;
    std::vector<Clip> clips;
    std::vector<Group> groups;
};

// Persistent clipboard data: history (newest first), pinned clips, and the
// bookmark group forest. All JSON files live under the per-user data dir.
struct Store {
    std::vector<Clip> clips;
    std::vector<Clip> pinned;
    std::vector<Group> groups;
    size_t maxClips = 250;

    // Load all three files (missing/corrupt files yield empty data).
    void load(const class AppConfig& config);

    // Save all three files. Returns false if any write failed.
    bool save(const class AppConfig& config) const;

    // A new clipboard value arrived: deduplicate (move to top) or insert at the
    // head, pruning beyond maxClips. Mutates in place; caller decides to save.
    void sinkClip(const std::string& text);

    // Remove a history entry. The pinned list is independent and untouched.
    bool eraseClip(size_t index);

    // --- bookmarks ----------------------------------------------------------
    // Resolve a path (vector of group indices) to a Group*, or nullptr if any
    // index is out of range. The empty path means the root forest.
    Group* groupAt(const std::vector<size_t>& path);
    const Group* groupAt(const std::vector<size_t>& path) const;

    // Add a clip to the group identified by slash-separated name path,
    // creating intermediate groups as needed. Returns false on empty path.
    bool addClipToPath(const std::string& namePath, const Clip& clip);

    // Create a group inside the group at `path` (empty path = root).
    bool addGroup(const std::vector<size_t>& path, const std::string& name);

    // Rename the node (group or clip) at `path`'s final index.
    bool renameNode(const std::vector<size_t>& path, const std::string& newName);

    // Delete the node (group or clip) at `path`'s final index.
    bool deleteNode(const std::vector<size_t>& path);
};

} // namespace kadath