// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
#include "gpu_base.h"

#include "shaders.inc"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

namespace spelunk {

SDL_GPUTexture *GpuBase::MakeTexture(SDL_GPUTextureFormat format, int w, int h, SDL_GPUTextureUsageFlags usage) {
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = format;
    info.usage = usage;
    info.width = static_cast<Uint32>(w);
    info.height = static_cast<Uint32>(h);
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    return SDL_CreateGPUTexture(m_device, &info);
}

SDL_GPUShader *GpuBase::MakeShader(const unsigned char *spirv, size_t spirvSize, const char *msl, bool fragment, unsigned samplers, unsigned uniforms) {
    SDL_GPUShaderCreateInfo info{};
    info.stage = fragment ? SDL_GPU_SHADERSTAGE_FRAGMENT : SDL_GPU_SHADERSTAGE_VERTEX;
    info.num_samplers = samplers;
    info.num_uniform_buffers = uniforms;
    if (SDL_GetGPUShaderFormats(m_device) & SDL_GPU_SHADERFORMAT_MSL) {
        info.code = reinterpret_cast<const Uint8 *>(msl);
        info.code_size = strlen(msl);
        info.entrypoint = "main0";
        info.format = SDL_GPU_SHADERFORMAT_MSL;
    } else {
        info.code = spirv;
        info.code_size = spirvSize;
        info.entrypoint = "main";
        info.format = SDL_GPU_SHADERFORMAT_SPIRV;
    }
    SDL_GPUShader *shader = SDL_CreateGPUShader(m_device, &info);
    if (!shader) {
        fprintf(stderr, "gpu: shader: %s\n", SDL_GetError());
    }
    return shader;
}

SDL_GPUGraphicsPipeline *GpuBase::MakePipeline(SDL_GPUShader *vertex, SDL_GPUShader *fragment, SDL_GPUTextureFormat format, bool blend) {
    SDL_GPUColorTargetDescription target{};
    target.format = format;
    if (blend) {
        target.blend_state.enable_blend = true;
        target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        target.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        target.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    }
    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = vertex;
    info.fragment_shader = fragment;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    info.rasterizer_state.enable_depth_clip = true; // (no depth clamping: Open)
    info.target_info.color_target_descriptions = &target;
    info.target_info.num_color_targets = 1;
    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(m_device, &info);
    if (!pipeline) {
        fprintf(stderr, "gpu: pipeline: %s\n", SDL_GetError());
    }
    return pipeline;
}

bool GpuBase::Open(SDL_Window *window) {
    // (without the optional features SDL asks for unless told not to, none of which is used here: many phones' GPUs lack
    // one (Mali: drawIndirectFirstInstance), and SDL then gives no device at all; as satrecomp found)
    const SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, true);
    SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_MSL_BOOLEAN, true);
    SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, false);
    SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_CLIP_DISTANCE_BOOLEAN, false);
    SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_DEPTH_CLAMPING_BOOLEAN, false);
    SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_INDIRECT_DRAW_FIRST_INSTANCE_BOOLEAN, false);
    SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_ANISOTROPY_BOOLEAN, false);
    m_device = SDL_CreateGPUDeviceWithProperties(props);
    SDL_DestroyProperties(props);
    if (!m_device) {
        fprintf(stderr, "gpu: %s\n", SDL_GetError());
        return false;
    }
    // The GPU and driver SDL chose, once, into the output (the launcher's logs/game.log): what a player can tell us
    static bool told = false;
    if (!told) {
        told = true;
        std::string line = std::string("gpu: ") + SDL_GetGPUDeviceDriver(m_device);
#if SDL_VERSION_ATLEAST(3, 4, 0)
        const SDL_PropertiesID about = SDL_GetGPUDeviceProperties(m_device);
        for (const char *key : {SDL_PROP_GPU_DEVICE_NAME_STRING, SDL_PROP_GPU_DEVICE_DRIVER_NAME_STRING,
                                SDL_PROP_GPU_DEVICE_DRIVER_INFO_STRING, SDL_PROP_GPU_DEVICE_DRIVER_VERSION_STRING}) {
            std::string value = SDL_GetStringProperty(about, key, "");
            if (strcmp(key, SDL_PROP_GPU_DEVICE_DRIVER_VERSION_STRING) == 0 && SDL_HasProperty(about, SDL_PROP_GPU_DEVICE_DRIVER_INFO_STRING)) {
                continue; // (the version, where the driver's own description already says it)
            }
            std::replace(value.begin(), value.end(), '\n', ' ');
            while (!value.empty() && value.back() == ' ') {
                value.pop_back();
            }
            if (!value.empty()) {
                line += ", " + value;
            }
        }
#endif
        printf("%s\n", line.c_str());
        fflush(stdout);
    }
    // (with no window, the format windows' pictures most often have: a window given later that has another makes the
    // pipelines again, SetWindow)
    m_swapFormat = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    if (window && !Claim(window)) {
        SDL_DestroyGPUDevice(m_device);
        m_device = nullptr;
        return false;
    }

    SDL_GPUSamplerCreateInfo sampler{};
    sampler.min_filter = sampler.mag_filter = SDL_GPU_FILTER_NEAREST;
    sampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sampler.address_mode_u = sampler.address_mode_v = sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    m_nearest = SDL_CreateGPUSampler(m_device, &sampler);
    m_fullVertex = MakeShader(k_full_vert_spirv, sizeof k_full_vert_spirv, k_full_vert_msl, false, 0, 0);
    m_pictureShader = MakeShader(k_picture_frag_spirv, sizeof k_picture_frag_spirv, k_picture_frag_msl, true, 1, 0);
    m_overlayShader = MakeShader(k_overlay_frag_spirv, sizeof k_overlay_frag_spirv, k_overlay_frag_msl, true, 1, 0);
    if (!m_nearest || !m_fullVertex || !m_pictureShader || !m_overlayShader || !MakeWindowPipelines()) {
        Close();
        return false;
    }
    return true;
}

bool GpuBase::Claim(SDL_Window *window) {
    if (!SDL_ClaimWindowForGPUDevice(m_device, window)) {
        fprintf(stderr, "gpu: %s\n", SDL_GetError());
        return false;
    }
    m_window = window;
    m_vsync = true;
    SetVsync(false);
    m_swapFormat = SDL_GetGPUSwapchainTextureFormat(m_device, window);
    return true;
}

void GpuBase::SetVsync(bool on) {
    if (!m_device || !m_window || on == m_vsync) {
        return;
    }
    m_vsync = on;
    // Off: a frame is shown when it is done, no waiting for the display where the system allows (docs/LAG.md)
    SDL_GPUPresentMode mode = SDL_GPU_PRESENTMODE_VSYNC;
    if (!on && SDL_WindowSupportsGPUPresentMode(m_device, m_window, SDL_GPU_PRESENTMODE_IMMEDIATE)) {
        mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
    } else if (!on && SDL_WindowSupportsGPUPresentMode(m_device, m_window, SDL_GPU_PRESENTMODE_MAILBOX)) {
        mode = SDL_GPU_PRESENTMODE_MAILBOX;
    }
    SDL_SetGPUSwapchainParameters(m_device, m_window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode);
}

bool GpuBase::MakeWindowPipelines() {
    for (SDL_GPUGraphicsPipeline **p : {&m_picturePipeline, &m_overlayPipeline, &m_screenPipeline}) {
        if (*p) {
            SDL_ReleaseGPUGraphicsPipeline(m_device, *p);
            *p = nullptr;
        }
    }
    m_picturePipeline = MakePipeline(m_fullVertex, m_pictureShader, m_swapFormat);
    m_overlayPipeline = MakePipeline(m_fullVertex, m_overlayShader, m_swapFormat, true);
    if (m_screenShader) {
        m_screenPipeline = MakePipeline(m_fullVertex, m_screenShader, m_swapFormat);
    }
    return m_picturePipeline && m_overlayPipeline && (!m_screenShader || m_screenPipeline);
}

bool GpuBase::SetWindow(SDL_Window *window) {
    if (!m_device || window == m_window) {
        return m_device != nullptr;
    }
    SDL_WaitForGPUIdle(m_device);
    if (m_window) {
        SDL_ReleaseWindowFromGPUDevice(m_device, m_window);
        m_window = nullptr;
    }
    if (!window) {
        return true;
    }
    const SDL_GPUTextureFormat before = m_swapFormat;
    if (!Claim(window)) {
        m_swapFormat = before;
        return false;
    }
    return m_swapFormat == before || MakeWindowPipelines();
}

bool GpuBase::SetScreenShader(SDL_GPUShader *screen) {
    m_screenShader = screen;
    return screen && MakeWindowPipelines();
}

void GpuBase::Close() {
    if (!m_device) {
        return;
    }
    SDL_WaitForGPUIdle(m_device);
    for (SDL_GPUFence *&fence : m_frameFences) {
        if (fence) {
            SDL_ReleaseGPUFence(m_device, fence);
            fence = nullptr;
        }
    }
    m_frameAt = 0;
    if (m_workFence) {
        SDL_ReleaseGPUFence(m_device, m_workFence);
        m_workFence = nullptr;
    }
    m_workSincePresent = 0;
    m_uploadAt = m_uploadOffset = 0;
    for (SDL_GPUTexture *t : {m_picture, m_overlay, m_layer}) {
        if (t) {
            SDL_ReleaseGPUTexture(m_device, t);
        }
    }
    for (SDL_GPUGraphicsPipeline *p : {m_picturePipeline, m_overlayPipeline, m_screenPipeline}) {
        if (p) {
            SDL_ReleaseGPUGraphicsPipeline(m_device, p);
        }
    }
    for (SDL_GPUShader *s : {m_pictureShader, m_overlayShader, m_screenShader}) {
        if (s) {
            SDL_ReleaseGPUShader(m_device, s);
        }
    }
    for (SDL_GPUTransferBuffer *b : {m_upload, m_download}) {
        if (b) {
            SDL_ReleaseGPUTransferBuffer(m_device, b);
        }
    }
    if (m_fullVertex) {
        SDL_ReleaseGPUShader(m_device, m_fullVertex);
    }
    for (SDL_GPUSampler *sampler : {m_nearest, m_linear}) {
        if (sampler) {
            SDL_ReleaseGPUSampler(m_device, sampler);
        }
    }
    if (m_window) {
        SDL_ReleaseWindowFromGPUDevice(m_device, m_window);
    }
    SDL_DestroyGPUDevice(m_device);
    m_device = nullptr;
    m_window = nullptr;
    m_picture = m_overlay = m_layer = nullptr;
    m_layerOn = false;
    m_linear = nullptr;
    m_picturePipeline = m_overlayPipeline = m_screenPipeline = nullptr;
    m_pictureShader = m_overlayShader = m_screenShader = nullptr;
    m_upload = m_download = nullptr;
    m_uploadSize = m_downloadSize = 0;
    m_fullVertex = nullptr;
    m_nearest = nullptr;
}

void *GpuBase::MapUpload(uint32_t size) {
    // (each part starts 512 bytes in: Direct3D 12 copies to a texture only from there, SDL otherwise copying it again)
    constexpr uint32_t kAlign = 512;
    bool fresh = m_uploadAt > m_uploadSize || size > m_uploadSize - m_uploadAt;
    if (size > m_uploadSize) {
        if (m_upload) {
            SDL_ReleaseGPUTransferBuffer(m_device, m_upload); // (SDL keeps it until the GPU is done with it)
        }
        m_uploadSize = std::max(size, 4u << 20);
        SDL_GPUTransferBufferCreateInfo info{};
        info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        info.size = m_uploadSize;
        m_upload = SDL_CreateGPUTransferBuffer(m_device, &info);
        fresh = true;
    }
    if (fresh) {
        m_uploadAt = 0;
    }
    // (cycled only for a new buffer's worth; else the part after what this frame's passes before read)
    uint8_t *base = static_cast<uint8_t *>(SDL_MapGPUTransferBuffer(m_device, m_upload, fresh));
    m_uploadOffset = m_uploadAt;
    m_uploadAt += (size + kAlign - 1) / kAlign * kAlign;
    return base ? base + m_uploadOffset : nullptr;
}

void GpuBase::EndFrame(SDL_GPUCommandBuffer *cmd) {
    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    SDL_GPUFence *&before = m_frameFences[m_frameAt];
    if (before) {
        SDL_WaitForGPUFences(m_device, true, &before, 1);
        SDL_ReleaseGPUFence(m_device, before);
    }
    before = fence;
    m_frameAt = (m_frameAt + 1) % kFramesInFlight;
    m_uploadAt = UINT32_MAX; // (the next frame's uploads into a buffer the GPU is not reading)
    if (m_workFence) {
        SDL_ReleaseGPUFence(m_device, m_workFence); // (the present's own fence holds the frames from here)
        m_workFence = nullptr;
    }
    m_workSincePresent = 0;
}

void GpuBase::SubmitWork(SDL_GPUCommandBuffer *cmd) {
    // (a present between two frames, the usual case, starts the count again, so this never waits then; run-ahead's
    // frames and online rollback's frames run again, several to a present, wait at most for work from earlier in the
    // same frame)
    if (++m_workSincePresent % kFramesInFlight != 0) {
        SDL_SubmitGPUCommandBuffer(cmd);
        return;
    }
    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if (m_workFence) {
        SDL_WaitForGPUFences(m_device, true, &m_workFence, 1);
        SDL_ReleaseGPUFence(m_device, m_workFence);
    }
    m_workFence = fence;
}

void GpuBase::UploadRows(SDL_GPUCopyPass *copy, SDL_GPUTexture *to, int x, int y, int width, int rows, uint32_t offset) {
    SDL_GPUTextureTransferInfo source{};
    source.transfer_buffer = m_upload;
    source.offset = m_uploadOffset + offset;
    source.pixels_per_row = static_cast<Uint32>(width);
    source.rows_per_layer = static_cast<Uint32>(rows);
    SDL_GPUTextureRegion region{};
    region.texture = to;
    region.x = static_cast<Uint32>(x);
    region.y = static_cast<Uint32>(y);
    region.w = static_cast<Uint32>(width);
    region.h = static_cast<Uint32>(rows);
    region.d = 1;
    // (never "cycled": that would throw the rest of the texture away)
    SDL_UploadToGPUTexture(copy, &source, &region, false);
}

void GpuBase::Download(SDL_GPUTexture *texture, int x, int y, int w, int h, void *out, uint32_t bytes) {
    if (bytes > m_downloadSize) {
        if (m_download) {
            SDL_ReleaseGPUTransferBuffer(m_device, m_download);
        }
        m_downloadSize = bytes;
        SDL_GPUTransferBufferCreateInfo info{};
        info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
        info.size = bytes;
        m_download = SDL_CreateGPUTransferBuffer(m_device, &info);
    }
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(m_device);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureRegion region{};
    region.texture = texture;
    region.x = static_cast<Uint32>(x);
    region.y = static_cast<Uint32>(y);
    region.w = static_cast<Uint32>(w);
    region.h = static_cast<Uint32>(h);
    region.d = 1;
    SDL_GPUTextureTransferInfo to{};
    to.transfer_buffer = m_download;
    to.pixels_per_row = static_cast<Uint32>(w);
    to.rows_per_layer = static_cast<Uint32>(h);
    SDL_DownloadFromGPUTexture(copy, &region, &to);
    SDL_EndGPUCopyPass(copy);
    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    SDL_WaitForGPUFences(m_device, true, &fence, 1);
    SDL_ReleaseGPUFence(m_device, fence);
    std::memcpy(out, SDL_MapGPUTransferBuffer(m_device, m_download, false), bytes);
    SDL_UnmapGPUTransferBuffer(m_device, m_download);
}

void GpuBase::OutputSize(int &width, int &height) const {
    width = height = 0;
    if (m_window) {
        SDL_GetWindowSizeInPixels(m_window, &width, &height);
    }
}

void GpuBase::Draw(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *target, bool clear, SDL_GPUGraphicsPipeline *pipeline, SDL_GPUTexture *texture,
                   const int32_t *params, const SDL_FRect *to, bool overlay, const SDL_FRect *whole) {
    SDL_GPUColorTargetInfo info{};
    info.texture = target;
    info.load_op = SDL_GPU_LOADOP_CLEAR;
    info.store_op = SDL_GPU_STOREOP_STORE;
    info.clear_color = {0, 0, 0, clear ? 1.0f : 0.0f};
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &info, 1, nullptr);
    SDL_BindGPUGraphicsPipeline(pass, pipeline);
    if (to) {
        const SDL_GPUViewport viewport{to->x, to->y, to->w, to->h, 0, 1};
        SDL_SetGPUViewport(pass, &viewport);
    }
    const SDL_GPUTextureSamplerBinding binding{texture, m_nearest};
    SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
    if (params) {
        SDL_PushGPUFragmentUniformData(cmd, 0, params, 16);
    }
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    if (overlay && m_overlayOn && m_overlay) {
        SDL_BindGPUGraphicsPipeline(pass, m_overlayPipeline);
        const SDL_GPUTextureSamplerBinding over{m_overlay, m_nearest};
        SDL_BindGPUFragmentSamplers(pass, 0, &over, 1);
        SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    }
    if (whole && m_layerOn && m_layer) {
        if (!m_linear) {
            SDL_GPUSamplerCreateInfo info{};
            info.min_filter = info.mag_filter = SDL_GPU_FILTER_LINEAR;
            info.address_mode_u = info.address_mode_v = info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            m_linear = SDL_CreateGPUSampler(m_device, &info);
        }
        const SDL_GPUViewport viewport{whole->x, whole->y, whole->w, whole->h, 0, 1};
        SDL_SetGPUViewport(pass, &viewport);
        SDL_BindGPUGraphicsPipeline(pass, m_overlayPipeline);
        const SDL_GPUTextureSamplerBinding layer{m_layer, m_linear ? m_linear : m_nearest};
        SDL_BindGPUFragmentSamplers(pass, 0, &layer, 1);
        SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    }
    SDL_EndGPURenderPass(pass);
}

void GpuBase::Present(SDL_GPUGraphicsPipeline *pipeline, SDL_GPUTexture *texture, const int32_t *params, const SDL_FRect &to, bool overlay) {
    // (every present ends a frame, whether or not there is a window to show it in: EndFrame)
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(m_device);
    SDL_GPUTexture *swapchain = nullptr;
    Uint32 sw = 0, sh = 0;
    // (no window of its own: the picture goes through a shader, port/shading.h; or none to show it in, hidden or
    // minimised: SDL then gives no picture, and does not wait)
    if (m_window && SDL_WaitAndAcquireGPUSwapchainTexture(cmd, m_window, &swapchain, &sw, &sh) && swapchain) {
        const SDL_FRect whole{0, 0, static_cast<float>(sw), static_cast<float>(sh)};
        Draw(cmd, swapchain, true, pipeline, texture, params, &to, overlay, &whole);
    }
    EndFrame(cmd);
}

void GpuBase::PresentToPixels(SDL_GPUGraphicsPipeline *pipeline, SDL_GPUTexture *texture, const int32_t *params, int width, int height,
                              std::vector<uint32_t> &out) {
    SDL_GPUTexture *target = MakeTexture(m_swapFormat, width, height, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER);
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(m_device);
    Draw(cmd, target, false, pipeline, texture, params, nullptr, true);
    SDL_SubmitGPUCommandBuffer(cmd);
    out.resize(static_cast<size_t>(width) * height);
    Download(target, 0, 0, width, height, out.data(), static_cast<uint32_t>(out.size() * 4));
    SDL_ReleaseGPUTexture(m_device, target);
    // (the window's format may have blue in the low byte)
    if (m_swapFormat == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM || m_swapFormat == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB) {
        for (uint32_t &p : out) {
            p = (p & 0xFF00FF00u) | ((p & 0xFF) << 16) | ((p >> 16) & 0xFF);
        }
    }
}

void GpuBase::Upload(SDL_GPUTexture *&texture, int &haveW, int &haveH, const uint32_t *pixels, int width, int height) {
    if (!texture || width != haveW || height != haveH) {
        if (texture) {
            SDL_ReleaseGPUTexture(m_device, texture);
        }
        texture = MakeTexture(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, width, height, SDL_GPU_TEXTUREUSAGE_SAMPLER);
        haveW = width;
        haveH = height;
    }
    const uint32_t bytes = static_cast<uint32_t>(width) * height * 4;
    std::memcpy(MapUpload(bytes), pixels, bytes);
    SDL_UnmapGPUTransferBuffer(m_device, m_upload);
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(m_device);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    UploadRows(copy, texture, 0, 0, width, height);
    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(cmd);
}

void GpuBase::SetOverlay(const uint32_t *pixels, int width, int height) {
    m_overlayOn = pixels != nullptr;
    if (pixels) {
        Upload(m_overlay, m_overlayW, m_overlayH, pixels, width, height);
    }
}

void GpuBase::SetWindowLayer(const uint32_t *pixels, int width, int height) {
    m_layerOn = pixels != nullptr && m_device != nullptr;
    if (m_layerOn) {
        Upload(m_layer, m_layerW, m_layerH, pixels, width, height);
    }
}

void GpuBase::PresentPicture(const uint32_t *pixels, int width, int height, const SDL_FRect &to) {
    Upload(m_picture, m_pictureW, m_pictureH, pixels, width, height);
    Present(m_picturePipeline, m_picture, nullptr, to, false);
}

} // namespace spelunk
