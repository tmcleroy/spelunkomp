// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The board with no window: runs a game for a number of frames with scripted input, and writes screenshots, RAM
// dumps, per-frame hashes and recordings of the code run (the checks and the translator's input).
//
//   --game games/<id>/game.cfg   --roms FOLDER   --frames N
//   --cpu interpreter|blocks|native    how the game's code is run (native: the translations linked in)
//   --shot FRAME:file.png              the picture after that frame, stood up as the player sees it (--raw-shots: as
//                                      the board draws it, 320 x 240)
//   --dump-ram FRAME:file              main RAM after that frame (and the EEPROM in file.eeprom)
//   --press FRAME:COUNT:BITS[:PLAYER]  a player's inputs (machine/cv1000.h's kIn bits) held for COUNT frames
//   --system FRAME:COUNT:BITS          the coin, start and service switches (kSys bits)
//   --poke FROM:ADDRESS:VALUE[:BYTES]  a value written into main RAM before every frame from FROM on (finding what a
//                                      place in memory does); BYTES 1, 2 or 4 (4 by default)
//   --set-reg ADDRESS:REG:VALUE        a register given a value each time the code at ADDRESS (a routine's start) is
//                                      reached (trying what an argument does)
//   --timing                           the board's slowdown: the CPU's cache and bus timing (machine/bus_timing.h);
//                                      the run's last line then says how many frames of the screen the game
//                                      started no draw list in (frames it was late for)
//   --hash-log FILE                    a line per frame: the frame and a hash of what the game can see
//   --record FILE [--record-static N]  the blocks of code run (and N steps of those they lead to), for tools/translate.py
//   --wav FILE                         the sound, as the chip makes it
//   --save-state FRAME:file   --load-state file
//   --save-on-arrival FILE             (with --features: a game run unseen to where a mode begins, to_boss and the
//                                      rest) the state saved after the first frame shown once the game is there, its
//                                      frame said ("arrival: frame N"), and the run ended (tools/shadow/: the boss
//                                      states, made from the ROM set alone)
//   --features NAME[=N],...            the game's features (games/<id>/src: free play, the training features) as the
//                                      game program's --features has them; off unless named. Without it the board
//                                      alone runs, as the checks run it; `--features list`: each one's name and
//                                      number of values
//   --auto SEED[:FROM[:2]]             made-up play from frame FROM (400): coins, starts, the stick and buttons
//                                      (:2 both players)
//   SPELUNK_FRAME_DUMP=FILE[:DIV]      (measurement) every frame's picture, small and grey (port/frame_dump.h:
//                                      lining the port up against a board's video, tools/boardvideo)
//   SPELUNK_LATE_LOG=FILE              (with --timing) a line a frame: the frame, 1 if the game started no draw list
//   SPELUNK_ROUTE=FILE                 (measurement) a player's route by the game's frames (refreshes the game started a
//                                      draw list in, counted from the run's start): lines `G BITS [ADDRESS:VALUE:BYTES]...`,
//                                      player 1's inputs and RAM pokes from game frame G on, until the next line
//                                      (tools/boardvideo/route.py: a board video's ship replayed)
#include "../machine/cv1000.h"
#include "../runtime/blocks.h"
#include "../runtime/game.h"
#include "../runtime/romset.h"
#include "../runtime/state.h"
#include "../machine/zip.h"
#include "frame_dump.h"
#include "png_write.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>

using namespace spelunk;

namespace {
struct Held {
    uint64_t from, count;
    uint32_t bits;
    int player; // (-1: the system switches)
};

// The picture turned `degrees` clockwise (0, 90, 180 or 270)
std::vector<uint32_t> Turned(const uint32_t *in, int w, int h, int degrees, int &outW, int &outH) {
    outW = degrees % 180 ? h : w;
    outH = degrees % 180 ? w : h;
    std::vector<uint32_t> out(static_cast<size_t>(outW) * outH);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int ox = x, oy = y;
            switch (degrees) {
            case 90: ox = h - 1 - y; oy = x; break;
            case 180: ox = w - 1 - x; oy = h - 1 - y; break;
            case 270: ox = y; oy = w - 1 - x; break;
            default: break;
            }
            out[static_cast<size_t>(oy) * outW + ox] = in[static_cast<size_t>(y) * w + x];
        }
    }
    return out;
}
} // namespace

int main(int argc, char **argv) {
    std::string configPath, romFolder = "rom", cpuMode = "native", hashLog, recordPath, loadState, wavPath, arrivalState;
    bool wasUnseen = false;
    std::vector<int16_t> wav;
    uint32_t wavRate = 0;
    uint64_t frames = 600;
    std::map<uint64_t, std::string> shots, dumps, saveStates;
    std::vector<Held> held;
    unsigned recordStatic = 0;
    bool rawShots = false, autoPlay = false;
    uint64_t autoSeed = 0, autoFrom = 400;
    int autoPlayers = 1;
    std::string featureList;
    struct Poke {
        uint64_t from;
        uint32_t address, value, bytes;
    };
    std::vector<Poke> pokes;
    bool timingOn = false;
    uint64_t lateFrames = 0;
    FILE *lateLog = getenv("SPELUNK_LATE_LOG") ? fopen(getenv("SPELUNK_LATE_LOG"), "w") : nullptr;
    struct SetReg {
        uint32_t address, reg, value;
    };
    std::vector<SetReg> setRegs;
    struct RouteStep {
        uint64_t game;
        uint32_t bits;
        std::vector<Poke> pokes;
    };
    std::vector<RouteStep> route;
    if (const char *path = getenv("SPELUNK_ROUTE")) {
        FILE *in = fopen(path, "r");
        if (!in) {
            fprintf(stderr, "cannot read %s\n", path);
            return 1;
        }
        char line[512];
        while (fgets(line, sizeof line, in)) {
            char *at = line;
            RouteStep step{};
            step.game = strtoull(at, &at, 0);
            step.bits = static_cast<uint32_t>(strtoul(at, &at, 0));
            while (*at && *at != '\n') {
                Poke poke{0, 0, 0, 4};
                poke.address = static_cast<uint32_t>(strtoul(at, &at, 0));
                if (*at != ':') {
                    break;
                }
                poke.value = static_cast<uint32_t>(strtoul(at + 1, &at, 0));
                if (*at == ':') {
                    poke.bytes = static_cast<uint32_t>(strtoul(at + 1, &at, 0));
                }
                step.pokes.push_back(poke);
            }
            route.push_back(step);
        }
        fclose(in);
    }
    size_t routeAt = 0;
    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        auto frameAnd = [&](std::map<uint64_t, std::string> &into) {
            const std::string v = next();
            const size_t colon = v.find(':');
            into[strtoull(v.substr(0, colon).c_str(), nullptr, 0)] = v.substr(colon + 1);
        };
        if (a == "--timing") {
            timingOn = true;
        } else if (a == "--set-reg") {
            const std::string v = next();
            char *end = nullptr;
            SetReg r{};
            r.address = static_cast<uint32_t>(strtoul(v.c_str(), &end, 0));
            r.reg = static_cast<uint32_t>(strtoul(*end ? end + 1 : end, &end, 0)) & 15;
            r.value = static_cast<uint32_t>(strtoul(*end ? end + 1 : end, &end, 0));
            setRegs.push_back(r);
        } else if (a == "--poke") {
            const std::string v = next();
            Poke poke{0, 0, 0, 4};
            char *end = nullptr;
            poke.from = strtoull(v.c_str(), &end, 0);
            poke.address = static_cast<uint32_t>(strtoul(*end ? end + 1 : end, &end, 0));
            poke.value = static_cast<uint32_t>(strtoul(*end ? end + 1 : end, &end, 0));
            if (*end) {
                poke.bytes = static_cast<uint32_t>(strtoul(end + 1, nullptr, 0));
            }
            pokes.push_back(poke);
        } else if (a == "--game") {
            configPath = next();
        } else if (a == "--roms") {
            romFolder = next();
        } else if (a == "--frames") {
            frames = strtoull(next().c_str(), nullptr, 0);
        } else if (a == "--cpu") {
            cpuMode = next();
        } else if (a == "--shot") {
            frameAnd(shots);
        } else if (a == "--raw-shots") {
            rawShots = true;
        } else if (a == "--dump-ram") {
            frameAnd(dumps);
        } else if (a == "--save-state") {
            frameAnd(saveStates);
        } else if (a == "--load-state") {
            loadState = next();
        } else if (a == "--save-on-arrival") {
            arrivalState = next();
        } else if (a == "--hash-log") {
            hashLog = next();
        } else if (a == "--wav") {
            wavPath = next();
        } else if (a == "--record") {
            recordPath = next();
        } else if (a == "--record-static") {
            recordStatic = static_cast<unsigned>(strtoul(next().c_str(), nullptr, 0));
        } else if (a == "--features") {
            featureList = next();
        } else if (a == "--auto") {
            const std::string v = next();
            autoPlay = true;
            autoSeed = strtoull(v.c_str(), nullptr, 0);
            if (const size_t colon = v.find(':'); colon != std::string::npos) {
                autoFrom = strtoull(v.c_str() + colon + 1, nullptr, 0);
                if (const size_t again = v.find(':', colon + 1); again != std::string::npos) {
                    autoPlayers = atoi(v.c_str() + again + 1) == 2 ? 2 : 1;
                }
            }
        } else if (a == "--press" || a == "--system") {
            const std::string v = next();
            Held h{0, 1, 0, a == "--system" ? -1 : 0};
            unsigned long long from = 0, count = 1;
            unsigned bits = 0;
            int player = 1;
            const int got = sscanf(v.c_str(), "%llu:%llu:%i:%d", &from, &count, &bits, &player);
            if (got < 3) {
                fprintf(stderr, "%s wants FRAME:COUNT:BITS\n", a.c_str());
                return 2;
            }
            h.from = from;
            h.count = count;
            h.bits = bits;
            if (h.player >= 0) {
                h.player = player == 2 ? 1 : 0;
            }
            held.push_back(h);
        } else {
            fprintf(stderr, "unknown argument %s\n", a.c_str());
            return 2;
        }
    }

    GameConfig config;
    std::string error;
    if (configPath.empty()) {
        ParseGameConfig(BuiltInGameConfig(), config);
    } else if (!ReadGameConfig(configPath, config, error)) {
        fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    auto board = std::make_unique<Cv1000>();
    if (!LoadSet(romFolder, config, *board, error)) {
        fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    const int rotate = atoi(config["rotate"].c_str());

    if (cpuMode == "interpreter") {
        board->cpu.mode = SH3::Mode::Interpreter;
    } else if (cpuMode == "blocks") {
        board->cpu.mode = SH3::Mode::Blocks;
    } else {
        board->cpu.mode = SH3::Mode::Native;
        board->cpu.InstallTranslations();
    }
    // The game's features, when asked for (none: the board alone, as the checks run it)
    std::vector<int> features(AllFeatures().size(), 0);
    const bool withGame = !featureList.empty();
    if (featureList == "list") {
        // (each feature's name and how many values it has: tools/explore.sh reads it)
        for (const recomp::FeatureInfo &info : AllFeatures()) {
            printf("%s %zu\n", info.name.c_str(), info.choices.empty() ? size_t{2} : info.choices.size());
        }
        return 0;
    }
    for (size_t from = 0; withGame && from <= featureList.size();) {
        const size_t comma = featureList.find(',', from);
        std::string name = featureList.substr(from, comma == std::string::npos ? std::string::npos : comma - from);
        int value = 1;
        if (const size_t equals = name.find('='); equals != std::string::npos) {
            value = atoi(name.c_str() + equals + 1);
            name.resize(equals);
        }
        if (const size_t at = FeatureIndex(name); at < features.size()) {
            features[at] = value;
        } else {
            fprintf(stderr, "no feature %s\n", name.c_str());
            return 2;
        }
        if (comma == std::string::npos) {
            break;
        }
        from = comma + 1;
    }
    if (withGame && ThisGame().start) {
        ThisGame().start(*board);
    }
    if (!recordPath.empty()) {
        board->cpu.recording = true;
        board->cpu.recordStatic = recordStatic;
        if (board->cpu.mode == SH3::Mode::Interpreter) {
            board->cpu.mode = SH3::Mode::Blocks;
        }
    }
    if (const char *path = getenv("SPELUNK_INTERPRETED")) {
        (void)path;
        board->cpu.countInterpreted = true;
    }
    if (!loadState.empty()) {
        std::vector<uint8_t> data;
        if (!ReadWholeFile(loadState, data)) {
            fprintf(stderr, "cannot read %s\n", loadState.c_str());
            return 1;
        }
        Archive ar(data, false);
        board->Serialize(ar);
        if (!ar.Ok()) {
            fprintf(stderr, "%s is not a state of this game and build\n", loadState.c_str());
            return 1;
        }
    }

    FILE *hashes = hashLog.empty() ? nullptr : fopen(hashLog.c_str(), "w");
    std::vector<uint32_t> picture(Blitter::kScreenWidth * Blitter::kScreenHeight);
    FrameDump frameDump; // (SPELUNK_FRAME_DUMP: every frame, small and grey; port/frame_dump.h)
    const auto began = std::chrono::steady_clock::now();
    const uint64_t first = board->frame;
    board->cpu.timing = timingOn;
    if (getenv("SPELUNK_TRACE_SOUND")) {
        board->sound.traceFrame = &board->frame;
    }
    for (SetReg &r : setRegs) {
        board->cpu.SetNativeCall(r.address, [](SH3 &cpu, void *context) {
            const SetReg *set = static_cast<const SetReg *>(context);
            cpu.R[set->reg] = set->value;
            return false; // (the routine itself runs)
        }, &r);
    }
    uint64_t random = autoSeed * 0x9E3779B97F4A7C15ull + 0x1234567;
    uint32_t autoStick = 0, autoLeft = 0;
    while (board->frame < frames) {
        const uint64_t f = board->frame;
        board->inputs[0] = board->inputs[1] = board->system = 0;
        for (const Held &h : held) {
            if (f >= h.from && f < h.from + h.count) {
                (h.player < 0 ? board->system : board->inputs[h.player]) |= h.bits;
            }
        }
        if (autoPlay && f >= autoFrom) {
            // (a coin and a start now and then; the stick held a while each way; the shot mostly held, a bomb seldom)
            auto roll = [&]() {
                random ^= random << 13;
                random ^= random >> 7;
                random ^= random << 17;
                return random;
            };
            if (autoLeft-- == 0) {
                static constexpr uint32_t kWays[] = {0, kInUp, kInDown, kInLeft, kInRight, kInUp | kInLeft, kInUp | kInRight, kInDown | kInLeft, kInDown | kInRight};
                autoStick = kWays[roll() % 9];
                autoLeft = 5 + roll() % 40;
            }
            board->inputs[0] |= autoStick | (roll() % 8 ? kInButton1 : 0) | (roll() % 600 == 0 ? kInButton2 : 0) | (roll() % 5 == 0 ? kInButton3 : 0);
            const uint64_t phase = (f - autoFrom) % 240;
            board->system |= phase < 6 ? kSysCoin1 : 0;
            board->system |= phase >= 30 && phase < 34 ? kSysStart1 : 0;
            if (autoPlayers == 2) {
                // (the second player: the first's stick turned about, its own buttons)
                const uint32_t stick = ((autoStick & (kInUp | kInLeft)) << 1) | ((autoStick & (kInDown | kInRight)) >> 1);
                board->inputs[1] |= stick | (roll() % 8 ? kInButton1 : 0) | (roll() % 600 == 0 ? kInButton2 : 0);
                board->system |= phase >= 60 && phase < 66 ? kSysCoin2 : 0;
                board->system |= phase >= 90 && phase < 94 ? kSysStart2 : 0;
            }
        }
        for (const Poke &poke : pokes) {
            if (f >= poke.from) {
                for (uint32_t i = 0; i < poke.bytes; i++) {
                    board->ram[(poke.address + i) & (SH3::kRamSize - 1)] = static_cast<uint8_t>(poke.value >> ((poke.bytes - 1 - i) * 8));
                }
            }
        }
        if (!route.empty()) {
            const uint64_t game = f - first - lateFrames;
            while (routeAt + 1 < route.size() && route[routeAt + 1].game <= game) {
                routeAt++;
            }
            if (route[routeAt].game <= game) {
                board->inputs[0] = route[routeAt].bits;
                for (const Poke &poke : route[routeAt].pokes) {
                    for (uint32_t i = 0; i < poke.bytes; i++) {
                        board->ram[(poke.address + i) & (SH3::kRamSize - 1)] = static_cast<uint8_t>(poke.value >> ((poke.bytes - 1 - i) * 8));
                    }
                }
            }
        }
        if (withGame) {
            // (a game run unseen to where a mode begins: the controls are the game's, as in the game program)
            const int bits = ThisGame().unseen ? ThisGame().unseen(*board, features) : -1;
            if (bits >= 0) {
                board->inputs[0] = static_cast<uint32_t>(bits);
                board->inputs[1] = board->system = 0;
            } else if (wasUnseen && !arrivalState.empty()) {
                // (there: the state as the frame begins, and the run ends)
                std::vector<uint8_t> data;
                Archive ar(data, true);
                board->Serialize(ar);
                if (FILE *out = fopen(arrivalState.c_str(), "wb")) {
                    fwrite(data.data(), 1, data.size(), out);
                    fclose(out);
                }
                fprintf(stderr, "arrival: frame %llu\n", static_cast<unsigned long long>(board->frame));
                break;
            }
            wasUnseen = bits >= 0;
            if (ThisGame().beforeFrame) {
                ThisGame().beforeFrame(*board, features);
            }
        }
        const uint32_t listsBefore = board->blitter.lists;
        board->RunFrame();
        lateFrames += board->blitter.lists == listsBefore;
        if (lateLog) {
            fprintf(lateLog, "%llu %d\n", static_cast<unsigned long long>(board->frame), board->blitter.lists == listsBefore);
        }
        const uint64_t done = board->frame;
        if (withGame && ThisGame().afterFrame) {
            ThisGame().afterFrame(*board, features);
        }
        if (!wavPath.empty()) {
            // (written at the rate the chip settles on; a change of rate mid-run would need resampling here)
            wavRate = board->sound.sampleRate;
            wav.insert(wav.end(), board->sound.out.begin(), board->sound.out.end());
        }
        board->sound.out.clear();
        if (hashes) {
            fprintf(hashes, "%llu %016llx\n", static_cast<unsigned long long>(done), static_cast<unsigned long long>(board->StateHash()));
        }
        if (frameDump.on()) {
            board->Picture(picture.data());
            int w = Blitter::kScreenWidth, h = Blitter::kScreenHeight;
            const std::vector<uint32_t> out = Turned(picture.data(), w, h, rotate, w, h);
            frameDump.Write(out.data(), w, h);
        }
        if (const auto it = shots.find(done); it != shots.end()) {
            board->Picture(picture.data());
            int w = Blitter::kScreenWidth, h = Blitter::kScreenHeight;
            const std::vector<uint32_t> out = Turned(picture.data(), w, h, rawShots ? 0 : rotate, w, h);
            WritePng(it->second.c_str(), out.data(), w, h);
        }
        if (const auto it = dumps.find(done); it != dumps.end()) {
            if (FILE *out = fopen(it->second.c_str(), "wb")) {
                fwrite(board->ram.data(), 1, board->boardD ? 0x1000000 : 0x800000, out);
                fclose(out);
            }
            // (and the EEPROM beside it, its 256 words big-endian as the game reads them: <dump>.eeprom)
            if (FILE *out = fopen((it->second + ".eeprom").c_str(), "wb")) {
                for (const uint16_t word : board->rtc.eeprom) {
                    fputc(word >> 8, out);
                    fputc(word & 0xFF, out);
                }
                fclose(out);
            }
        }
        if (const auto it = saveStates.find(done); it != saveStates.end()) {
            std::vector<uint8_t> data;
            Archive ar(data, true);
            board->Serialize(ar);
            if (FILE *out = fopen(it->second.c_str(), "wb")) {
                fwrite(data.data(), 1, data.size(), out);
                fclose(out);
            }
        }
    }
    if (hashes) {
        fclose(hashes);
    }
    if (!wavPath.empty()) {
        if (FILE *f = fopen(wavPath.c_str(), "wb")) {
            const uint32_t bytes = static_cast<uint32_t>(wav.size() * 2), riff = 36 + bytes, fmt = 16, byteRate = wavRate * 2;
            const uint16_t pcm = 1, channels = 1, align = 2, bits = 16;
            fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fmt, 4, 1, f);
            fwrite(&pcm, 2, 1, f); fwrite(&channels, 2, 1, f); fwrite(&wavRate, 4, 1, f); fwrite(&byteRate, 4, 1, f);
            fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f);
            fwrite(wav.data(), 2, wav.size(), f);
            fclose(f);
        }
        long long peak = 0, nonzero = 0;
        for (int16_t v : wav) { peak = std::max<long long>(peak, v < 0 ? -v : v); nonzero += v != 0; }
        fprintf(stderr, "sound: %zu samples at %u Hz, peak %lld, %lld not silent\n", wav.size(), wavRate, peak, nonzero);
    }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
    const uint64_t ran = board->frame - first;
    fprintf(stderr, "%llu frames of the screen with no draw list started (the game was late, or drew nothing)\n", static_cast<unsigned long long>(lateFrames));
    if (lateLog) {
        fclose(lateLog);
    }
    fprintf(stderr, "%llu frames in %.2f s (%.0f fps); pc %08X; blocks: %llu native, %llu interpreted\n", static_cast<unsigned long long>(ran), seconds,
            seconds > 0 ? ran / seconds : 0.0, board->cpu.PC, static_cast<unsigned long long>(board->cpu.nativeBlocks),
            static_cast<unsigned long long>(board->cpu.interpretedBlocks));
    if (!recordPath.empty()) {
        board->cpu.SaveRecording(recordPath.c_str());
    }
    if (board->cpu.waitSkips) {
        fprintf(stderr, "frame waits (timing off): %llu turns worked out in %llu skips\n",
                static_cast<unsigned long long>(board->cpu.waitTurnsSkipped), static_cast<unsigned long long>(board->cpu.waitSkips));
    }
    if (board->cpu.pollSkips) {
        fprintf(stderr, "poll loops: %llu turns worked out in %llu skips (%llu of them the blitter's wait)\n",
                static_cast<unsigned long long>(board->cpu.pollTurnsSkipped), static_cast<unsigned long long>(board->cpu.pollSkips),
                static_cast<unsigned long long>(board->cpu.pollStatusTurns));
    }
    if (board->cpu.plannedEvents + board->cpu.genericEvents) {
        fprintf(stderr, "planned: %llu events served, %llu generic\n", static_cast<unsigned long long>(board->cpu.plannedEvents),
                static_cast<unsigned long long>(board->cpu.genericEvents));
    }
#ifdef SPELUNK_SHADOW_TIMING
    fprintf(stderr, "shadow: %llu events compared with the reference model, all the same; %llu poll-loop skips (%llu turns) the same as the turns run, "
            "%llu of them worked out all at once and the same turn by turn\n",
            static_cast<unsigned long long>(board->cpu.shadowEvents), static_cast<unsigned long long>(board->cpu.shadowSkips),
            static_cast<unsigned long long>(board->cpu.shadowSkipTurns), static_cast<unsigned long long>(board->cpu.shadowAtOnce));
#endif
#ifdef SPELUNK_TIMING_STATS
    spelunk::g_timingStats.Print(stderr);
    spelunk::g_timingStats.WriteDump();
#endif
    if (const char *path = getenv("SPELUNK_INTERPRETED")) {
        board->cpu.SaveInterpreted(path);
    }
    return 0;
}
