// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// A Cave 68000 board with no window: runs a game for a number of frames with scripted input, and writes
// screenshots, RAM dumps, per-frame hashes and the places its code reached (the checks and the translator's input).
//
//   --game games/<id>/game.cfg   --roms FOLDER   --frames N
//   --cpu interpreter|blocks|native|check   how the game's code is run (native: the translations linked in; check: a
//                                      build with -DCHECK68K=ON, each translated block against the interpreter)
//   --shot FRAME:file.png              the picture after that frame, stood up as the player sees it (--raw-shots: as
//                                      the board draws it, 320 x 240)
//   --dump-ram FRAME:file              work RAM after that frame
//   --frame-lines N   --blank-lines N  (measurement) frames of N lines of 1,024 cycles (the board's: 272), the bit
//                                      DoDonPachi reads as the blank low for N lines (32): a reference's timing
//   --start-cycle N                    (measurement) the 68000's first instruction N cycles into frame 0 (its reset
//                                      let go then: where a reference's is), the E clock's phase as it would be
//   --ymz-ticks C:O:D:V:E[:Si:Sp:Sf[:K]] (measurement) the YMZ280B's ticks every C CPU cycles from O (in place of
//                                      its crystal's 88,200 a second), a voice's end raising the interrupt D + V x its
//                                      number + the slots of the voices before it (Si idle, Sp playing, Sf fetching)
//                                      as they stand in the tick it is raised in, E nibbles past its end address, a
//                                      key-on taken K + the same slots into a tick (after that: the next tick; before
//                                      the last tick's visit, when that runs past the tick's end: with that tick). The
//                                      FPGA core's chip:
//                                      181.5:-0.5:158:0:2:2.5:5.5:7:157
//   --oki-slots ORIGIN[:DELAY]         (measurement) DonPachi's OKI chips as the FPGA core's (jt6295): a stop reaches
//                                      a voice's status bit at its turn (a quarter of a sample), each chip's clock made
//                                      from 32 MHz as the core's, its clock ORIGIN + 2 x the CPU's cycle; a start's
//                                      bit up DELAY cycles after the command (its phrase's addresses read)
//   --ymz-ticks-until FRAME            (measurement) --ymz-ticks up to that frame, the chip's own crystal after it:
//                                      the shipped timing from a state the same as the core's
//   --extra-irq FRAME:CYCLE            (measurement) the blank's interrupt line raised once more, CYCLE cycles into
//                                      that frame (as a sound interrupt would come there: what it pushes on the stack)
//   --ram-follow FILE:ADDRESS[:BYTES]  (measurement) at each frame's start the bytes at ADDRESS set as another
//                                      --ram-log (a reference's) has them then: e.g. DoDonPachi's random index 101272,
//                                      which its idle loop advances, so that spare time does not part the two runs
//   --ram-log FILE                     work RAM (64 KB, as the 68000 sees it) after every frame, one after another:
//                                      the format of the FPGA core's harness (docs/ACCURACY.md), to compare frame by frame
//   --dump-program FILE                the program as the CPU sees it (what tools/m68k/recomp.py translates)
//   --press FRAME:COUNT:BITS[:PLAYER]  a player's inputs (machine/inputs.h's kIn bits) held for COUNT frames
//   --system FRAME:COUNT:BITS          the coin, start and service switches (kSys bits)
//   --auto SEED[:FROM]                 made-up play from frame FROM (400): coins, starts, the stick and buttons
//   --poke FROM:ADDRESS:VALUE[:BYTES]  a value written into work RAM before every frame from FROM on; BYTES 1, 2 or
//                                      4 (2 by default)
//   --poke-at FRAME:ADDRESS:VALUE[:BYTES]  the same, before that frame only
//   --hash-log FILE                    a line per frame: the frame and a hash of what the game can see
//   --frame-log FILE                   a line a frame: the frame, 1 if the game had a new picture, the idle loop's
//                                      spins (--spin), the board's waits (PGM)
//   --spin ADDRESS                     (hex) the instruction of the game's idle loop counted (--cpu blocks or
//                                      interpreter: translated code is not counted)
//   --record-entries FILE              the places the code reached by computed jumps, added to FILE
//   --wav FILE                         the sound, as the chip makes it
//   (a board with a Z80 for sound: Mazinger Z, Air Gallet)
//   --z80 interpreter|native           how the Z80 is run (native, the default: its translated code, tools/translatez80.sh)
//   --dump-z80 FRAME:file              the Z80's 64 KB as it sees them after that frame (its ROM, the bank on show, its RAM)
//   --record-z80-entries FILE          the places the Z80's code reached by computed jumps, added to FILE
//   --save-state FRAME:file   --load-state file
//   SPELUNK_FRAME_DUMP=FILE[:DIV]      (measurement) every frame's picture, small and grey (port/frame_dump.h:
//                                      lining the port up against a board's video, tools/boardvideo)
//   SPELUNK_CYCLE_TRACE=FROM-TO:file   (--cpu interpreter) each instruction started in those frames: its address
//                                      and the clock then (measurement, against a reference's bus log)
//   SPELUNK_WATCH=lo-hi                (interpreter) who writes to those addresses
//   SPELUNK_TRACE_IRQ=file             (measurement) each change of the interrupt line: frame, cycle in it, level, why
#include "../machine/cave68k.h"
#include "../machine/zip.h"
#include "../runtime/native68k.h"
#include "../runtime/native_z80.h"
#include "../runtime/romset.h"
#include "../runtime/state.h"
#include "frame_dump.h"
#include "png_write.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <set>
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
    std::string configPath, romFolder = "rom", cpuMode = "native", z80Mode = "native", z80EntriesPath, hashLog, entriesPath, loadState, wavPath, programPath;
    std::vector<int16_t> wav;
    uint64_t frames = 600, autoSeed = 0, autoFrom = 400;
    bool autoPlay = false, rawShots = false;
    std::map<uint64_t, std::string> shots, dumps, z80Dumps, saveStates;
    std::vector<Held> held;
    std::string frameLog, ramLog;
    double frameLines = 0, blankLines = 0;
    uint64_t startCycle = 0;
    std::string ymzTicks;
    int64_t okiSlots = -1;
    uint64_t okiStartDelay = 0;
    uint64_t ymzUntil = ~0ull;
    std::vector<uint8_t> follow;
    uint32_t followAt = 0, followBytes = 1;
    uint32_t spinAt = 1;
    uint64_t extraIrqFrame = ~0ull, extraIrqCycle = 0;
    struct Poke {
        uint64_t from;
        uint32_t address, value, bytes;
        bool once;
    };
    std::vector<Poke> pokes;
    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        auto frameAnd = [&](std::map<uint64_t, std::string> &into) {
            const std::string v = next();
            const size_t colon = v.find(':');
            into[strtoull(v.substr(0, colon).c_str(), nullptr, 0)] = v.substr(colon + 1);
        };
        if (a == "--poke" || a == "--poke-at") {
            const std::string v = next();
            Poke poke{0, 0, 0, 2, a == "--poke-at"};
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
        } else if (a == "--dump-z80") {
            frameAnd(z80Dumps);
        } else if (a == "--z80") {
            z80Mode = next();
        } else if (a == "--record-z80-entries") {
            z80EntriesPath = next();
        } else if (a == "--dump-program") {
            programPath = next();
        } else if (a == "--save-state") {
            frameAnd(saveStates);
        } else if (a == "--load-state") {
            loadState = next();
        } else if (a == "--hash-log") {
            hashLog = next();
        } else if (a == "--frame-log") {
            frameLog = next();
        } else if (a == "--ram-log") {
            ramLog = next();
        } else if (a == "--ram-follow") {
            const std::string v = next();
            const size_t colon = v.find(':');
            if (!ReadWholeFile(v.substr(0, colon), follow)) {
                fprintf(stderr, "cannot read %s\n", v.substr(0, colon).c_str());
                return 1;
            }
            char *end = nullptr;
            followAt = static_cast<uint32_t>(strtoul(v.c_str() + colon + 1, &end, 16)) & 0xFFFF;
            if (*end) {
                followBytes = static_cast<uint32_t>(strtoul(end + 1, nullptr, 0));
            }
        } else if (a == "--extra-irq") {
            const std::string v = next();
            unsigned long long f = 0, c = 0;
            if (sscanf(v.c_str(), "%llu:%llu", &f, &c) != 2) {
                fprintf(stderr, "--extra-irq wants FRAME:CYCLE\n");
                return 2;
            }
            extraIrqFrame = f;
            extraIrqCycle = c;
        } else if (a == "--ymz-ticks") {
            ymzTicks = next();
        } else if (a == "--oki-slots") {
            const std::string v = next();
            okiSlots = strtoll(v.c_str(), nullptr, 0);
            if (const size_t c = v.find(':'); c != std::string::npos) {
                okiStartDelay = strtoull(v.c_str() + c + 1, nullptr, 0);
            }
        } else if (a == "--ymz-ticks-until") {
            ymzUntil = strtoull(next().c_str(), nullptr, 0);
        } else if (a == "--start-cycle") {
            startCycle = strtoull(next().c_str(), nullptr, 0);
        } else if (a == "--frame-lines") {
            frameLines = atof(next().c_str());
        } else if (a == "--blank-lines") {
            blankLines = atof(next().c_str());
        } else if (a == "--spin") {
            spinAt = static_cast<uint32_t>(strtoul(next().c_str(), nullptr, 16));
        } else if (a == "--wav") {
            wavPath = next();
        } else if (a == "--record-entries") {
            entriesPath = next();
        } else if (a == "--auto") {
            const std::string v = next();
            autoPlay = true;
            autoSeed = strtoull(v.c_str(), nullptr, 0);
            if (const size_t colon = v.find(':'); colon != std::string::npos) {
                autoFrom = strtoull(v.c_str() + colon + 1, nullptr, 0);
            }
        } else if (a == "--press" || a == "--system") {
            const std::string v = next();
            Held h{0, 1, 0, a == "--system" ? -1 : 0};
            unsigned long long from = 0, count = 1;
            unsigned bits = 0;
            int player = 1;
            if (sscanf(v.c_str(), "%llu:%llu:%i:%d", &from, &count, &bits, &player) < 3) {
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
    auto board = std::make_unique<Cave68k>();
    if (!LoadSet(romFolder, config, *board, error)) {
        fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    if (!programPath.empty()) {
        if (FILE *f = fopen(programPath.c_str(), "wb")) {
            fwrite(board->program.data(), 1, board->program.size(), f);
            fclose(f);
        }
        return 0;
    }
    const int rotate = atoi(config["rotate"].c_str());
    board->mode = cpuMode == "interpreter" ? M68K::Mode::Interpreter : cpuMode == "blocks" ? M68K::Mode::Blocks : cpuMode == "check" ? M68K::Mode::Check
                                                                                                                  : M68K::Mode::Native;
    board->cpu.recording = !entriesPath.empty();
    board->z80.z80Mode = z80Mode == "interpreter" ? Z80::Mode::Interpreter : Z80::Mode::Native;
    board->z80.z80.recording = !z80EntriesPath.empty();
    if (const char *watch = getenv("SPELUNK_WATCH")) {
        unsigned lo = 0, hi = 0;
        if (sscanf(watch, "%x-%x", &lo, &hi) == 2) {
            board->cpu.watchLow = lo;
            board->cpu.watchHigh = hi;
        }
    }
    if (frameLines > 0) {
        board->frameCycles = static_cast<uint64_t>(frameLines * 1024);
    }
    if (blankLines > 0) {
        board->blankCycles = static_cast<uint64_t>(blankLines * 1024);
    }
    board->oki.slotOrigin = okiSlots;
    board->oki.slotStartDelay = okiStartDelay;
    if (!ymzTicks.empty()) {
        // (the YMZ280B timed as the FPGA core's: ticks of CYCLES from OFFSET, a voice's end seen DELAY + PERVOICE x its
        // number later, ENDPAST nibbles past the end address)
        unsigned past = 0;
        sscanf(ymzTicks.c_str(), "%lf:%lf:%lf:%lf:%u:%lf:%lf:%lf:%lf", &board->sound.tickCycles, &board->sound.tickOffset,
               &board->sound.irqDelay, &board->sound.irqDelayPerVoice, &past, &board->sound.slotIdle, &board->sound.slotPlay,
               &board->sound.slotFetch, &board->sound.keyOnAt);
        board->sound.endPast = past;
    }
    if (startCycle > 0) {
        // (the 68000's reset let go later; its E clock as it was, running from power on)
        board->cpu.cycles += startCycle;
        board->cpu.eclockPhase = (board->cpu.eclockPhase + 10 - startCycle % 10) % 10;
    }
    board->cpu.keepHistory = getenv("SPELUNK_PC_HISTORY") != nullptr;
    board->cpu.noteWrites = !ramLog.empty();
    board->cpu.spinAt = spinAt;
    // SPELUNK_TRACE_AT=50E62,5101A (with --cpu blocks: the places need not be ones the translated code stops by at):
    // a line on stdout each time the game's code comes to one of the places, with the frame, the registers and the
    // sixteen longs on top of the stack
    if (const char *places = getenv("SPELUNK_TRACE_AT")) {
        static Cave68k *traced;
        traced = board.get();
        for (const char *at = places; *at;) {
            char *end = nullptr;
            const uint32_t address = static_cast<uint32_t>(strtoul(at, &end, 16));
            AddHook(address, "trace", [](M68K &c) {
                // (SPELUNK_TRACE_FRAMES=file: only in the frames the file lists, one a line)
                static const std::set<uint64_t> frames = [] {
                    std::set<uint64_t> list;
                    if (const char *path = getenv("SPELUNK_TRACE_FRAMES")) {
                        if (FILE *in = fopen(path, "r")) {
                            unsigned long long f = 0;
                            while (fscanf(in, "%llu", &f) == 1) {
                                list.insert(f);
                            }
                            fclose(in);
                        }
                    }
                    return list;
                }();
                if (!frames.empty() && !frames.count(traced->frame)) {
                    return false;
                }
                printf("%llu %06X", static_cast<unsigned long long>(traced->frame), c.pc);
                for (uint32_t r : c.d) {
                    printf(" %08X", r);
                }
                for (uint32_t r : c.a) {
                    printf(" %08X", r);
                }
                for (uint32_t i = 0; i < 16; i++) {
                    printf(" %08X", native::R32(c, c.a[7] + i * 4));
                }
                // (and, for the games whose objects are a5 and a6: the longs at a5 + 4, a5 + 0xC, a6, a6 + 6, where those
                // are in RAM; then the longs at SPELUNK_TRACE_MEM's places)
                for (uint32_t at : {c.a[5] + 4, c.a[5] + 0xC, c.a[6], c.a[6] + 6}) {
                    printf(" %08X", (at & 0xFFFFFF) - c.ramBase < c.ramSize - 4 ? native::R32(c, at) : 0);
                }
                if (const char *mem = getenv("SPELUNK_TRACE_MEM")) {
                    for (const char *m = mem; *m;) {
                        char *end = nullptr;
                        printf(" %08X", native::R32(c, static_cast<uint32_t>(strtoul(m, &end, 16))));
                        m = *end ? end + 1 : end;
                    }
                }
                // (SPELUNK_TRACE_OBJECT=1: after " #", the 0x40 bytes of a5's record and the 0x20 of a6's, in longs, where
                // those are in RAM: a game's object as its task and its body)
                if (getenv("SPELUNK_TRACE_OBJECT")) {
                    printf(" #");
                    for (auto [base, longs] : {std::pair{c.a[5], 16u}, std::pair{c.a[6], 8u}}) {
                        for (uint32_t i = 0; i < longs; i++) {
                            const uint32_t at = base + i * 4;
                            printf(" %08X", (at & 0xFFFFFF) - c.ramBase < c.ramSize - 4 ? native::R32(c, at) : 0);
                        }
                    }
                }
                // (SPELUNK_TRACE_SPRITES=101216: the long there is where the frame's next sprite goes; after " |", the
                // 16-byte entries written since the frame's last traced line, five words of each; with
                // SPELUNK_TRACE_PALETTE=C00000:512 (the palette's place, and the bytes from one sprite colour's entries to the
                // next's) each is followed by its colour's first 16 entries)
                if (const char *where = getenv("SPELUNK_TRACE_SPRITES")) {
                    static uint64_t frame = ~0ull;
                    static uint32_t before = 0;
                    const uint32_t now = native::R32(c, static_cast<uint32_t>(strtoul(where, nullptr, 16)));
                    printf(" |");
                    if (frame == traced->frame && now > before && now - before < 0x4000) {
                        for (uint32_t at = before; at < now; at += 16) {
                            for (uint32_t w = 0; w < 5; w++) {
                                printf(" %04X", native::R16(c, at + w * 2));
                            }
                            if (const char *palette = getenv("SPELUNK_TRACE_PALETTE")) {
                                char *end = nullptr;
                                const uint32_t base = static_cast<uint32_t>(strtoul(palette, &end, 16));
                                const uint32_t step = *end ? static_cast<uint32_t>(strtoul(end + 1, nullptr, 0)) : 512;
                                const uint32_t colour = (native::R16(c, at) >> 8) & 0x3F;
                                for (uint32_t w = 0; w < 16; w++) {
                                    printf(" %04X", native::R16(c, base + colour * step + w * 2));
                                }
                            }
                        }
                    }
                    frame = traced->frame;
                    before = now;
                }
                printf("\n");
                return false;
            });
            at = *end ? end + 1 : end;
        }
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
    FILE *frameOut = frameLog.empty() ? nullptr : fopen(frameLog.c_str(), "w");
    FILE *ramOut = ramLog.empty() ? nullptr : fopen(ramLog.c_str(), "wb");
    uint64_t picturesBefore = board->pictures, spinsBefore = 0;
    std::vector<uint32_t> picture(CaveVideo::kWidth * CaveVideo::kHeight);
    FrameDump frameDump; // (SPELUNK_FRAME_DUMP: every frame, small and grey; port/frame_dump.h)
    const auto began = std::chrono::steady_clock::now();
    const uint64_t first = board->frame;
    if (getenv("SPELUNK_TRACE_SOUND")) {
        board->sound.traceFrame = &board->frame;
    }
    if (const char *path = getenv("SPELUNK_TRACE_IRQ")) {
        board->irqTrace = fopen(path, "w");
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
        }
        for (const Poke &poke : pokes) {
            if (poke.once ? f == poke.from : f >= poke.from) {
                for (uint32_t i = 0; i < poke.bytes; i++) {
                    board->ram[(poke.address + i) & 0xFFFF] = static_cast<uint8_t>(poke.value >> ((poke.bytes - 1 - i) * 8));
                }
            }
        }
        if (f > 0 && (f - 1 + 1) * 0x10000 <= follow.size()) {
            // (the reference's RAM after frame f - 1 is record f - 1)
            for (uint32_t i = 0; i < followBytes; i++) {
                board->ram[(followAt + i) & 0xFFFF] = follow[(f - 1) * 0x10000 + ((followAt + i) & 0xFFFF)];
            }
        }
        static unsigned long long traceFrom = 0, traceTo = 0;
        static FILE *traceOut = [] {
            const char *v = getenv("SPELUNK_CYCLE_TRACE");
            char path[1024] = {};
            return v && sscanf(v, "%llu-%llu:%1023s", &traceFrom, &traceTo, path) == 3 ? fopen(path, "w") : nullptr;
        }();
        if (traceOut) {
            board->cpu.cycleTrace = f >= traceFrom && f <= traceTo ? traceOut : nullptr;
            board->cpu.traceWrites = getenv("SPELUNK_CYCLE_TRACE_WRITES") != nullptr;
            if (board->cpu.cycleTrace) {
                fprintf(traceOut, "frame %llu %llu\n", static_cast<unsigned long long>(f), static_cast<unsigned long long>(board->cpu.cycles));
            }
        }
        if (f == ymzUntil) {
            board->sound.UseCrystal(board->cpu.cycles, Cave68k::kCpuHz); // (measurement: the shipped chip timing from here)
        }
        if (f == extraIrqFrame) {
            board->extraIrqAt = extraIrqCycle;
        }
        board->RunFrame();
        const uint64_t done = board->frame;
        if (!wavPath.empty()) {
            wav.insert(wav.end(), board->SoundOut().begin(), board->SoundOut().end());
        }
        board->SoundOut().clear();
        if (frameOut) {
            // (a line a frame: the frame, 1 if the game had a new picture ready, the idle loop's spins)
            fprintf(frameOut, "%llu %u %llu\n", static_cast<unsigned long long>(done), board->pictures != picturesBefore ? 1u : 0u,
                    static_cast<unsigned long long>(board->cpu.spins - spinsBefore));
            picturesBefore = board->pictures;
            spinsBefore = board->cpu.spins;
        }
        if (ramOut) {
            // (as the bus had it at the blank: an instruction it fell inside has not made its late writes yet)
            const std::vector<uint8_t> ram = board->cpu.RamAt(done * board->frameCycles);
            fwrite(ram.data(), 1, ram.size(), ramOut);
        }
        if (hashes) {
            fprintf(hashes, "%llu %016llx\n", static_cast<unsigned long long>(done), static_cast<unsigned long long>(board->StateHash()));
        }
        if (frameDump.on()) {
            board->Picture(picture.data());
            int w = CaveVideo::kWidth, h = CaveVideo::kHeight;
            const std::vector<uint32_t> out = Turned(picture.data(), w, h, rotate, w, h);
            frameDump.Write(out.data(), w, h);
        }
        if (const auto it = shots.find(done); it != shots.end()) {
            board->Picture(picture.data());
            int w = CaveVideo::kWidth, h = CaveVideo::kHeight;
            const std::vector<uint32_t> out = Turned(picture.data(), w, h, rawShots ? 0 : rotate, w, h);
            WritePng(it->second.c_str(), out.data(), w, h);
        }
        if (const auto it = dumps.find(done); it != dumps.end()) {
            if (FILE *out = fopen(it->second.c_str(), "wb")) {
                fwrite(board->ram.data(), 1, board->ram.size(), out);
                fclose(out);
            }
            // (and the sprite chip's memory beside it: the lists the game wrote)
            if (FILE *out = fopen((it->second + ".sprites").c_str(), "wb")) {
                fwrite(board->video.spriteRam.data(), 1, board->video.spriteRam.size(), out);
                fclose(out);
            }
        }
        if (const auto it = z80Dumps.find(done); it != z80Dumps.end()) {
            std::vector<uint8_t> image;
            board->z80.Image(image);
            if (FILE *out = fopen(it->second.c_str(), "wb")) {
                fwrite(image.data(), 1, image.size(), out);
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
    if (frameOut) {
        fclose(frameOut);
    }
    if (ramOut) {
        fclose(ramOut);
    }
    if (!wavPath.empty()) {
        if (FILE *f = fopen(wavPath.c_str(), "wb")) {
            const uint32_t rate = Ymz280b::sampleRate, bytes = static_cast<uint32_t>(wav.size() * 2), riff = 36 + bytes, fmt = 16, byteRate = rate * 2;
            const uint16_t pcm = 1, channels = 1, align = 2, bits = 16;
            fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fmt, 4, 1, f);
            fwrite(&pcm, 2, 1, f); fwrite(&channels, 2, 1, f); fwrite(&rate, 4, 1, f); fwrite(&byteRate, 4, 1, f);
            fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f);
            fwrite(wav.data(), 2, wav.size(), f);
            fclose(f);
        }
        long long peak = 0, nonzero = 0;
        for (int16_t v : wav) { peak = std::max<long long>(peak, v < 0 ? -v : v); nonzero += v != 0; }
        fprintf(stderr, "sound: %zu samples at %u Hz, peak %lld, %lld not silent\n", wav.size(), Ymz280b::sampleRate, peak, nonzero);
    }
    if (board->map->z80) {
        const Z80 &z80 = board->z80.z80;
        fprintf(stderr, "z80: pc %04X, %llu cycles; %llu commands from the 68000, %llu answers; %llu writes to the FM chip (its loudest sample %d), %llu to the OKIs (%d)\n",
                z80.pc, static_cast<unsigned long long>(z80.cycles), static_cast<unsigned long long>(board->z80.commands),
                static_cast<unsigned long long>(board->z80.answers), static_cast<unsigned long long>(board->z80.fmWrites), board->z80.fmPeak,
                static_cast<unsigned long long>(board->z80.okiWrites), board->z80.okiPeak);
        const uint64_t native = z80.nativeCycles, interpreted = z80.interpretedCycles;
        fprintf(stderr, "z80 code: %s; %llu cycles run translated, %llu interpreted (%.3f%% interpreted)%s\n",
                !HasZ80Code() ? "none translated" : z80.codeDiffers ? "translated, but not what the ROM holds" : "translated, the ROM holds it",
                static_cast<unsigned long long>(native), static_cast<unsigned long long>(interpreted),
                native + interpreted ? 100.0 * interpreted / (native + interpreted) : 0.0, z80Mode == "interpreter" ? " (--z80 interpreter)" : "");
    }
    if (!z80EntriesPath.empty()) {
        std::set<uint32_t> entries(board->z80.z80.newEntries.begin(), board->z80.z80.newEntries.end());
        if (FILE *in = fopen(z80EntriesPath.c_str(), "r")) {
            char line[64];
            while (fgets(line, sizeof line, in)) {
                if (line[0] != '#' && line[0] != '\n') {
                    entries.insert(static_cast<uint32_t>(strtoul(line, nullptr, 16)));
                }
            }
            fclose(in);
        }
        if (FILE *out = fopen(z80EntriesPath.c_str(), "w")) {
            fprintf(out, "# Places the Z80's code was seen to reach in ways the translator cannot follow\n");
            for (uint32_t e : entries) {
                fprintf(out, "%04X\n", e);
            }
            fclose(out);
        }
    }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
    const uint64_t ran = board->frame - first;
    fprintf(stderr, "%llu frames in %.2f s (%.0f fps); %u with a new sprite list; pc %06X; %llu instructions interpreted; %llu blocks checked (%llu not comparable)\n",
            static_cast<unsigned long long>(ran), seconds, seconds > 0 ? ran / seconds : 0.0, board->pictures, board->cpu.pc,
            static_cast<unsigned long long>(board->cpu.interpreted), static_cast<unsigned long long>(BlocksChecked()),
            static_cast<unsigned long long>(BlocksUnsure()));
    if (!entriesPath.empty()) {
        // (added to what the file has)
        std::set<uint32_t> entries(board->cpu.newEntries.begin(), board->cpu.newEntries.end());
        if (FILE *in = fopen(entriesPath.c_str(), "r")) {
            char line[64];
            while (fgets(line, sizeof line, in)) {
                if (line[0] != '#' && line[0] != '\n') {
                    entries.insert(static_cast<uint32_t>(strtoul(line, nullptr, 16)));
                }
            }
            fclose(in);
        }
        if (FILE *out = fopen(entriesPath.c_str(), "w")) {
            fprintf(out, "# Places the game's code was seen to reach in ways the translator cannot follow\n");
            for (uint32_t e : entries) {
                fprintf(out, "%06X\n", e);
            }
            fclose(out);
        }
    }
    return 0;
}
