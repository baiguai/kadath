#include "storage/store.hpp"

#include "storage/config.hpp"
#include "util.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <ctime>

namespace kadath {

using json = nlohmann::json;

namespace {

std::string nowStr() {
    char buf[32];
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

json clipToJson(const Clip& c) {
    return {{"text", c.text}, {"ts", c.ts}};
}

Clip clipFromJson(const json& j) {
    Clip c;
    if (j.contains("text") && j["text"].is_string()) c.text = j["text"].get<std::string>();
    if (j.contains("ts") && j["ts"].is_string()) c.ts = j["ts"].get<std::string>();
    return c;
}

json groupToJson(const Group& g) {
    json j;
    j["name"] = g.name;
    j["clips"] = json::array();
    for (const auto& c : g.clips) j["clips"].push_back(clipToJson(c));
    j["groups"] = json::array();
    for (const auto& sub : g.groups) j["groups"].push_back(groupToJson(sub));
    return j;
}

Group groupFromJson(const json& j) {
    Group g;
    if (j.contains("name") && j["name"].is_string()) g.name = j["name"].get<std::string>();
    if (j.contains("clips") && j["clips"].is_array()) {
        for (const auto& c : j["clips"]) g.clips.push_back(clipFromJson(c));
    }
    if (j.contains("groups") && j["groups"].is_array()) {
        for (const auto& s : j["groups"]) g.groups.push_back(groupFromJson(s));
    }
    return g;
}

} // namespace

void Store::load(const AppConfig& config) {
    maxClips = (size_t)config.maxClips;

    clips.clear();
    pinned.clear();
    groups.clear();

    std::string hist = readFile(config.clipsFile);
    if (!hist.empty()) {
        try {
            if (auto j = json::parse(hist); j.is_array()) {
                for (const auto& c : j) clips.push_back(clipFromJson(c));
            }
        } catch (...) {}
    }

    std::string pin = readFile(config.pinnedFile);
    if (!pin.empty()) {
        try {
            if (auto j = json::parse(pin); j.is_array()) {
                for (const auto& c : j) pinned.push_back(clipFromJson(c));
            }
        } catch (...) {}
    }

    std::string bm = readFile(config.bookmarksFile);
    if (!bm.empty()) {
        try {
            if (auto j = json::parse(bm); j.is_array()) {
                for (const auto& g : j) groups.push_back(groupFromJson(g));
            }
        } catch (...) {}
    }
}

bool Store::save(const AppConfig& config) const {
    json hist = json::array();
    for (const auto& c : clips) hist.push_back(clipToJson(c));
    json pin = json::array();
    for (const auto& c : pinned) pin.push_back(clipToJson(c));
    json bm = json::array();
    for (const auto& g : groups) bm.push_back(groupToJson(g));
    return writeFile(config.clipsFile, hist.dump(2) + "\n") &&
           writeFile(config.pinnedFile, pin.dump(2) + "\n") &&
           writeFile(config.bookmarksFile, bm.dump(2) + "\n");
}

void Store::sinkClip(const std::string& text) {
    std::string trimmed = text;
    while (!trimmed.empty() && (trimmed.back() == '\n' || trimmed.back() == '\r')) {
        trimmed.pop_back();
    }
    if (trimmed.empty()) return;

    for (size_t i = 0; i < clips.size(); ++i) {
        if (clips[i].text == trimmed) {
            if (i == 0) return; // already at the top
            Clip c = std::move(clips[i]);
            clips.erase(clips.begin() + (ptrdiff_t)i);
            clips.insert(clips.begin(), std::move(c));
            return;
        }
    }

    Clip c;
    c.text = trimmed;
    c.ts = nowStr();
    clips.insert(clips.begin(), std::move(c));
    if (clips.size() > maxClips) {
        clips.resize(maxClips);
    }
}

bool Store::eraseClip(size_t index) {
    if (index >= clips.size()) return false;
    clips.erase(clips.begin() + (ptrdiff_t)index);
    return true;
}

Group* Store::groupAt(const std::vector<size_t>& path) {
    return const_cast<Group*>(static_cast<const Store*>(this)->groupAt(path));
}

const Group* Store::groupAt(const std::vector<size_t>& path) const {
    if (path.empty()) return nullptr;
    const Group* g = nullptr;
    for (size_t idx : path) {
        const std::vector<Group>* list = g ? &g->groups : &groups;
        if (idx >= list->size()) return nullptr;
        g = &(*list)[idx];
    }
    return g;
}

bool Store::addClipToPath(const std::string& namePath, const Clip& clip) {
    if (namePath.empty()) return false;
    std::vector<Group>* place = &groups;
    size_t start = 0;
    while (true) {
        size_t dot = namePath.find('/', start);
        std::string name = namePath.substr(start, dot == std::string::npos ? namePath.size() - start : dot - start);
        if (name.empty()) {
            if (dot == std::string::npos) break;
            start = dot + 1;
            continue;
        }
        Group* g = nullptr;
        for (auto& sub : *place) {
            if (sub.name == name) { g = &sub; break; }
        }
        if (!g) {
            place->emplace_back();
            place->back().name = name;
            g = &place->back();
        }
        if (dot == std::string::npos) {
            g->clips.insert(g->clips.begin(), clip);
            return true;
        }
        place = &g->groups;
        start = dot + 1;
    }
    return false;
}

bool Store::addGroup(const std::vector<size_t>& path, const std::string& name) {
    if (name.empty()) return false;
    std::vector<Group>& target = path.empty() ? groups : groupAt(path)->groups;
    for (const auto& g : target) {
        if (g.name == name) return false; // no duplicates at same level
    }
    Group g;
    g.name = name;
    target.push_back(std::move(g));
    return true;
}

bool Store::renameNode(const std::vector<size_t>& path, const std::string& newName) {
    if (path.empty() || newName.empty() || newName.find('/') != std::string::npos) return false;
    std::vector<size_t> parent = path;
    size_t last = parent.back();
    parent.pop_back();
    if (auto* g = groupAt(parent)) {
        if (last < g->groups.size()) {
            g->groups[last].name = newName;
            return true;
        }
        size_t clipIdx = last - g->groups.size();
        if (clipIdx < g->clips.size()) {
            g->clips[clipIdx].text = newName;
            return true;
        }
    } else if (parent.empty()) {
        if (last < groups.size()) {
            groups[last].name = newName;
            return true;
        }
        size_t clipIdx = last - groups.size();
        if (clipIdx < clips.size()) {
            (void)clipIdx;
        }
    }
    return false;
}

bool Store::deleteNode(const std::vector<size_t>& path) {
    if (path.empty()) return false;
    std::vector<size_t> parent = path;
    size_t last = parent.back();
    parent.pop_back();
    if (auto* g = groupAt(parent)) {
        if (last < g->groups.size()) {
            g->groups.erase(g->groups.begin() + (ptrdiff_t)last);
            return true;
        }
        size_t clipIdx = last - g->groups.size();
        if (clipIdx < g->clips.size()) {
            g->clips.erase(g->clips.begin() + (ptrdiff_t)clipIdx);
            return true;
        }
    } else if (parent.empty()) {
        if (last < groups.size()) {
            groups.erase(groups.begin() + (ptrdiff_t)last);
            return true;
        }
        size_t clipIdx = last - groups.size();
        if (clipIdx < clips.size()) {
            (void)clipIdx;
        }
    }
    return false;
}

} // namespace kadath