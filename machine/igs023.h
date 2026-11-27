// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The IGS023, the video chip of IGS's PGM board: a background of 32 x 32 tiles of 5 bits a pixel with a scroll for
// every line, a text layer of 8 x 8 tiles of 4 bits over everything, and up to 256 sprites a frame drawn from two
// ROMs (the "A" ROMs hold the pixels, 5 bits each and three to a word, only those that are drawn; the "B" ROMs a mask
// bit for every pixel), each sprite in front of or behind the background, zoomed by tables the game sets.
//
// The list of sprites is taken from work RAM by the chip's own DMA near the end of each frame (the 68000 is held off
// the bus meanwhile) and drawn in the frame after. The palette is 0x1400 bytes of 15-bit colours.
//
// What is where and how it is drawn are as MAME's igs023_video.cpp has them (BSD-3-Clause, David Haywood): see
// THIRD_PARTY_NOTICES.md. This is the reference the GPU's picture is checked against.
#pragma once

#include <cstdint>
#include <vector>

namespace spelunk {

class Archive;

class Igs023 {
public:
    static constexpr int kWidth = 448, kHeight = 224;

    // The ROMs: the background and text tiles (one region, both read from its start), the sprites' pixels (A, as
    // 16-bit words) and masks (B); each sprite ROM's size a power of two (the region's, as the board's address lines
    // wrap)
    std::vector<uint8_t> tiles;
    std::vector<uint16_t> spritePixels, spriteMasks;

    // The chip's memory and registers, as 16-bit words
    uint16_t bgRam[0x800] = {};        // 900000 (64 x 16 tiles: the tile, then its colour and flips)
    uint16_t textRam[0x1000] = {};     // 904000 (64 x 32 tiles)
    uint16_t rowScroll[0x800] = {};    // 907000 (the background's scroll of each line on the screen)
    uint16_t paletteRam[0xA00] = {};   // A00000
    uint16_t spriteBuffer[0x800] = {}; // B00000 (what the DMA copied: 256 sprites of 8 words, 5 used)
    uint16_t zoomRam[0x20] = {};       // B01000
    uint16_t bgY = 0, bgX = 0, bgScale = 0, textY = 0, textX = 0, ctrl = 0;

    // The 68000's view of 900000-907FFF (`low` the offset in it) and of B00000-B0FFFF
    uint16_t ReadVideoRam(uint32_t low) const;
    void WriteVideoRam(uint32_t low, uint16_t value, uint16_t mask);
    uint16_t ReadReg(uint32_t low, uint16_t line) const;
    // (false: the write was to the control register, whose interrupt bits the board looks at)
    bool WriteReg(uint32_t low, uint16_t value, uint16_t mask);

    // The DMA: the list copied from work RAM (`ram`: 16-bit words from its start) into the chip; the words it took
    unsigned CopyList(const uint16_t *ram);
    // At the vertical blank: the list latched at the blank before goes on show, and the copied list is read into the
    // sprites of the next frame
    void LatchSprites();
    // The picture (448 x 224, 32 bits a pixel, red in the low byte) from the layers as they are now and the sprites on show
    void Draw(uint32_t *out);

    // The sprites, decoded: those on show, and those latched for the next frame (both kept with the board's state)
    struct Sprite {
        int32_t x, y;
        uint32_t xzoom, yzoom;
        uint32_t color, offs, width, height;
        uint8_t flip, pri, xgrow, ygrow;
    };
    std::vector<Sprite> shown, next;

    void Serialize(Archive &ar);

    // The picture taken apart (the scene export, port/scene.h), from the layers as they are now and the sprites on
    // show: at each pixel the background's colour, the frontmost sprite's (the first in the list drawn there: the chip
    // draws no later sprite over it, in front of the background or not) with whether it goes behind the background and
    // which sprite it is, and the text's; 0 where there is none (colours red in the low byte, 0xFF in the top)
    struct Split {
        std::vector<uint32_t> background, sprites, text;
        std::vector<uint8_t> spriteBehind;
        std::vector<int32_t> spriteEntry;
        uint32_t backdrop = 0;
    };
    void SplitFrame(Split &out);
    // One of the sprites on show drawn alone, in the box of the screen it covers (false: none of it is on the screen)
    bool SpriteAlone(size_t which, int &x, int &y, int &w, int &h, std::vector<uint32_t> &pixels);

private:
    // (while a split is made: where each pixel's parts are noted, by palette index; 0xFFFF none)
    struct Noted {
        std::vector<uint16_t> background, sprites, text;
        std::vector<uint8_t> spriteBehind;
        std::vector<int32_t> spriteEntry;
    };
    Noted *m_noted = nullptr;
    int32_t m_entry = -1;
    void Colours(const Noted &noted, Split &out) const;
    std::vector<uint16_t> m_indices; // the picture as palette indices
    std::vector<uint8_t> m_priority;
    uint32_t m_aoffset = 0, m_boffset = 0;
    uint8_t m_abit = 0;

    uint8_t SpritePixel();
    void DrawPixel(int x, int pri, uint16_t *dest, uint8_t *destPri, uint16_t value);
    void SpriteLine(int wide, uint16_t *dest, uint8_t *destPri, uint32_t xzoom, bool xgrow, int flip, int xpos, int pri, int realXSize,
                    int palette, bool draw, bool zoomed);
    void DrawSprite(const Sprite &s);
};

} // namespace spelunk
