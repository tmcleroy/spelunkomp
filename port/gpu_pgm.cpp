// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
#include "gpu_pgm.h"

#include "shaders.inc"

#include <algorithm>
#include <bit>
#include <cstdio>
#include <cstring>

namespace spelunk {

namespace {
    constexpr int kW = Igs023::kWidth, kH = Igs023::kHeight;
    constexpr int kRow = 4096;
    constexpr int kVramRows = 42; // (256 words a row: background 8, text 16, line scroll 8, palette 10)

    // The compose shader's uniforms (port/shaders/compose_pgm.frag)
    struct ComposeParams {
        int32_t scroll[4];
        int32_t misc[4];
    };
} // namespace

GpuPgm::~GpuPgm() {
    Shutdown();
}

void GpuPgm::Shutdown() {
    if (!m_device) {
        return;
    }
    SDL_WaitForGPUIdle(m_device);
    for (SDL_GPUTexture *t : {m_tiles, m_pixels, m_masks, m_counts, m_spriteData, m_spriteBuffer, m_vram, m_picture}) {
        if (t) {
            SDL_ReleaseGPUTexture(m_device, t);
        }
    }
    for (SDL_GPUGraphicsPipeline *p : {m_spritePipeline, m_composePipeline}) {
        if (p) {
            SDL_ReleaseGPUGraphicsPipeline(m_device, p);
        }
    }
    m_tiles = m_pixels = m_masks = m_counts = m_spriteData = m_spriteBuffer = m_vram = m_picture = nullptr;
    m_spritePipeline = m_composePipeline = nullptr;
    Close();
}

SDL_GPUTexture *GpuPgm::MakeRom(const void *bytes, size_t size, SDL_GPUTextureFormat format, uint32_t texel) {
    const size_t texels = std::max<size_t>(1, (size + texel - 1) / texel);
    const int rows = static_cast<int>((texels + kRow - 1) / kRow);
    SDL_GPUTexture *texture = MakeTexture(format, kRow, rows, SDL_GPU_TEXTUREUSAGE_SAMPLER);
    if (!texture) {
        return nullptr;
    }
    // (sent a slice at a time through a transfer buffer of its own: the sprite ROMs are tens of megabytes)
    constexpr int kSliceRows = 256;
    SDL_GPUTransferBufferCreateInfo info{};
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    info.size = static_cast<uint32_t>(kSliceRows) * kRow * texel;
    SDL_GPUTransferBuffer *slice = SDL_CreateGPUTransferBuffer(m_device, &info);
    if (!slice) {
        SDL_ReleaseGPUTexture(m_device, texture);
        return nullptr;
    }
    const size_t rowBytes = static_cast<size_t>(kRow) * texel;
    for (int row = 0; row < rows; row += kSliceRows) {
        const int count = std::min(kSliceRows, rows - row);
        uint8_t *to = static_cast<uint8_t *>(SDL_MapGPUTransferBuffer(m_device, slice, true));
        const size_t from = static_cast<size_t>(row) * rowBytes, want = static_cast<size_t>(count) * rowBytes;
        const size_t have = from < size ? std::min(want, size - from) : 0;
        if (have) {
            std::memcpy(to, static_cast<const uint8_t *>(bytes) + from, have);
        }
        std::memset(to + have, 0, want - have);
        SDL_UnmapGPUTransferBuffer(m_device, slice);
        SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(m_device);
        SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
        SDL_GPUTextureTransferInfo source{};
        source.transfer_buffer = slice;
        source.pixels_per_row = kRow;
        source.rows_per_layer = static_cast<Uint32>(count);
        SDL_GPUTextureRegion region{};
        region.texture = texture;
        region.y = static_cast<Uint32>(row);
        region.w = kRow;
        region.h = static_cast<Uint32>(count);
        region.d = 1;
        SDL_UploadToGPUTexture(copy, &source, &region, false);
        SDL_EndGPUCopyPass(copy);
        SDL_SubmitGPUCommandBuffer(cmd);
    }
    SDL_ReleaseGPUTransferBuffer(m_device, slice);
    return texture;
}

bool GpuPgm::Init(SDL_Window *window, const Igs023 &video) {
    if (!Open(window) || video.spritePixels.empty() || video.spriteMasks.empty()) {
        return false;
    }
    // The ROMs: the tiles' bytes, the A and B words, and the count of clear mask bits before each B word
    m_tiles = MakeRom(video.tiles.data(), video.tiles.size(), SDL_GPU_TEXTUREFORMAT_R8_UINT, 1);
    m_pixels = MakeRom(video.spritePixels.data(), video.spritePixels.size() * 2, SDL_GPU_TEXTUREFORMAT_R16_UINT, 2);
    m_masks = MakeRom(video.spriteMasks.data(), video.spriteMasks.size() * 2, SDL_GPU_TEXTUREFORMAT_R16_UINT, 2);
    std::vector<uint32_t> counts(video.spriteMasks.size());
    uint32_t clear = 0;
    for (size_t n = 0; n < counts.size(); n++) {
        counts[n] = clear;
        clear += 16 - static_cast<uint32_t>(std::popcount(video.spriteMasks[n]));
    }
    m_counts = MakeRom(counts.data(), counts.size() * 4, SDL_GPU_TEXTUREFORMAT_R32_UINT, 4);
    m_rom[0] = static_cast<int32_t>(video.spritePixels.size() - 1);
    m_rom[1] = static_cast<int32_t>(video.spriteMasks.size() - 1);
    m_rom[2] = std::countr_zero(video.spriteMasks.size());
    m_rom[3] = static_cast<int32_t>(clear);
    m_bgTiles = std::max<int32_t>(1, static_cast<int32_t>(video.tiles.size() / 640));
    m_textTiles = std::max<int32_t>(1, static_cast<int32_t>(video.tiles.size() / 32));

    const SDL_GPUTextureUsageFlags sampled = SDL_GPU_TEXTUREUSAGE_SAMPLER, target = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    m_spriteData = MakeTexture(SDL_GPU_TEXTUREFORMAT_R32G32B32A32_INT, 1024, 1, sampled);
    m_spriteBuffer = MakeTexture(SDL_GPU_TEXTUREFORMAT_R16_UINT, kW, kH, sampled | target);
    m_vram = MakeTexture(SDL_GPU_TEXTUREFORMAT_R16_UINT, 256, kVramRows, sampled);
    m_picture = MakeTexture(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, kW, kH, sampled | target);

    SDL_GPUShader *spriteVertex = MakeShader(k_sprites_pgm_vert_spirv, sizeof k_sprites_pgm_vert_spirv, k_sprites_pgm_vert_msl, false, 1, 0);
    SDL_GPUShader *sprite = MakeShader(k_sprites_pgm_frag_spirv, sizeof k_sprites_pgm_frag_spirv, k_sprites_pgm_frag_msl, true, 4, 1);
    SDL_GPUShader *compose = MakeShader(k_compose_pgm_frag_spirv, sizeof k_compose_pgm_frag_spirv, k_compose_pgm_frag_msl, true, 3, 1);
    SDL_GPUShader *screen = MakeShader(k_screen_pgm_frag_spirv, sizeof k_screen_pgm_frag_spirv, k_screen_pgm_frag_msl, true, 1, 1);
    bool ok = m_tiles && m_pixels && m_masks && m_counts && m_spriteData && m_spriteBuffer && m_vram && m_picture && spriteVertex && sprite && compose &&
              screen;
    if (ok) {
        m_spritePipeline = MakePipeline(spriteVertex, sprite, SDL_GPU_TEXTUREFORMAT_R16_UINT);
        m_composePipeline = MakePipeline(m_fullVertex, compose, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
        // (the base keeps the screen's shader: the window's pipeline, made again for another window)
        const bool screenOk = SetScreenShader(screen);
        screen = nullptr;
        ok = m_spritePipeline && m_composePipeline && screenOk;
    }
    for (SDL_GPUShader *s : {spriteVertex, sprite, compose, screen}) {
        if (s) {
            SDL_ReleaseGPUShader(m_device, s);
        }
    }
    if (!ok) {
        fprintf(stderr, "gpu: could not be set up: %s\n", SDL_GetError());
        Shutdown();
        return false;
    }
    return true;
}

void GpuPgm::Render(const Igs023 &video) {
    // The sprites on show, four texels each: where (its first pixel, how far it reaches across and down with its
    // zoom), its data (first mask word, first pixel word, look, width in words), its zoom tables
    std::vector<const Igs023::Sprite *> drawn;
    for (const Igs023::Sprite &s : video.shown) {
        if (!(video.ctrl & 0x2000) || s.pri) {
            drawn.push_back(&s);
        }
    }
    const uint32_t sprites = static_cast<uint32_t>(std::min<size_t>(drawn.size(), 256));
    m_spriteTexels.assign(static_cast<size_t>(std::max(sprites, 1u)) * 16, 0);
    const size_t bmask = video.spriteMasks.size() - 1;
    for (uint32_t i = 0; i < sprites; i++) {
        const Igs023::Sprite &s = *drawn[i];
        int32_t *t = &m_spriteTexels[static_cast<size_t>(i) * 16];
        auto reach = [](uint32_t count, uint32_t zoom, bool grow) {
            int32_t size = 0;
            for (uint32_t n = 0; n < count; n++) {
                const bool bit = (zoom >> (n & 31)) & 1;
                if (grow || !bit) {
                    size += bit ? 2 : 1;
                }
            }
            return size - 1;
        };
        t[0] = s.x;
        t[1] = s.y;
        t[2] = reach(s.width * 16, s.xzoom, s.xgrow);
        t[3] = reach(s.height, s.yzoom, s.ygrow);
        // (the sprite's first two mask words say where its pixels begin)
        t[4] = static_cast<int32_t>(s.offs + 2);
        t[5] = static_cast<int32_t>(((static_cast<uint32_t>(video.spriteMasks[(s.offs + 1) & bmask]) << 16) | video.spriteMasks[s.offs & bmask]) >> 2);
        t[6] = static_cast<int32_t>(s.color | (s.flip & 1 ? 0x100 : 0) | (s.flip & 2 ? 0x200 : 0) | (s.pri ? 0x400 : 0) | (s.xgrow ? 0x800 : 0) |
                                    (s.ygrow ? 0x1000 : 0));
        t[7] = static_cast<int32_t>(s.width);
        t[8] = static_cast<int32_t>(s.xzoom);
        t[9] = static_cast<int32_t>(s.yzoom);
    }
    // The chip's memory: the background, the text, the line scroll, the palette
    const uint32_t spriteBytes = static_cast<uint32_t>(m_spriteTexels.size() * 4), vramBytes = 256 * kVramRows * 2;
    uint8_t *data = static_cast<uint8_t *>(MapUpload(spriteBytes + vramBytes));
    std::memcpy(data, m_spriteTexels.data(), spriteBytes);
    uint16_t *vram = reinterpret_cast<uint16_t *>(data + spriteBytes);
    std::memcpy(vram, video.bgRam, sizeof video.bgRam);
    std::memcpy(vram + 8 * 256, video.textRam, sizeof video.textRam);
    std::memcpy(vram + 24 * 256, video.rowScroll, sizeof video.rowScroll);
    std::memcpy(vram + 32 * 256, video.paletteRam, sizeof video.paletteRam);
    SDL_UnmapGPUTransferBuffer(m_device, m_upload);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(m_device);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    UploadRows(copy, m_spriteData, 0, 0, static_cast<int>(m_spriteTexels.size() / 4), 1);
    UploadRows(copy, m_vram, 0, 0, 256, kVramRows, spriteBytes);
    SDL_EndGPUCopyPass(copy);

    // The sprite buffer: cleared, then each sprite in the list's order
    SDL_GPUColorTargetInfo buffer{};
    buffer.texture = m_spriteBuffer;
    buffer.load_op = SDL_GPU_LOADOP_CLEAR;
    buffer.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &buffer, 1, nullptr);
    if (sprites && !video.spritePixels.empty()) {
        SDL_BindGPUGraphicsPipeline(pass, m_spritePipeline);
        const SDL_GPUTextureSamplerBinding vertexData{m_spriteData, m_nearest};
        SDL_BindGPUVertexSamplers(pass, 0, &vertexData, 1);
        const SDL_GPUTextureSamplerBinding fragment[4] = {
            {m_spriteData, m_nearest}, {m_masks, m_nearest}, {m_counts, m_nearest}, {m_pixels, m_nearest}};
        SDL_BindGPUFragmentSamplers(pass, 0, fragment, 4);
        SDL_PushGPUFragmentUniformData(cmd, 0, m_rom, sizeof m_rom);
        SDL_DrawGPUPrimitives(pass, sprites * 6, 1, 0, 0);
    }
    SDL_EndGPURenderPass(pass);
    spritesDrawn = sprites;

    // The picture
    ComposeParams p{};
    p.scroll[0] = video.bgX;
    p.scroll[1] = video.bgY;
    p.scroll[2] = video.textX;
    p.scroll[3] = video.textY;
    p.misc[0] = video.ctrl;
    p.misc[1] = m_bgTiles;
    p.misc[2] = m_textTiles;
    SDL_GPUColorTargetInfo picture{};
    picture.texture = m_picture;
    picture.load_op = SDL_GPU_LOADOP_DONT_CARE;
    picture.store_op = SDL_GPU_STOREOP_STORE;
    pass = SDL_BeginGPURenderPass(cmd, &picture, 1, nullptr);
    SDL_BindGPUGraphicsPipeline(pass, m_composePipeline);
    const SDL_GPUTextureSamplerBinding bindings[3] = {{m_spriteBuffer, m_nearest}, {m_vram, m_nearest}, {m_tiles, m_nearest}};
    SDL_BindGPUFragmentSamplers(pass, 0, bindings, 3);
    SDL_PushGPUFragmentUniformData(cmd, 0, &p, sizeof p);
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
    SubmitWork(cmd);
}

void GpuPgm::PresentScreen(int turn, const SDL_FRect &to) {
    const int32_t params[4] = {turn, 0, 0, 0};
    Present(m_screenPipeline, m_picture, params, to, true);
}

void GpuPgm::ScreenToPixels(int turn, int width, int height, std::vector<uint32_t> &out) {
    const int32_t params[4] = {turn, 0, 0, 0};
    PresentToPixels(m_screenPipeline, m_picture, params, width, height, out);
}

void GpuPgm::ReadScreen(uint32_t *out) {
    Download(m_picture, 0, 0, kW, kH, out, kW * kH * 4);
}

} // namespace spelunk
