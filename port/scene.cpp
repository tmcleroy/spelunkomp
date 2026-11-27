// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; from cps2recomp's port/scene.cpp (its files, scene.json and
// clip writers, so that cps2recomp's scene diorama opens what this saves).

#include "scene.h"

#include "png_write.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace port {

namespace {
    constexpr int kMostWaiting = 120; // (a clip's frames waiting to be written: past that it is cut short)

    std::filesystem::path PathOf(const std::string &text) {
        return std::filesystem::path(std::u8string(text.begin(), text.end()));
    }
    // A folder of its own under `under` ("name", or "name 2" and on), made; "" if it could not be
    std::string NewFolder(const std::string &under, const std::string &name) {
        std::error_code error;
        std::string folder = under + name;
        for (int n = 2; std::filesystem::exists(PathOf(folder), error); n++) {
            folder = under + name + " " + std::to_string(n);
        }
        std::filesystem::create_directories(PathOf(folder), error);
        return error ? "" : folder + "/";
    }
    std::string Hex(uint32_t colour) {
        char text[8];
        snprintf(text, sizeof(text), "#%02x%02x%02x", colour & 0xFF, (colour >> 8) & 0xFF, (colour >> 16) & 0xFF);
        return text;
    }
    std::string Quoted(const std::string &text) {
        std::string out = "\"";
        for (const char c : text) {
            if (c == '"' || c == '\\') {
                out += '\\';
            }
            out += c;
        }
        return out + "\"";
    }
    std::string Number(double value) {
        char text[32];
        snprintf(text, sizeof(text), "%.6g", value);
        return text;
    }
    bool WriteAll(const std::string &path, const std::string &text) {
        FILE *f = fopen(path.c_str(), "wb");
        if (!f) {
            return false;
        }
        const bool written = fwrite(text.data(), 1, text.size(), f) == text.size();
        return fclose(f) == 0 && written;
    }
    // (`fast`: a clip's frames, written as fast as zlib goes, so that the writers keep up with the game)
    bool Png(const std::string &path, const std::vector<uint32_t> &pixels, int width, int height, bool fast = false) {
        return WritePng(path.c_str(), pixels.data(), static_cast<uint32_t>(width), static_cast<uint32_t>(height), true, fast ? 1 : 6);
    }

    // `pixels` (w x h) turned `degrees` clockwise, as port/main.cpp's TurnTo turns the board's picture; w and h swapped
    // for a quarter turn
    void Turn(std::vector<uint32_t> &pixels, int &w, int &h, int degrees) {
        if (degrees % 360 == 0 || pixels.empty()) {
            return;
        }
        const int tw = degrees % 180 ? h : w, th = degrees % 180 ? w : h;
        std::vector<uint32_t> out(pixels.size());
        for (int oy = 0; oy < th; oy++) {
            for (int ox = 0; ox < tw; ox++) {
                int x = ox, y = oy;
                switch (degrees) {
                case 90: x = oy; y = h - 1 - ox; break;
                case 180: x = w - 1 - ox; y = h - 1 - oy; break;
                case 270: x = w - 1 - oy; y = ox; break;
                default: break;
                }
                out[static_cast<size_t>(oy) * tw + ox] = pixels[static_cast<size_t>(y) * w + x];
            }
        }
        pixels.swap(out);
        w = tw;
        h = th;
    }
    // A box (x, y, w, h) of a w x h picture where Turn puts it
    void TurnBox(int &x, int &y, int &bw, int &bh, int w, int h, int degrees) {
        const int x0 = x, y0 = y;
        switch (degrees) {
        case 90: x = h - (y0 + bh); y = x0; std::swap(bw, bh); break;
        case 180: x = w - (x0 + bw); y = h - (y0 + bh); break;
        case 270: x = y0; y = w - (x0 + bw); std::swap(bw, bh); break;
        default: break;
        }
    }

    std::string LayerJson(const SceneLayer &layer, bool still) {
        std::string json = "{\"name\": " + Quoted(layer.name);
        if (layer.board != "overlay") {
            json += ", \"board\": " + Quoted(layer.board) + ", \"kind\": " + Quoted(layer.kind);
            if (layer.tile) {
                json += ", \"tile\": " + std::to_string(layer.tile);
            }
            if (!layer.extra.empty()) {
                json += ", " + layer.extra;
            }
        }
        if (still) {
            json += ", \"file\": " + (layer.empty ? std::string("null") : Quoted(layer.name + ".png"));
        } else {
            json += ", \"folder\": " + Quoted(layer.name);
        }
        return json + "}";
    }
    std::string BoxesJson(const Scene &scene) {
        std::string json;
        for (const SceneBox &box : scene.boxes) {
            json += std::string(json.empty() ? "" : ", ") + "{\"player\": " + (box.player < 0 ? "null" : std::to_string(box.player + 1)) +
                    ", \"kind\": " + Quoted(box.kind) + ", \"x\": " + std::to_string(box.x) + ", \"y\": " + std::to_string(box.y) +
                    ", \"width\": " + std::to_string(box.width) + ", \"height\": " + std::to_string(box.height) +
                    ", \"colour\": " + Quoted(Hex(box.colour)) + ", \"fill\": " + std::to_string(box.fill) + "}";
        }
        return "[" + json + "]";
    }
    // The entries' list for scene.json, each with where it is in entries.png (`atX`; none: a clip's frame)
    std::string EntriesJson(const std::vector<SceneEntry> &entries, const std::vector<int> &atX, const std::vector<int> &atY,
                            const std::string &indent) {
        std::string json;
        for (size_t n = 0; n < entries.size(); n++) {
            const SceneEntry &e = entries[n];
            char tile[16], attributes[16];
            snprintf(tile, sizeof(tile), "0x%05X", e.tile);
            snprintf(attributes, sizeof(attributes), "0x%04X", e.attributes);
            json += indent + "{\"board\": \"game\", \"entry\": " + std::to_string(e.entry) + ", \"tile\": " + Quoted(tile) +
                    ", \"colour\": " + std::to_string(e.colour) + ", \"level\": " + std::to_string(e.level) +
                    ", \"attributes\": " + Quoted(attributes) + ", \"box\": [" + std::to_string(e.x) + ", " + std::to_string(e.y) + ", " +
                    std::to_string(e.w) + ", " + std::to_string(e.h) + "]" +
                    (n < atX.size() ? ", \"atlas\": [" + std::to_string(atX[n]) + ", " + std::to_string(atY[n]) + "]" : "") +
                    ", \"layer\": " + Quoted(e.layer) + ", \"part\": null, \"what\": null}" + (n + 1 < entries.size() ? ",\n" : "\n");
        }
        return json;
    }
    // The map of the entry each sprite pixel is of, as a picture: 1 + its place in the list, red the low byte, green the
    // high; nothing where no sprite is
    std::vector<uint32_t> EntryMapPixels(const Scene &scene) {
        std::vector<uint32_t> map(scene.entryMap.size());
        for (size_t n = 0; n < map.size(); n++) {
            map[n] = scene.entryMap[n] ? (scene.entryMap[n] & 0xFFFFu) | 0xFF000000u : 0;
        }
        return map;
    }
    // A still's sprites each by itself: entries.png, every entry's picture side by side (a shelf at a time, a pixel
    // between them), and entry-map.png; and their list for scene.json
    bool WriteEntries(const Scene &scene, const std::string &folder, std::string &json) {
        const auto &entries = scene.entries;
        int atlasWidth = 1024;
        for (const SceneEntry &e : entries) {
            atlasWidth = std::max(atlasWidth, e.w);
        }
        std::vector<size_t> byHeight(entries.size()); // (the tallest first, so that the shelves waste little)
        for (size_t n = 0; n < entries.size(); n++) {
            byHeight[n] = n;
        }
        std::stable_sort(byHeight.begin(), byHeight.end(), [&](size_t a, size_t b) { return entries[a].h > entries[b].h; });
        std::vector<int> atX(entries.size()), atY(entries.size());
        int x = 0, y = 0, shelf = 0;
        for (const size_t n : byHeight) {
            if (x + entries[n].w > atlasWidth) {
                x = 0;
                y += shelf + 1;
                shelf = 0;
            }
            atX[n] = x;
            atY[n] = y;
            x += entries[n].w + 1;
            shelf = std::max(shelf, entries[n].h);
        }
        const int atlasHeight = std::max(1, y + shelf);
        std::vector<uint32_t> atlas(static_cast<size_t>(atlasWidth) * atlasHeight, 0);
        for (size_t n = 0; n < entries.size(); n++) {
            const SceneEntry &e = entries[n];
            for (int row = 0; row < e.h && !e.pixels.empty(); row++) {
                std::copy_n(&e.pixels[static_cast<size_t>(row) * e.w], e.w, &atlas[static_cast<size_t>(atY[n] + row) * atlasWidth + atX[n]]);
            }
        }
        if (!Png(folder + "entries.png", atlas, atlasWidth, atlasHeight) ||
            !Png(folder + "entry-map.png", EntryMapPixels(scene), scene.width, scene.height)) {
            return false;
        }
        json = ",\n  \"entryAtlas\": \"entries.png\",\n  \"entryMap\": \"entry-map.png\",\n  \"entries\": [\n" +
               EntriesJson(entries, atX, atY, "    ") + "  ]";
        return true;
    }

    // A scene's files: `picture` and its layers' (the empty ones not written), named a still's "<layer>.png", a clip's
    // "<layer>/0001.png" (a clip's frame's sprites each by itself too: entry-map/0001.png, entries/0001.json)
    bool WriteScene(const Scene &scene, const std::string &folder, int number) {
        auto path = [&](const std::string &name) {
            if (number <= 0) {
                return folder + name + ".png";
            }
            char file[16];
            snprintf(file, sizeof(file), "%04d.png", number);
            std::error_code error;
            std::filesystem::create_directories(PathOf(folder + name), error);
            return folder + name + "/" + file;
        };
        const bool fast = number > 0;
        bool written = Png(path("picture"), scene.picture, scene.width, scene.height, fast);
        for (const auto *list : {&scene.layers, &scene.overlays}) {
            for (const SceneLayer &layer : *list) {
                if (!layer.empty) {
                    written = Png(path(layer.name), layer.pixels, layer.width ? layer.width : scene.width, layer.height ? layer.height : scene.height,
                                  fast) &&
                              written;
                }
            }
        }
        if (number > 0 && !scene.entries.empty()) {
            written = Png(path("entry-map"), EntryMapPixels(scene), scene.width, scene.height, true) && written;
            std::string file = path("entries");
            file.replace(file.size() - 4, 4, ".json");
            written = WriteAll(file, "{\"entries\": [\n" + EntriesJson(scene.entries, {}, {}, "  ") + "]}\n") && written;
        }
        return written;
    }
} // namespace

void TurnScene(Scene &scene, int degrees) {
    degrees = ((degrees % 360) + 360) % 360;
    if (degrees == 0) {
        return;
    }
    const int w = scene.width, h = scene.height;
    auto turn = [&](std::vector<uint32_t> &pixels) {
        int tw = w, th = h;
        Turn(pixels, tw, th, degrees);
    };
    turn(scene.picture);
    for (SceneLayer &layer : scene.layers) {
        turn(layer.pixels);
    }
    turn(scene.entryMap);
    for (SceneEntry &e : scene.entries) {
        int ew = e.w, eh = e.h;
        Turn(e.pixels, ew, eh, degrees);
        TurnBox(e.x, e.y, e.w, e.h, w, h, degrees);
    }
    for (SceneBox &b : scene.boxes) {
        TurnBox(b.x, b.y, b.width, b.height, w, h, degrees);
    }
    if (degrees % 180) {
        std::swap(scene.width, scene.height);
    }
}

SceneExport::~SceneExport() {
    {
        std::lock_guard<std::mutex> lock(m_lock);
        m_stopping = true;
    }
    m_wake.notify_all();
    for (std::thread &writer : m_writers) {
        writer.join();
    }
}

std::string SceneExport::Save(const Scene &scene, const std::string &under, const std::string &name) {
    const std::string folder = NewFolder(under, name);
    if (folder.empty() || !WriteScene(scene, folder, 0)) {
        return "SCENE NOT SAVED";
    }
    std::string entriesJson;
    if (!scene.entries.empty() && !WriteEntries(scene, folder, entriesJson)) {
        return "SCENE NOT SAVED";
    }
    std::string json = "{\n  \"kind\": \"scene\",\n  \"frame\": " + std::to_string(scene.frame) + ",\n  \"width\": " +
                       std::to_string(scene.width) + ",\n  \"height\": " + std::to_string(scene.height) + ",\n  \"pixelAspect\": " +
                       Number(scene.pixelAspect) + ",\n  \"backdrop\": " + Quoted(Hex(scene.backdrop)) +
                       ",\n  \"picture\": \"picture.png\",\n  \"layers\": [\n";
    for (size_t n = 0; n < scene.layers.size(); n++) {
        json += "    " + LayerJson(scene.layers[n], true) + (n + 1 < scene.layers.size() ? ",\n" : "\n");
    }
    json += "  ]" + entriesJson;
    if (!scene.overlays.empty()) {
        json += ",\n  \"overlays\": [\n";
        for (size_t n = 0; n < scene.overlays.size(); n++) {
            json += "    " + LayerJson(scene.overlays[n], true) + (n + 1 < scene.overlays.size() ? ",\n" : "\n");
        }
        json += "  ],\n  \"boxes\": " + BoxesJson(scene);
    }
    json += "\n}\n";
    if (!WriteAll(folder + "scene.json", json)) {
        return "SCENE NOT SAVED";
    }
    fprintf(stderr, "scene: %s\n", folder.c_str());
    const size_t at = folder.find("exports/");
    return "SCENE SAVED  " + (at == std::string::npos ? folder : folder.substr(at));
}

std::string SceneExport::StartClip(const std::string &under, const std::string &name, int frames, double fps) {
    {
        std::lock_guard<std::mutex> lock(m_lock);
        if (m_clip.on || m_finishing || !m_jobs.empty() || m_writing) {
            return "A CLIP IS STILL BEING SAVED";
        }
        m_failed = false;
    }
    m_clip.folder = NewFolder(under, name);
    if (m_clip.folder.empty()) {
        return "CLIP NOT SAVED";
    }
    m_clip.wanted = frames;
    m_clip.taken = 0;
    m_clip.missed = 0;
    m_clip.fps = fps;
    m_clip.names.clear();
    m_clip.json.clear();
    m_clip.entries = false;
    m_clip.overlayNames.clear();
    m_clip.overlaysJson.clear();
    m_clip.on = true;
    if (m_writers.empty()) {
        const unsigned threads = std::clamp(std::thread::hardware_concurrency() / 2, 1u, 4u);
        for (unsigned n = 0; n < threads; n++) {
            m_writers.emplace_back([this] { Writer(); });
        }
    }
    return "RECORDING SCENE CLIP";
}

void SceneExport::Missed() {
    // (no picture this frame: a frame of the clip fewer; none for as long as the clip was to be, and it ends)
    if (m_clip.on && ++m_clip.missed >= m_clip.wanted) {
        StopClip();
    }
}

void SceneExport::Frame(Scene &&scene) {
    if (!m_clip.on) {
        return;
    }
    Job job;
    job.scene = std::move(scene);
    const Scene &s = job.scene;
    std::string order;
    for (const SceneLayer &layer : s.layers) {
        if (std::find(m_clip.names.begin(), m_clip.names.end(), layer.name) == m_clip.names.end()) {
            m_clip.names.push_back(layer.name);
            m_clip.json += std::string(m_clip.json.empty() ? "" : "\x1F") + LayerJson(layer, false);
        }
        if (!layer.empty) {
            order += std::string(order.empty() ? "" : ", ") + Quoted(layer.name);
        }
    }
    std::string overlays;
    for (const SceneLayer &overlay : s.overlays) {
        if (std::find(m_clip.overlayNames.begin(), m_clip.overlayNames.end(), overlay.name) == m_clip.overlayNames.end()) {
            m_clip.overlayNames.push_back(overlay.name);
            m_clip.overlaysJson += std::string(m_clip.overlaysJson.empty() ? "" : "\x1F") + LayerJson(overlay, false);
        }
        if (!overlay.empty) {
            overlays += std::string(overlays.empty() ? "" : ", ") + Quoted(overlay.name);
        }
    }
    m_clip.taken++;
    job.folder = m_clip.folder;
    job.number = m_clip.taken;
    m_frameJson += std::string(m_frameJson.empty() ? "" : ",\n") + "    {\"number\": " + std::to_string(job.number) + ", \"frame\": " +
                   std::to_string(s.frame) + ", \"backdrop\": " + Quoted(Hex(s.backdrop)) + ", \"layers\": [" + order + "]" +
                   (s.overlays.empty() ? "" : ", \"overlays\": [" + overlays + "], \"boxes\": " + BoxesJson(s)) +
                   ", \"entries\": " + std::to_string(s.entries.size()) + "}";
    m_clip.entries = m_clip.entries || !s.entries.empty();
    m_width = s.width;
    m_height = s.height;
    m_pixelAspect = s.pixelAspect;
    bool full;
    {
        std::lock_guard<std::mutex> lock(m_lock);
        m_jobs.push_back(std::move(job));
        full = static_cast<int>(m_jobs.size()) >= kMostWaiting;
    }
    m_wake.notify_one();
    if (m_clip.taken >= m_clip.wanted || full) {
        StopClip(full && m_clip.taken < m_clip.wanted);
    }
}

void SceneExport::StopClip(bool cutShort) {
    if (!m_clip.on) {
        return;
    }
    m_clip.on = false;
    // scene.json, written when the last frame is
    auto list = [](const std::string &joined) {
        std::string out;
        for (size_t at = 0; at <= joined.size();) {
            const size_t end = std::min(joined.find('\x1F', at), joined.size());
            out += "    " + joined.substr(at, end - at) + (end < joined.size() ? ",\n" : "\n");
            at = end + 1;
        }
        return out;
    };
    const std::string overlays = m_clip.overlaysJson.empty() ? "" : ",\n  \"overlays\": [\n" + list(m_clip.overlaysJson) + "  ]";
    std::string json = "{\n  \"kind\": \"clip\",\n  \"width\": " + std::to_string(m_width) + ",\n  \"height\": " + std::to_string(m_height) +
                       ",\n  \"pixelAspect\": " + Number(m_pixelAspect) + ",\n  \"fps\": " + Number(m_clip.fps) + ",\n  \"frames\": " +
                       std::to_string(m_clip.taken) + ",\n  \"picture\": \"picture\",\n  \"layers\": [\n" +
                       (m_clip.taken ? list(m_clip.json) : "") + "  ]" + overlays +
                       (m_clip.entries ? ",\n  \"entryMap\": \"entry-map\",\n  \"entries\": \"entries\"" : "") + ",\n  \"frameList\": [\n" +
                       m_frameJson + "\n  ]\n}\n";
    m_frameJson.clear();
    std::lock_guard<std::mutex> lock(m_lock);
    m_finalJson = std::move(json);
    m_finalFolder = m_clip.folder;
    m_finalFrames = m_clip.taken;
    m_cutShort = cutShort;
    m_finishing = true;
    if (m_jobs.empty() && m_writing == 0) {
        Finish();
    }
}

// (with the lock held: the clip's frames all written)
void SceneExport::Finish() {
    m_finishing = false;
    const bool written = WriteAll(m_finalFolder + "scene.json", m_finalJson);
    std::string folder = m_finalFolder;
    if (const size_t at = folder.find("exports/"); at != std::string::npos) {
        folder = folder.substr(at);
    }
    if (m_failed || !written) {
        m_said = "CLIP NOT SAVED";
    } else if (m_finalFrames == 0) {
        m_said = "NO SCENE TO SAVE YET";
    } else {
        m_said = std::string(m_cutShort ? "CLIP CUT SHORT  " : "CLIP SAVED  ") + folder;
    }
    m_saying = true;
    fprintf(stderr, "scene clip: %s (%d frames)\n", m_finalFolder.c_str(), m_finalFrames);
}

void SceneExport::Writer() {
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(m_lock);
            m_wake.wait(lock, [this] { return m_stopping || !m_jobs.empty(); });
            if (m_jobs.empty()) {
                return;
            }
            job = std::move(m_jobs.front());
            m_jobs.pop_front();
            m_writing++;
        }
        const bool written = WriteScene(job.scene, job.folder, job.number);
        std::lock_guard<std::mutex> lock(m_lock);
        m_writing--;
        m_failed = m_failed || !written;
        if (m_finishing && m_jobs.empty() && m_writing == 0) {
            Finish();
        }
    }
}

bool SceneExport::TakeSaid(std::string &said) {
    std::lock_guard<std::mutex> lock(m_lock);
    if (!m_saying) {
        return false;
    }
    m_saying = false;
    said = m_said;
    return true;
}

} // namespace port
