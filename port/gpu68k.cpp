// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
#include "gpu68k.h"

#include "shaders.inc"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace spelunk {

namespace {
    constexpr int kW = CaveVideo::kWidth, kH = CaveVideo::kHeight;
    constexpr int kPixelsRow = 4096;     // the pixel textures' width
    constexpr int kSpriteDataW = 2048;   // two texels a sprite: up to 1,024 sprites a row
    constexpr int kMaxSprites = 0x4000 / 16;

    // The compose shader's uniforms (port/shaders/compose68k.frag, std140: each row an ivec4)
    struct ComposeParams {
        int32_t misc[4];
        int32_t on[CaveVideo::kLayers][4];
        int32_t lines[CaveVideo::kLayers][4];
        int32_t look[CaveVideo::kLayers][4];
        int32_t where[CaveVideo::kLayers][4];
    };

    // Words of the board's memory (big-endian bytes) as the GPU's 16-bit texels
    void Words(const std::vector<uint8_t> &bytes, uint8_t *out) {
        for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
            out[i] = bytes[i + 1];
            out[i + 1] = bytes[i];
        }
    }
} // namespace

Gpu68k::~Gpu68k() {
    Shutdown();
}

void Gpu68k::Shutdown() {
    if (!m_device) {
        return;
    }
    SDL_WaitForGPUIdle(m_device);
    for (SDL_GPUTexture *t : {m_spritePixels, m_tilePixels, m_spriteData, m_spriteBuffer, m_layers, m_palette, m_picture}) {
        if (t) {
            SDL_ReleaseGPUTexture(m_device, t);
        }
    }
    for (SDL_GPUGraphicsPipeline *p : {m_spritePipeline, m_composePipeline}) {
        if (p) {
            SDL_ReleaseGPUGraphicsPipeline(m_device, p);
        }
    }
    m_spritePixels = m_tilePixels = m_spriteData = m_spriteBuffer = m_layers = m_palette = m_picture = nullptr;
    m_spritePipeline = m_composePipeline = nullptr;
    Close();
}

SDL_GPUTexture *Gpu68k::MakePixels(const std::vector<uint8_t> &bytes) {
    const int rows = std::max<int>(1, static_cast<int>((bytes.size() + kPixelsRow - 1) / kPixelsRow));
    SDL_GPUTexture *texture = MakeTexture(SDL_GPU_TEXTUREFORMAT_R8_UINT, kPixelsRow, rows, SDL_GPU_TEXTUREUSAGE_SAMPLER);
    if (!texture) {
        return nullptr;
    }
    // (a transfer buffer of its own, let go afterwards: the sprites' pixels are many megabytes, sent once)
    const uint32_t size = static_cast<uint32_t>(rows) * kPixelsRow;
    SDL_GPUTransferBufferCreateInfo info{};
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    info.size = size;
    SDL_GPUTransferBuffer *whole = SDL_CreateGPUTransferBuffer(m_device, &info);
    uint8_t *to = static_cast<uint8_t *>(SDL_MapGPUTransferBuffer(m_device, whole, false));
    std::memset(to, 0, size);
    if (!bytes.empty()) {
        std::memcpy(to, bytes.data(), bytes.size());
    }
    SDL_UnmapGPUTransferBuffer(m_device, whole);
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(m_device);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureTransferInfo source{};
    source.transfer_buffer = whole;
    source.pixels_per_row = kPixelsRow;
    source.rows_per_layer = static_cast<Uint32>(rows);
    SDL_GPUTextureRegion region{};
    region.texture = texture;
    region.w = kPixelsRow;
    region.h = static_cast<Uint32>(rows);
    region.d = 1;
    SDL_UploadToGPUTexture(copy, &source, &region, false);
    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(m_device, whole);
    return texture;
}

bool Gpu68k::Init(SDL_Window *window, const CaveVideo &video) {
    if (!Open(window)) {
        return false;
    }
    // The pixels: the sprites', and every layer's tiles one after another
    m_spritePixels = MakePixels(video.spritePixels);
    m_spriteMask = video.spritePixels.empty() ? 0 : static_cast<uint32_t>(video.spritePixels.size() - 1);
    std::vector<uint8_t> tiles;
    for (int n = 0; n < CaveVideo::kLayers; n++) {
        const std::vector<uint8_t> &pixels = video.kinds[n].pixels;
        m_tileBase[n] = static_cast<int32_t>(tiles.size());
        m_tileCount[n] = std::max<int32_t>(1, static_cast<int32_t>(pixels.size() / 64));
        tiles.insert(tiles.end(), pixels.begin(), pixels.end());
    }
    m_tilePixels = MakePixels(tiles);
    const SDL_GPUTextureUsageFlags sampled = SDL_GPU_TEXTUREUSAGE_SAMPLER, target = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    m_spriteData = MakeTexture(SDL_GPU_TEXTUREFORMAT_R32G32B32A32_INT, kSpriteDataW, 1, sampled);
    m_spriteBuffer = MakeTexture(SDL_GPU_TEXTUREFORMAT_R16_UINT, kW, kH, sampled | target);
    m_layers = MakeTexture(SDL_GPU_TEXTUREFORMAT_R16_UINT, 128, 128 * CaveVideo::kLayers, sampled);
    m_palette = MakeTexture(SDL_GPU_TEXTUREFORMAT_R16_UINT, 256, 128, sampled);
    m_picture = MakeTexture(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, kW, kH, sampled | target);

    SDL_GPUShader *spriteVertex = MakeShader(k_sprites68k_vert_spirv, sizeof k_sprites68k_vert_spirv, k_sprites68k_vert_msl, false, 1, 1);
    SDL_GPUShader *sprite = MakeShader(k_sprites68k_frag_spirv, sizeof k_sprites68k_frag_spirv, k_sprites68k_frag_msl, true, 1, 1);
    SDL_GPUShader *compose = MakeShader(k_compose68k_frag_spirv, sizeof k_compose68k_frag_spirv, k_compose68k_frag_msl, true, 4, 1);
    SDL_GPUShader *screen = MakeShader(k_screen68k_frag_spirv, sizeof k_screen68k_frag_spirv, k_screen68k_frag_msl, true, 1, 1);
    bool ok = m_spritePixels && m_tilePixels && m_spriteData && m_spriteBuffer && m_layers && m_palette && m_picture && spriteVertex && sprite &&
              compose && screen;
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

void Gpu68k::Render(const CaveVideo &video, const NativeFrame68k &frame) {
    if (frame.palette.empty()) {
        return; // (no picture made yet)
    }
    // The frame's data, from one transfer buffer: the sprites, the layers' memory, the palette
    const uint32_t sprites = static_cast<uint32_t>(std::min<size_t>(frame.sprites.size(), kMaxSprites));
    m_spriteWords.assign(static_cast<size_t>(std::max(sprites, 1u)) * 8, 0);
    for (uint32_t i = 0; i < sprites; i++) {
        const NativeSprite &s = frame.sprites[i];
        int32_t *w = &m_spriteWords[static_cast<size_t>(i) * 8];
        w[0] = s.x;
        w[1] = s.y;
        w[2] = s.width;
        w[3] = s.height;
        w[4] = static_cast<int32_t>(s.code);
        w[5] = static_cast<int32_t>(s.attributes);
        w[6] = s.zoomX;
        w[7] = s.zoomY;
    }
    const uint32_t spriteBytes = static_cast<uint32_t>(m_spriteWords.size() * 4);
    const uint32_t layerBytes = 0x8000, paletteBytes = 0x10000;
    uint8_t *data = static_cast<uint8_t *>(MapUpload(spriteBytes + layerBytes * CaveVideo::kLayers + paletteBytes));
    std::memcpy(data, m_spriteWords.data(), spriteBytes);
    for (int n = 0; n < CaveVideo::kLayers; n++) {
        uint8_t *to = data + spriteBytes + layerBytes * n;
        if (frame.layerRam[n].size() == layerBytes) {
            Words(frame.layerRam[n], to);
        } else {
            std::memset(to, 0, layerBytes);
        }
    }
    Words(frame.palette, data + spriteBytes + layerBytes * CaveVideo::kLayers);
    SDL_UnmapGPUTransferBuffer(m_device, m_upload);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(m_device);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    UploadRows(copy, m_spriteData, 0, 0, static_cast<int>(m_spriteWords.size() / 4), 1);
    UploadRows(copy, m_layers, 0, 0, 128, 128 * CaveVideo::kLayers, spriteBytes);
    UploadRows(copy, m_palette, 0, 0, 256, 128, spriteBytes + layerBytes * CaveVideo::kLayers);
    SDL_EndGPUCopyPass(copy);

    // The sprite buffer: cleared, then each sprite in the list's order
    SDL_GPUColorTargetInfo buffer{};
    buffer.texture = m_spriteBuffer;
    buffer.load_op = SDL_GPU_LOADOP_CLEAR;
    buffer.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &buffer, 1, nullptr);
    if (sprites) {
        SDL_BindGPUGraphicsPipeline(pass, m_spritePipeline);
        const SDL_GPUTextureSamplerBinding data{m_spriteData, m_nearest}, pixels{m_spritePixels, m_nearest};
        SDL_BindGPUVertexSamplers(pass, 0, &data, 1);
        SDL_BindGPUFragmentSamplers(pass, 0, &pixels, 1);
        const int32_t params[4] = {static_cast<int32_t>(m_spriteMask), 0, 0, 0}, screen[4] = {kW, kH, 0, 0};
        SDL_PushGPUVertexUniformData(cmd, 0, screen, sizeof screen);
        SDL_PushGPUFragmentUniformData(cmd, 0, params, sizeof params);
        SDL_DrawGPUPrimitives(pass, sprites * 6, 1, 0, 0);
    }
    SDL_EndGPURenderPass(pass);
    spritesDrawn = sprites;

    // The picture: each layer as its registers had it when the picture was made (the same sums as CaveVideo::Draw)
    ComposeParams p{};
    p.misc[0] = video.layers;
    p.misc[1] = static_cast<int32_t>(video.backgroundPen);
    p.misc[2] = static_cast<int32_t>(video.spriteColourStep);
    p.misc[3] = video.left;
    for (int n = 0; n < video.layers && n < CaveVideo::kLayers; n++) {
        const uint16_t *regs = frame.layerRegs[n];
        const CaveVideo::LayerKind &kind = video.kinds[n];
        const bool big = !kind.only8 && (regs[1] & 0x2000), flipX = !(regs[0] & 0x8000), flipY = !(regs[1] & 0x8000);
        // (each layer sits a pixel to the side of the one before, and 8 x 8 tiles 8 more)
        const int offsetX = 0x13 - (n + 1) - (big ? 0 : 8), offsetY = -0x12;
        p.on[n][0] = !(regs[2] & 0x10) && !kind.pixels.empty();
        p.on[n][1] = big;
        p.on[n][2] = flipX;
        p.on[n][3] = flipY;
        p.lines[n][0] = !kind.only8 && (regs[0] & 0x4000);
        p.lines[n][1] = !kind.only8 && (regs[1] & 0x4000);
        p.lines[n][2] = (regs[0] & 0x1FF) - frame.spriteRegs[0] + (flipX ? offsetX + 2 : -offsetX);
        p.lines[n][3] = (regs[1] & 0x1FF) - frame.spriteRegs[1] + (flipY ? offsetY + 2 : -offsetY);
        p.look[n][0] = regs[2] & 3;
        p.look[n][1] = static_cast<int32_t>(kind.colourBase);
        p.look[n][2] = static_cast<int32_t>(kind.colourStep);
        p.look[n][3] = m_tileCount[n];
        p.where[n][0] = m_tileBase[n];
        p.where[n][1] = static_cast<int32_t>(kind.colourMask);
        p.where[n][2] = kind.banked && frame.tileBank;
    }
    SDL_GPUColorTargetInfo picture{};
    picture.texture = m_picture;
    picture.load_op = SDL_GPU_LOADOP_DONT_CARE;
    picture.store_op = SDL_GPU_STOREOP_STORE;
    pass = SDL_BeginGPURenderPass(cmd, &picture, 1, nullptr);
    SDL_BindGPUGraphicsPipeline(pass, m_composePipeline);
    const SDL_GPUTextureSamplerBinding bindings[4] = {
        {m_spriteBuffer, m_nearest}, {m_layers, m_nearest}, {m_tilePixels, m_nearest}, {m_palette, m_nearest}};
    SDL_BindGPUFragmentSamplers(pass, 0, bindings, 4);
    SDL_PushGPUFragmentUniformData(cmd, 0, &p, sizeof p);
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
    SubmitWork(cmd);
}

void Gpu68k::PresentScreen(int turn, const SDL_FRect &to) {
    const int32_t params[4] = {turn, 0, 0, 0};
    Present(m_screenPipeline, m_picture, params, to, true);
}

void Gpu68k::ScreenToPixels(int turn, int width, int height, std::vector<uint32_t> &out) {
    const int32_t params[4] = {turn, 0, 0, 0};
    PresentToPixels(m_screenPipeline, m_picture, params, width, height, out);
}

void Gpu68k::ReadScreen(uint32_t *out) {
    Download(m_picture, 0, 0, kW, kH, out, kW * kH * 4);
}

} // namespace spelunk
