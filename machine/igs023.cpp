// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// As MAME's igs023_video.cpp has the chip (BSD-3-Clause, David Haywood): see igs023.h.
#include "igs023.h"

#include "../runtime/state.h"

#include <cstring>

namespace spelunk {

namespace {
    // What of each of a sprite's five words the DMA keeps
    constexpr uint16_t kListMask[5] = {0xFFFF, 0xFBFF, 0x7FFF, 0xFFFF, 0xFFFF};

    uint32_t Colour(uint16_t c) {
        const uint32_t r = (c >> 10) & 31, g = (c >> 5) & 31, b = c & 31;
        return 0xFF000000u | ((b << 3 | b >> 2) << 16) | ((g << 3 | g >> 2) << 8) | (r << 3 | r >> 2);
    }
} // namespace

uint16_t Igs023::ReadVideoRam(uint32_t low) const {
    low &= 0x7FFE;
    if (low < 0x4000) {
        return bgRam[(low & 0xFFF) >> 1];
    }
    if (low < 0x7000) {
        return textRam[(low & 0x1FFF) >> 1];
    }
    return rowScroll[(low & 0xFFF) >> 1];
}

void Igs023::WriteVideoRam(uint32_t low, uint16_t value, uint16_t mask) {
    low &= 0x7FFE;
    uint16_t &word = low < 0x4000 ? bgRam[(low & 0xFFF) >> 1] : low < 0x7000 ? textRam[(low & 0x1FFF) >> 1] : rowScroll[(low & 0xFFF) >> 1];
    word = static_cast<uint16_t>((word & ~mask) | (value & mask));
}

uint16_t Igs023::ReadReg(uint32_t low, uint16_t line) const {
    low &= 0xFFFE;
    if (low < 0x1000) {
        return spriteBuffer[low >> 1];
    }
    switch (low) {
    case 0x2000: return bgY;
    case 0x3000: return bgX;
    case 0x4000: return bgScale;
    case 0x5000: return textY;
    case 0x6000: return textX;
    case 0x7000: return line;
    case 0xE000: return ctrl;
    default: return 0;
    }
}

bool Igs023::WriteReg(uint32_t low, uint16_t value, uint16_t mask) {
    low &= 0xFFFE;
    auto combine = [&](uint16_t &reg) { reg = static_cast<uint16_t>((reg & ~mask) | (value & mask)); };
    if (low < 0x1000) {
        combine(spriteBuffer[low >> 1]);
    } else if (low < 0x1040) {
        combine(zoomRam[(low - 0x1000) >> 1]);
    } else {
        switch (low) {
        case 0x2000: combine(bgY); break;
        case 0x3000: combine(bgX); break;
        case 0x4000: combine(bgScale); break;
        case 0x5000: combine(textY); break;
        case 0x6000: combine(textX); break;
        case 0xE000: combine(ctrl); return false;
        default: break;
        }
    }
    return true;
}

unsigned Igs023::CopyList(const uint16_t *ram) {
    unsigned words = 0;
    for (int i = 0; i < 256; i++) {
        for (int part = 0; part < 5; part++) {
            spriteBuffer[i * 8 + part] = ram[words++] & kListMask[part];
        }
        if ((spriteBuffer[i * 8 + 4] & 0x7FFF) == 0) {
            break;
        }
    }
    return words;
}

void Igs023::LatchSprites() {
    shown.swap(next);
    next.clear();
    for (int n = 0; n < 0x800; n += 8) {
        const uint16_t *w = &spriteBuffer[n];
        if ((w[4] & 0x7FFF) == 0) {
            break; // (as on the board)
        }
        Sprite s{};
        const bool xgrow = w[0] & 0x8000, ygrow = w[1] & 0x8000;
        int xzom = (w[0] & 0x7800) >> 11, yzom = (w[1] & 0x7800) >> 11;
        s.x = static_cast<int16_t>(w[0] << 5) >> 5;
        s.y = static_cast<int16_t>(w[1] << 6) >> 6;
        s.flip = static_cast<uint8_t>((w[2] & 0x6000) >> 13);
        s.color = (w[2] & 0x1F00) >> 8;
        s.pri = (w[2] >> 7) & 1;
        s.offs = (static_cast<uint32_t>(w[2] & 0x7F) << 16) | w[3];
        s.width = (w[4] & 0x7E00) >> 9;
        s.height = w[4] & 0x1FF;
        if (xgrow) {
            xzom = 0x10 - xzom;
        }
        if (ygrow) {
            yzom = 0x10 - yzom;
        }
        // (DoDonPachi Dai-Ou-Jou leaves the last table at 0 and wants 1)
        auto zoom = [&](int z) -> uint32_t {
            return z < 0x10 ? (z == 0xF ? 1u : (static_cast<uint32_t>(zoomRam[z * 2]) << 16) | zoomRam[z * 2 + 1]) : 0u;
        };
        s.xzoom = zoom(xzom);
        s.yzoom = zoom(yzom);
        s.xgrow = xgrow;
        s.ygrow = ygrow;
        next.push_back(s);
    }
}

// ---- The sprites: drawn in the order their data comes from the ROMs (the pixels are packed: only those drawn are
// there), the last of the list first; a pixel a sprite has drawn is not drawn again ----

uint8_t Igs023::SpritePixel() {
    const uint8_t value = (spritePixels[m_aoffset & (spritePixels.size() - 1)] >> m_abit) & 0x1F;
    m_abit += 5;
    if (m_abit >= 15) {
        m_aoffset++;
        m_abit = 0;
    }
    return value;
}

void Igs023::DrawPixel(int x, int pri, uint16_t *dest, uint8_t *destPri, uint16_t value) {
    if (x < 0 || x >= kWidth) {
        return;
    }
    if (m_noted && !(destPri[x] & 1)) {
        const size_t i = static_cast<size_t>(dest - m_indices.data()) + static_cast<size_t>(x);
        m_noted->sprites[i] = value;
        m_noted->spriteBehind[i] = pri != 0;
        m_noted->spriteEntry[i] = m_entry;
    }
    if (!(destPri[x] & 1) && (!pri || !(destPri[x] & 2))) {
        dest[x] = value;
    }
    destPri[x] |= 1;
}

void Igs023::SpriteLine(int wide, uint16_t *dest, uint8_t *destPri, uint32_t xzoom, bool xgrow, int flip, int xpos, int pri, int realXSize,
                        int palette, bool draw, bool zoomed) {
    int xoffset = 0, xcntdraw = 0;
    for (int xcnt = 0; xcnt < wide; xcnt++) {
        uint16_t mask = spriteMasks[m_boffset & (spriteMasks.size() - 1)];
        for (int x = 0; x < 16; x++) {
            const bool xzoombit = zoomed && ((xzoom >> (xoffset & 0x1F)) & 1);
            xoffset++;
            if (!(mask & 1)) {
                const uint16_t value = static_cast<uint16_t>(SpritePixel() + palette * 32);
                if (draw && (xgrow || !xzoombit)) {
                    for (int i = 0; i < (xzoombit ? 2 : 1); i++) {
                        const int at = (flip & 1) ? xpos + realXSize - xcntdraw : xpos + xcntdraw;
                        DrawPixel(at, pri, dest, destPri, value);
                        xcntdraw++;
                    }
                }
            } else if (xgrow || !xzoombit) {
                xcntdraw += xzoombit ? 2 : 1;
            }
            mask >>= 1;
        }
        m_boffset++;
    }
}

void Igs023::DrawSprite(const Sprite &s) {
    const int wide = static_cast<int>(s.width), high = static_cast<int>(s.height), flip = s.flip;
    const bool zoomed = s.xzoom || s.yzoom;
    m_boffset = s.offs;
    const size_t bmask = spriteMasks.size() - 1;
    m_aoffset = ((static_cast<uint32_t>(spriteMasks[(m_boffset + 1) & bmask]) << 16) | spriteMasks[m_boffset & bmask]) >> 2;
    m_abit = 0;
    m_boffset += 2;
    // Where the drawing ends (flipped sprites are drawn from their far end: the data comes in the ROM's order)
    int realYSize = 0, realXSize = 0;
    for (int y = 0; y < high; y++) {
        const bool bit = zoomed && ((s.yzoom >> (y & 0x1F)) & 1);
        if (s.ygrow || !bit) {
            realYSize += bit ? 2 : 1;
        }
    }
    realYSize--;
    for (int x = 0; x < wide * 16; x++) {
        const bool bit = zoomed && ((s.xzoom >> (x & 0x1F)) & 1);
        if (s.xgrow || !bit) {
            realXSize += bit ? 2 : 1;
        }
    }
    realXSize--;
    int ycntdraw = 0;
    // A line drawn where it lands; false: it fell off the screen on the side the sprite goes on towards (the rest
    // need not be drawn)
    auto line = [&]() {
        const int y = (flip & 2) ? s.y + realYSize - ycntdraw : s.y + ycntdraw;
        if (y >= 0 && y < kHeight) {
            SpriteLine(wide, &m_indices[static_cast<size_t>(y) * kWidth], &m_priority[static_cast<size_t>(y) * kWidth], s.xzoom, s.xgrow, flip,
                       s.x, s.pri, realXSize, static_cast<int>(s.color), true, zoomed);
            return true;
        }
        SpriteLine(wide, nullptr, nullptr, s.xzoom, s.xgrow, flip, s.x, s.pri, realXSize, static_cast<int>(s.color), false, zoomed);
        return (flip & 2) ? y >= 0 : y < kHeight - 1;
    };
    for (int ycnt = 0; ycnt < high; ycnt++) {
        const bool bit = zoomed && ((s.yzoom >> (ycnt & 0x1F)) & 1);
        if (bit && s.ygrow) {
            // the line twice
            const uint32_t a = m_aoffset, b = m_boffset;
            const uint8_t abit = m_abit;
            const int y = (flip & 2) ? s.y + realYSize - ycntdraw : s.y + ycntdraw;
            if (y >= 0 && y < kHeight) {
                SpriteLine(wide, &m_indices[static_cast<size_t>(y) * kWidth], &m_priority[static_cast<size_t>(y) * kWidth], s.xzoom, s.xgrow, flip,
                           s.x, s.pri, realXSize, static_cast<int>(s.color), true, zoomed);
            } else {
                SpriteLine(wide, nullptr, nullptr, s.xzoom, s.xgrow, flip, s.x, s.pri, realXSize, static_cast<int>(s.color), false, zoomed);
            }
            ycntdraw++;
            m_aoffset = a;
            m_boffset = b;
            m_abit = abit;
            if (!line()) {
                return;
            }
            ycntdraw++;
        } else if (bit) {
            // the line skipped
            SpriteLine(wide, nullptr, nullptr, s.xzoom, s.xgrow, flip, s.x, s.pri, realXSize, static_cast<int>(s.color), false, zoomed);
        } else {
            if (!line()) {
                return;
            }
            ycntdraw++;
        }
    }
}

void Igs023::Draw(uint32_t *out) {
    m_indices.assign(static_cast<size_t>(kWidth) * kHeight, 0x3FF);
    m_priority.assign(m_indices.size(), 0);
    // The background (5 bits a pixel, 31 clear), its scroll of each line
    if (!(ctrl & 0x1000)) {
        const size_t tileCount = tiles.size() / 640;
        for (int y = 0; y < kHeight; y++) {
            const uint32_t ty = (y + bgY) & 511;
            const uint32_t sx = bgX + rowScroll[y];
            for (int x = 0; x < kWidth; x++) {
                const uint32_t tx = (x + sx) & 2047;
                const uint32_t at = ((ty >> 5) * 64 + (tx >> 5)) * 2;
                const uint32_t code = bgRam[at] % tileCount, attr = bgRam[at + 1];
                uint32_t px = tx & 31, py = ty & 31;
                if (attr & 0x40) {
                    px = 31 - px;
                }
                if (attr & 0x80) {
                    py = 31 - py;
                }
                // (the five bits of a pixel are in a row, the first the lowest, the bytes read lowest bit first)
                const uint32_t bit = static_cast<uint32_t>(code * 5120 + py * 160 + px * 5);
                const uint32_t pair = tiles[bit >> 3] | (((bit >> 3) + 1 < tiles.size() ? tiles[(bit >> 3) + 1] : 0) << 8);
                const uint32_t pixel = (pair >> (bit & 7)) & 31;
                if (pixel != 31) {
                    const size_t i = static_cast<size_t>(y) * kWidth + x;
                    m_indices[i] = static_cast<uint16_t>(0x400 + ((attr & 0x3E) >> 1) * 32 + pixel);
                    m_priority[i] = 2;
                    if (m_noted) {
                        m_noted->background[i] = m_indices[i];
                    }
                }
            }
        }
    }
    // The sprites, the last in the list first
    if (!spritePixels.empty() && !spriteMasks.empty()) {
        for (size_t n = shown.size(); n-- > 0;) {
            if ((ctrl & 0x2000) && !shown[n].pri) {
                continue;
            }
            m_entry = static_cast<int32_t>(n);
            DrawSprite(shown[n]);
        }
    }
    // The text (4 bits a pixel, 15 clear)
    if (!(ctrl & 0x800)) {
        const size_t tileCount = tiles.size() / 32;
        for (int y = 0; y < kHeight; y++) {
            const uint32_t ty = (y + textY) & 255;
            for (int x = 0; x < kWidth; x++) {
                const uint32_t tx = (x + textX) & 511;
                const uint32_t at = ((ty >> 3) * 64 + (tx >> 3)) * 2;
                const uint32_t code = textRam[at] % tileCount, attr = textRam[at + 1];
                uint32_t px = tx & 7, py = ty & 7;
                if (attr & 0x40) {
                    px = 7 - px;
                }
                if (attr & 0x80) {
                    py = 7 - py;
                }
                const uint32_t pixel = (tiles[code * 32 + py * 4 + (px >> 1)] >> ((px & 1) * 4)) & 15;
                if (pixel != 15) {
                    m_indices[static_cast<size_t>(y) * kWidth + x] = static_cast<uint16_t>(0x800 + ((attr & 0x3E) >> 1) * 16 + pixel);
                    if (m_noted) {
                        m_noted->text[static_cast<size_t>(y) * kWidth + x] = m_indices[static_cast<size_t>(y) * kWidth + x];
                    }
                }
            }
        }
    }
    for (size_t i = 0; i < m_indices.size(); i++) {
        out[i] = Colour(paletteRam[m_indices[i] % 0xA00]);
    }
}

void Igs023::Colours(const Noted &noted, Split &out) const {
    auto colours = [&](const std::vector<uint16_t> &from, std::vector<uint32_t> &to) {
        to.assign(from.size(), 0);
        for (size_t i = 0; i < from.size(); i++) {
            if (from[i] != 0xFFFF) {
                to[i] = Colour(paletteRam[from[i] % 0xA00]) | 0xFF000000u;
            }
        }
    };
    colours(noted.background, out.background);
    colours(noted.sprites, out.sprites);
    colours(noted.text, out.text);
    out.spriteBehind = noted.spriteBehind;
    out.spriteEntry = noted.spriteEntry;
    out.backdrop = Colour(paletteRam[0x3FF % 0xA00]) & 0xFFFFFFu;
}

void Igs023::SplitFrame(Split &out) {
    const size_t size = static_cast<size_t>(kWidth) * kHeight;
    Noted noted{std::vector<uint16_t>(size, 0xFFFF), std::vector<uint16_t>(size, 0xFFFF), std::vector<uint16_t>(size, 0xFFFF),
                std::vector<uint8_t>(size, 0), std::vector<int32_t>(size, -1)};
    std::vector<uint32_t> picture(size);
    m_noted = &noted;
    Draw(picture.data());
    m_noted = nullptr;
    m_entry = -1;
    Colours(noted, out);
}

bool Igs023::SpriteAlone(size_t which, int &x, int &y, int &w, int &h, std::vector<uint32_t> &pixels) {
    if (which >= shown.size() || spritePixels.empty() || spriteMasks.empty()) {
        return false;
    }
    const size_t size = static_cast<size_t>(kWidth) * kHeight;
    Noted noted{{}, std::vector<uint16_t>(size, 0xFFFF), {}, std::vector<uint8_t>(size, 0), std::vector<int32_t>(size, -1)};
    m_indices.assign(size, 0x3FF);
    m_priority.assign(size, 0);
    m_noted = &noted;
    m_entry = 0;
    DrawSprite(shown[which]);
    m_noted = nullptr;
    m_entry = -1;
    int x0 = kWidth, y0 = kHeight, x1 = -1, y1 = -1;
    for (int py = 0; py < kHeight; py++) {
        for (int px = 0; px < kWidth; px++) {
            if (noted.spriteEntry[static_cast<size_t>(py) * kWidth + px] >= 0) {
                x0 = std::min(x0, px);
                x1 = std::max(x1, px);
                y0 = std::min(y0, py);
                y1 = std::max(y1, py);
            }
        }
    }
    if (x1 < 0) {
        return false;
    }
    x = x0;
    y = y0;
    w = x1 - x0 + 1;
    h = y1 - y0 + 1;
    pixels.assign(static_cast<size_t>(w) * h, 0);
    for (int py = y0; py <= y1; py++) {
        for (int px = x0; px <= x1; px++) {
            const size_t i = static_cast<size_t>(py) * kWidth + px;
            if (noted.spriteEntry[i] >= 0) {
                pixels[static_cast<size_t>(py - y0) * w + px - x0] = Colour(paletteRam[noted.sprites[i] % 0xA00]) | 0xFF000000u;
            }
        }
    }
    return true;
}

void Igs023::Serialize(Archive &ar) {
    ar.Mark(0x49473233);
    ar.Pod(bgRam);
    ar.Pod(textRam);
    ar.Pod(rowScroll);
    ar.Pod(paletteRam);
    ar.Pod(spriteBuffer);
    ar.Pod(zoomRam);
    ar.Pod(bgY);
    ar.Pod(bgX);
    ar.Pod(bgScale);
    ar.Pod(textY);
    ar.Pod(textX);
    ar.Pod(ctrl);
    ar.Vec(shown);
    ar.Vec(next);
}

} // namespace spelunk
