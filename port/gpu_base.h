// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// What both boards' native renderers share (port/gpu.h for the CV1000, port/gpu68k.h for Cave's 68000 boards): the
// SDL_GPU device and the window's swapchain, shaders and pipelines made from port/shaders.inc, the transfer buffers,
// and the drawing of a finished picture into the window: turned, scaled into the rectangle given, with the training
// readouts laid over it; or a picture made on the CPU (the menu) instead. With no window (the picture goes through a
// shader, port/shading.h, in an OpenGL window SDL_GPU must not draw to) the device draws the board's picture all the
// same, and it is read back.
#pragma once

#include <SDL3/SDL.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace spelunk {

class GpuBase {
public:
    bool Ok() const {
        return m_device != nullptr;
    }
    // The bytes of the transfer buffer that parts of textures are brought back through (the training tools' reads of
    // VRAM keep it as large as the largest: the timeline counts it in its limit)
    size_t DownloadBytes() const {
        return m_download ? m_downloadSize : 0;
    }
    // The window in real pixels
    void OutputSize(int &width, int &height) const;
    // A picture laid over the screen by the next present and each after it (32 bits a pixel, red in the low byte, its
    // alpha as how much of it shows), over the same rectangle; none: no picture
    void SetOverlay(const uint32_t *pixels, int width, int height);
    // A picture laid over the whole window by each present after this (the touch controls: port/touch.h), stretched
    // to it, over the picture and its overlay; as SetOverlay's pixels. None: none
    void SetWindowLayer(const uint32_t *pixels, int width, int height);
    // Shows a picture made on the CPU (32 bits a pixel, red in the low byte) instead of the screen
    void PresentPicture(const uint32_t *pixels, int width, int height, const SDL_FRect &to);
    // The window drawn into from now on, the one before let go of; none: no window (presents show nothing). The device
    // and all it holds stay. False if the window will not do (and there is then none)
    bool SetWindow(SDL_Window *window);
    // Each present waits for the screen's refresh (on: full screen at an arcade monitor's mode, paced by the screen,
    // recomp-common/arcade_screen.h), or (off, as the window is given) is shown the moment it is done
    void SetVsync(bool on);
    // Run-ahead's frames (port/hw_*.h's BeginAhead to EndAhead): their drawing is not held by SubmitWork's count. They
    // are a few to each present (run-ahead's 1 to 4), never pile up, and before they come the frame's own drawing was
    // waited for at most as a present waits; held, every frame from the fourth on waited for the GPU (#46)
    void AheadWork(bool on) {
        m_aheadWork = on;
    }

protected:
    GpuBase() = default;
    GpuBase(const GpuBase &) = delete;
    GpuBase &operator=(const GpuBase &) = delete;
    ~GpuBase() = default;

    // The device and what every renderer uses; false (and nothing kept) if the window's GPU will not do. No window:
    // the device alone (SetWindow gives it one later)
    bool Open(SDL_Window *window);
    void Close();

    SDL_GPUTexture *MakeTexture(SDL_GPUTextureFormat format, int w, int h, SDL_GPUTextureUsageFlags usage);
    SDL_GPUShader *MakeShader(const unsigned char *spirv, size_t spirvSize, const char *msl, bool fragment, unsigned samplers, unsigned uniforms);
    SDL_GPUGraphicsPipeline *MakePipeline(SDL_GPUShader *vertex, SDL_GPUShader *fragment, SDL_GPUTextureFormat format, bool blend = false);
    // The renderer's shader that draws its screen into the window (kept here, released at Close): m_screenPipeline,
    // made again if a window of another format is given
    bool SetScreenShader(SDL_GPUShader *screen);
    // `size` bytes of the upload transfer buffer, mapped: the frame's next free part of it (m_uploadOffset, where the
    // part starts). A frame's uploads go one after another into one buffer, which is cycled (what the GPU still reads
    // of the frames before is not touched) only when a frame begins or the buffer is full: never once a pass, which
    // had SDL make a buffer for each pass in flight
    void *MapUpload(uint32_t size);
    // Rows of `width` texels from the upload buffer (at `offset` into the part MapUpload gave) into a texture, never
    // cycling the texture (what is not written keeps its texels)
    void UploadRows(SDL_GPUCopyPass *copy, SDL_GPUTexture *to, int x, int y, int width, int rows, uint32_t offset = 0);
    // A frame's drawing submitted (the CV1000's lists, the 68000 boards' and the PGM's sprites and layers: not a
    // present). Frames run unseen (a game run on to where a mode begins shows one picture in 20) had nothing to hold
    // them, and queued 20 frames of GPU work before each present: now every kFramesInFlight-th submit since the last
    // present leaves a fence, and the next such submit waits for the one before, so at most about twice
    // kFramesInFlight frames are queued
    void SubmitWork(SDL_GPUCommandBuffer *cmd);
    void WaitFence(SDL_GPUFence *fence);
    bool AcquireSwapchain(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture **swapchain, Uint32 *w, Uint32 *h);
    // Part of a texture brought back, waiting for the GPU
    void Download(SDL_GPUTexture *texture, int x, int y, int w, int h, void *out, uint32_t bytes);
    // The transfer buffer let go when it is larger than `most` bytes (Gpu::GetVram: after a read of all of VRAM, 64 MB,
    // which a save state or the timeline's beginning make only now and then, it is not kept for the small reads after)
    void ShrinkDownload(uint32_t most);
    // Part of a texture brought back without waiting (the input lag readout's looks, port/main.cpp): StartRead puts the
    // copy on the GPU, into a buffer of its own (one of kReads), and returns at once; EndRead waits for it (by then, a
    // frame on, long done) and gives it. False: none started there
    static constexpr int kReads = 2;
    void StartRead(int which, SDL_GPUTexture *texture, int x, int y, int w, int h, uint32_t bytes);
    bool EndRead(int which, void *out, uint32_t bytes);

public:
    // (whether the GPU has done that copy: EndRead would not wait)
    bool ReadDone(int which) {
        return !m_readFences[which] || SDL_QueryGPUFence(m_device, m_readFences[which]);
    }

protected:

    // One triangle over the viewport with `pipeline`, reading `texture` (and the 16 bytes of `params`, if any, as its
    // uniforms), and the overlay over it when `overlay`: into the window, or into pixels here (`width` x `height`, 32
    // bits each, red in the low byte: the checks' shot of the window's own path)
    void Present(SDL_GPUGraphicsPipeline *pipeline, SDL_GPUTexture *texture, const int32_t *params, const SDL_FRect &to, bool overlay);
    void PresentToPixels(SDL_GPUGraphicsPipeline *pipeline, SDL_GPUTexture *texture, const int32_t *params, int width, int height,
                         std::vector<uint32_t> &out);

    SDL_Window *m_window = nullptr;
    bool m_vsync = false; // (SetVsync's)
    SDL_GPUDevice *m_device = nullptr;
    SDL_GPUTextureFormat m_swapFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
    SDL_GPUSampler *m_nearest = nullptr;
    SDL_GPUShader *m_fullVertex = nullptr; // (port/shaders/full.vert: one triangle over the target)
    SDL_GPUGraphicsPipeline *m_screenPipeline = nullptr;
    SDL_GPUTransferBuffer *m_upload = nullptr, *m_download = nullptr;
    uint32_t m_uploadSize = 0, m_downloadSize = 0;
    uint32_t m_uploadOffset = 0; // (where the part the last MapUpload gave starts)

private:
    // Frames in flight, at most this many: each present's command buffer leaves a fence, and the present waits for the
    // one from this many presents before (at once when the GPU keeps up: no lag; docs/LAG.md). Without it nothing
    // held the CPU back but the swapchain, which does not when there is no picture to give (a hidden or minimised
    // window, the shader's path) or the system shows frames without waiting: then a GPU slower than the game had
    // frames queued without end, and SDL a transfer buffer for each
    static constexpr int kFramesInFlight = 2;
    SDL_GPUFence *m_frameFences[kFramesInFlight] = {};
    SDL_GPUTransferBuffer *m_reads[kReads] = {};
    SDL_GPUFence *m_readFences[kReads] = {};
    uint32_t m_readSizes[kReads] = {};
    int m_frameAt = 0;
    SDL_GPUFence *m_workFence = nullptr; // (SubmitWork's, while no present comes)
    int m_workSincePresent = 0;
    bool m_aheadWork = false; // (AheadWork's)
    uint32_t m_uploadAt = 0; // (the upload buffer's next free byte this frame; past its end: cycled at the next map)
    void EndFrame(SDL_GPUCommandBuffer *cmd);
    SDL_GPUTexture *m_picture = nullptr, *m_overlay = nullptr, *m_layer = nullptr;
    int m_pictureW = 0, m_pictureH = 0, m_overlayW = 0, m_overlayH = 0, m_layerW = 0, m_layerH = 0;
    bool m_overlayOn = false, m_layerOn = false;
    SDL_GPUSampler *m_linear = nullptr; // (the window's layer, stretched smoothly)
    // A texture of these pixels, made again at another size, uploaded
    void Upload(SDL_GPUTexture *&texture, int &haveW, int &haveH, const uint32_t *pixels, int width, int height);
    SDL_GPUGraphicsPipeline *m_picturePipeline = nullptr, *m_overlayPipeline = nullptr;
    SDL_GPUShader *m_pictureShader = nullptr, *m_overlayShader = nullptr, *m_screenShader = nullptr;
    bool Claim(SDL_Window *window);
    bool MakeWindowPipelines();
    void Draw(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *target, bool clear, SDL_GPUGraphicsPipeline *pipeline, SDL_GPUTexture *texture,
              const int32_t *params, const SDL_FRect *to, bool overlay, const SDL_FRect *whole = nullptr);
};

} // namespace spelunk
