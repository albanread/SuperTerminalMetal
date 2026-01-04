//
// st_api_video_mode.cpp
// SuperTerminal Framework
//
// Implementation of unified video mode C API
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "st_api_video_mode.h"
#include "st_api_context.h"
#include "../Display/DisplayManager.h"
#include "../Display/VideoMode/VideoModeManager.h"
#include "../Display/LoResBuffer.h"
#include "../Metal/MetalRenderer.h"
#include <cstring>
#include <cmath>
#include <cctype>

using namespace SuperTerminal;
using namespace STApi;

// Helper to get the video mode manager
static VideoModeManager* getVideoModeManager() {
    ST_CHECK_PTR_RET(ST_CONTEXT.display(), "DisplayManager", nullptr);
    return ST_CONTEXT.display()->getVideoModeManager();
}

// =============================================================================
// Video Mode Management
// =============================================================================

ST_API int st_video_mode_set(STVideoMode mode) {
    ST_LOCK;
    
    auto display = ST_CONTEXT.display();
    if (!display) return 0;
    
    VideoModeManager* mgr = display->getVideoModeManager();
    if (!mgr) return 0;
    
    VideoMode vmMode;
    switch (mode) {
        case ST_VIDEO_MODE_NONE:
            vmMode = VideoMode::NONE;
            display->setLoResMode(false);
            display->setUResMode(false);
            display->setXResMode(false);
            display->setWResMode(false);
            display->setPResMode(false);
            break;
        case ST_VIDEO_MODE_LORES:
            vmMode = VideoMode::LORES;
            display->setLoResMode(true);
            display->setUResMode(false);
            display->setXResMode(false);
            display->setWResMode(false);
            display->setPResMode(false);
            break;
        case ST_VIDEO_MODE_XRES:
            vmMode = VideoMode::XRES;
            display->setLoResMode(false);
            display->setUResMode(false);
            display->setXResMode(true);
            display->setWResMode(false);
            display->setPResMode(false);
            break;
        case ST_VIDEO_MODE_WRES:
            vmMode = VideoMode::WRES;
            display->setLoResMode(false);
            display->setUResMode(false);
            display->setXResMode(false);
            display->setWResMode(true);
            display->setPResMode(false);
            break;
        case ST_VIDEO_MODE_URES:
            vmMode = VideoMode::URES;
            display->setLoResMode(false);
            display->setUResMode(true);
            display->setXResMode(false);
            display->setWResMode(false);
            display->setPResMode(false);
            break;
        case ST_VIDEO_MODE_PRES:
            vmMode = VideoMode::PRES;
            display->setLoResMode(false);
            display->setUResMode(false);
            display->setXResMode(false);
            display->setWResMode(false);
            display->setPResMode(true);
            break;
        default:
            return 0;
    }
    
    return mgr->setVideoMode(vmMode) ? 1 : 0;
}

ST_API int st_video_mode_name(const char* modeName) {
    ST_LOCK;
    
    if (!modeName) return 0;
    
    auto display = ST_CONTEXT.display();
    if (!display) return 0;
    
    VideoModeManager* mgr = display->getVideoModeManager();
    if (!mgr) return 0;
    
    // Convert to lowercase for case-insensitive comparison
    std::string modeStr = modeName;
    for (char& c : modeStr) {
        c = std::tolower(static_cast<unsigned char>(c));
    }
    
    VideoMode vmMode;
    bool needsLoRes = false;
    int loResWidth = 0, loResHeight = 0;
    
    // Map string to video mode
    if (modeStr == "text" || modeStr == "none") {
        vmMode = VideoMode::NONE;
        display->setLoResMode(false);
        display->setUResMode(false);
        display->setXResMode(false);
        display->setWResMode(false);
        display->setPResMode(false);
    } else if (modeStr == "lores") {
        // LORES: 160×75
        vmMode = VideoMode::LORES;
        needsLoRes = true;
        loResWidth = LoResBuffer::LORES_WIDTH;
        loResHeight = LoResBuffer::LORES_HEIGHT;
    } else if (modeStr == "mres" || modeStr == "mediumres" || modeStr == "midres") {
        // MRES: 320×150
        vmMode = VideoMode::LORES;
        needsLoRes = true;
        loResWidth = LoResBuffer::MIDRES_WIDTH;
        loResHeight = LoResBuffer::MIDRES_HEIGHT;
    } else if (modeStr == "hres" || modeStr == "highres" || modeStr == "hires") {
        // HRES: 640×300
        vmMode = VideoMode::LORES;
        needsLoRes = true;
        loResWidth = LoResBuffer::HIRES_WIDTH;
        loResHeight = LoResBuffer::HIRES_HEIGHT;
    } else if (modeStr == "xres") {
        vmMode = VideoMode::XRES;
        display->setLoResMode(false);
        display->setUResMode(false);
        display->setXResMode(true);
        display->setWResMode(false);
        display->setPResMode(false);
    } else if (modeStr == "wres") {
        vmMode = VideoMode::WRES;
        display->setLoResMode(false);
        display->setUResMode(false);
        display->setXResMode(false);
        display->setWResMode(true);
        display->setPResMode(false);
    } else if (modeStr == "ures") {
        vmMode = VideoMode::URES;
        display->setLoResMode(false);
        display->setUResMode(true);
        display->setXResMode(false);
        display->setWResMode(false);
        display->setPResMode(false);
    } else if (modeStr == "pres") {
        vmMode = VideoMode::PRES;
        display->setLoResMode(false);
        display->setUResMode(false);
        display->setXResMode(false);
        display->setWResMode(false);
        display->setPResMode(true);
    } else {
        // Unknown mode
        ST_SET_ERROR("Unknown video mode name");
        return 0;
    }
    
    // Handle LORES variants (lores/mres/hres)
    if (needsLoRes) {
        // Get LORES buffers and resize them
        auto frontBuffer = display->getLoResBuffer(0);
        auto backBuffer = display->getLoResBuffer(1);
        if (frontBuffer && backBuffer) {
            frontBuffer->resize(loResWidth, loResHeight);
            backBuffer->resize(loResWidth, loResHeight);
        }
        display->setLoResMode(true);
        display->setUResMode(false);
        display->setXResMode(false);
        display->setWResMode(false);
        display->setPResMode(false);
    }
    
    return mgr->setVideoMode(vmMode) ? 1 : 0;
}

ST_API STVideoMode st_video_mode_get(void) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (!mgr) return ST_VIDEO_MODE_NONE;
    
    VideoMode mode = mgr->getVideoMode();
    switch (mode) {
        case VideoMode::NONE:  return ST_VIDEO_MODE_NONE;
        case VideoMode::LORES: return ST_VIDEO_MODE_LORES;
        case VideoMode::XRES:  return ST_VIDEO_MODE_XRES;
        case VideoMode::WRES:  return ST_VIDEO_MODE_WRES;
        case VideoMode::URES:  return ST_VIDEO_MODE_URES;
        case VideoMode::PRES:  return ST_VIDEO_MODE_PRES;
        default: return ST_VIDEO_MODE_NONE;
    }
}

ST_API int st_video_mode_is_active(STVideoMode mode) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (!mgr) return 0;
    
    VideoMode vmMode;
    switch (mode) {
        case ST_VIDEO_MODE_NONE:  vmMode = VideoMode::NONE; break;
        case ST_VIDEO_MODE_LORES: vmMode = VideoMode::LORES; break;
        case ST_VIDEO_MODE_XRES:  vmMode = VideoMode::XRES; break;
        case ST_VIDEO_MODE_WRES:  vmMode = VideoMode::WRES; break;
        case ST_VIDEO_MODE_URES:  vmMode = VideoMode::URES; break;
        case ST_VIDEO_MODE_PRES:  vmMode = VideoMode::PRES; break;
        default: return 0;
    }
    
    return mgr->isVideoModeActive(vmMode) ? 1 : 0;
}

ST_API void st_video_mode_disable(void) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        mgr->disableVideoMode();
    }
}

ST_API int st_video_mode_has_active(void) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (!mgr) return 0;
    return mgr->hasVideoMode() ? 1 : 0;
}

ST_API void st_video_mode_get_resolution(int* width, int* height) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr && width && height) {
        mgr->getCurrentResolution(*width, *height);
    }
}

ST_API void st_video_mode_get_mode_resolution(STVideoMode mode, int* width, int* height) {
    ST_LOCK;
    if (!width || !height) return;
    
    VideoMode vmMode;
    switch (mode) {
        case ST_VIDEO_MODE_NONE:  vmMode = VideoMode::NONE; break;
        case ST_VIDEO_MODE_LORES: vmMode = VideoMode::LORES; break;
        case ST_VIDEO_MODE_XRES:  vmMode = VideoMode::XRES; break;
        case ST_VIDEO_MODE_WRES:  vmMode = VideoMode::WRES; break;
        case ST_VIDEO_MODE_URES:  vmMode = VideoMode::URES; break;
        case ST_VIDEO_MODE_PRES:  vmMode = VideoMode::PRES; break;
        default: 
            *width = 0;
            *height = 0;
            return;
    }
    
    VideoModeManager::getModeResolution(vmMode, *width, *height);
}

// =============================================================================
// Unified Drawing API
// =============================================================================

ST_API void st_video_pset(int x, int y, uint32_t color) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        mgr->pset(x, y, color);
    }
}

ST_API uint32_t st_video_pget(int x, int y) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        return mgr->pget(x, y);
    }
    return 0;
}

ST_API void st_video_clear(uint32_t color) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        mgr->clear(color);
    }
}

ST_API void st_video_clear_gpu(int buffer_id, uint32_t color) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        mgr->clearGPU(buffer_id, color);
    }
}

ST_API void st_video_rect(int x, int y, int width, int height, uint32_t color) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        mgr->rect(x, y, width, height, color);
    }
}

ST_API void st_video_rect_gpu(int buffer_id, int x, int y, int width, int height, uint32_t color) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        mgr->rectGPU(buffer_id, x, y, width, height, color);
    }
}

ST_API void st_video_circle(int cx, int cy, int radius, uint32_t color) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        mgr->circle(cx, cy, radius, color);
    }
}

ST_API void st_video_circle_gpu(int buffer_id, int cx, int cy, int radius, uint32_t color) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        mgr->circleGPU(buffer_id, cx, cy, radius, color);
    }
}

ST_API void st_video_circle_aa(int buffer_id, int cx, int cy, int radius, uint32_t color) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        // Dispatches to AA version if video_enable_antialias(true) was called
        mgr->circleGPU(buffer_id, cx, cy, radius, color);
    }
}

ST_API void st_video_line(int x0, int y0, int x1, int y1, uint32_t color) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        mgr->line(x0, y0, x1, y1, color);
    }
}

ST_API void st_video_line_gpu(int buffer_id, int x0, int y0, int x1, int y1, uint32_t color) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        mgr->lineGPU(buffer_id, x0, y0, x1, y1, color);
    }
}

ST_API void st_video_line_aa(int buffer_id, int x0, int y0, int x1, int y1, uint32_t color) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        // For now, fall back to non-AA version
        mgr->lineGPU(buffer_id, x0, y0, x1, y1, color);
    }
}

ST_API void st_video_rect_gradient_gpu(int buffer_id, int x, int y, int width, int height,
                                       uint32_t topLeft, uint32_t topRight,
                                       uint32_t bottomLeft, uint32_t bottomRight) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        mgr->rectGradient(x, y, width, height, topLeft, topRight, bottomLeft, bottomRight);
    }
}

ST_API void st_video_circle_gradient_gpu(int buffer_id, int cx, int cy, int radius,
                                         uint32_t centerColor, uint32_t edgeColor) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        mgr->circleGradient(cx, cy, radius, centerColor, edgeColor);
    }
}

ST_API int st_video_supports_gradients(void) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (!mgr) return 0;
    
    // Gradients are only supported in URES mode
    return mgr->isUResMode() ? 1 : 0;
}

ST_API int st_video_enable_antialias(int enable) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (!mgr) return 0;
    
    // Enable/disable AA and return whether current mode supports it
    bool supported = mgr->enableAntialiasing(enable != 0);
    return supported ? 1 : 0;
}

ST_API int st_video_supports_antialias(void) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (!mgr) return 0;
    
    // Check if current mode has AA functions available
    return mgr->supportsAntialiasing() ? 1 : 0;
}

ST_API void st_video_set_line_width(float width) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (!mgr) return;
    
    mgr->setLineWidth(width);
}

ST_API float st_video_get_line_width(void) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (!mgr) return 1.0f;
    
    return mgr->getLineWidth();
}

// =============================================================================
// Buffer Query Functions
// =============================================================================

ST_API int st_video_get_back_buffer(void) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (!mgr) return 1; // Default to buffer 1
    return mgr->getBackBuffer();
}

ST_API int st_video_get_front_buffer(void) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (!mgr) return 0; // Default to buffer 0
    return mgr->getFrontBuffer();
}

// =============================================================================
// Auto-Buffering Drawing Functions (use back buffer automatically)
// =============================================================================

ST_API void st_video_clear_auto(uint32_t color) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        int backBuffer = mgr->getBackBuffer();
        mgr->clearGPU(backBuffer, color);
    }
}

ST_API void st_video_rect_auto(int x, int y, int width, int height, uint32_t color) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        int backBuffer = mgr->getBackBuffer();
        mgr->rectGPU(backBuffer, x, y, width, height, color);
    }
}

ST_API void st_video_circle_auto(int cx, int cy, int radius, uint32_t color) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        int backBuffer = mgr->getBackBuffer();
        mgr->circleGPU(backBuffer, cx, cy, radius, color);
    }
}

ST_API void st_video_line_auto(int x0, int y0, int x1, int y1, uint32_t color) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        int backBuffer = mgr->getBackBuffer();
        mgr->lineGPU(backBuffer, x0, y0, x1, y1, color);
    }
}

ST_API void st_video_circle_aa_auto(int cx, int cy, int radius, uint32_t color) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        int backBuffer = mgr->getBackBuffer();
        mgr->circleGPU(backBuffer, cx, cy, radius, color);
    }
}

ST_API void st_video_line_aa_auto(int x0, int y0, int x1, int y1, uint32_t color) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        int backBuffer = mgr->getBackBuffer();
        mgr->lineGPU(backBuffer, x0, y0, x1, y1, color);
    }
}

ST_API void st_video_rect_gradient_auto(int x, int y, int width, int height,
                                        uint32_t topLeft, uint32_t topRight,
                                        uint32_t bottomLeft, uint32_t bottomRight) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        mgr->rectGradient(x, y, width, height, topLeft, topRight, bottomLeft, bottomRight);
    }
}

ST_API void st_video_circle_gradient_auto(int cx, int cy, int radius,
                                          uint32_t centerColor, uint32_t edgeColor) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        mgr->circleGradient(cx, cy, radius, centerColor, edgeColor);
    }
}

// =============================================================================
// Command Batching Functions
// =============================================================================

ST_API void st_video_begin_batch(void) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");
    
    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }
    
    renderer->beginBlitBatch();
}

ST_API void st_video_end_batch(void) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");
    
    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }
    
    renderer->endBlitBatch();
}

ST_API void st_video_blit(int src_buffer, int dst_buffer,
                          int src_x, int src_y, 
                          int dst_x, int dst_y,
                          int width, int height) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        mgr->blit(src_x, src_y, width, height, dst_x, dst_y);
    }
}

ST_API void st_video_blit_trans(int src_buffer, int dst_buffer,
                                 int src_x, int src_y,
                                 int dst_x, int dst_y,
                                 int width, int height,
                                 uint32_t transparent_color) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        mgr->blitTrans(src_x, src_y, width, height, dst_x, dst_y);
    }
}

ST_API void st_video_blit_gpu(int src_buffer, int dst_buffer,
                               int src_x, int src_y,
                               int dst_x, int dst_y,
                               int width, int height) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        mgr->blitGPU(src_buffer, dst_buffer, src_x, src_y, width, height, dst_x, dst_y);
    }
}

ST_API void st_video_blit_trans_gpu(int src_buffer, int dst_buffer,
                                     int src_x, int src_y,
                                     int dst_x, int dst_y,
                                     int width, int height) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        mgr->blitTransGPU(src_buffer, dst_buffer, src_x, src_y, width, height, dst_x, dst_y);
    }
}

ST_API void st_video_buffer(int buffer_id) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        mgr->setActiveBuffer(buffer_id);
    }
}

ST_API int st_video_buffer_get(void) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        return mgr->getActiveBuffer();
    }
    return 0;
}

ST_API void st_video_flip(void) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        mgr->swapBuffers(0, 1);
    }
}

ST_API void st_video_gpu_flip(void) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (!mgr) return;
    
    // For GPU flip, we want to display the current buffer without swapping
    // Call the mode-specific GPU flip which displays buffer 0
    VideoMode mode = mgr->getVideoMode();
    switch (mode) {
        case VideoMode::URES:
            st_ures_gpu_flip();
            break;
        case VideoMode::XRES:
            st_xres_flip();
            break;
        case VideoMode::WRES:
            st_wres_flip();
            break;
        case VideoMode::LORES:
            st_lores_flip();
            break;
        default:
            break;
    }
}

ST_API void st_video_sync(int buffer_id) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        mgr->syncBuffer(buffer_id);
    }
}

ST_API void st_video_swap(int buffer_id) {
    ST_LOCK;
    VideoModeManager* mgr = getVideoModeManager();
    if (mgr) {
        mgr->swapBuffers(buffer_id, (buffer_id + 1) % 2);
    }
}

// =============================================================================
// LORES Simplified Wrappers
// =============================================================================

ST_API uint8_t st_lores_pget_simple(int x, int y) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.display(), "DisplayManager", 0);
    
    auto loResBuffer = ST_CONTEXT.display()->getLoResBuffer();
    if (!loResBuffer) return 0;
    
    int width = loResBuffer->getWidth();
    int height = loResBuffer->getHeight();
    
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return 0;
    }
    
    const uint8_t* data = loResBuffer->getPixelData();
    size_t index = y * width + x;
    
    // Extract the 4-bit color index from the packed byte
    return data[index] & 0x0F;
}

ST_API void st_lores_rect_simple(int x, int y, int width, int height, uint8_t color_index) {
    // Call the full version with black background
    st_lores_fillrect(x, y, width, height, color_index, st_rgb(0, 0, 0));
}

ST_API void st_lores_circle_simple(int cx, int cy, int radius, uint8_t color_index) {
    ST_LOCK;
    // Use a simple filled circle algorithm with pset
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x*x + y*y <= radius*radius) {
                st_lores_pset(cx + x, cy + y, color_index, st_rgb(0, 0, 0));
            }
        }
    }
}

ST_API void st_lores_line_simple(int x0, int y0, int x1, int y1, uint8_t color_index) {
    ST_LOCK;
    // Use Bresenham's line algorithm
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    
    int x = x0;
    int y = y0;
    
    while (true) {
        st_lores_pset(x, y, color_index, st_rgb(0, 0, 0));
        
        if (x == x1 && y == y1) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

// =============================================================================
// XRES Simplified Wrappers
// =============================================================================

ST_API void st_xres_rect_simple(int x, int y, int width, int height, uint8_t color_index) {
    // Just call fillrect
    st_xres_fillrect(x, y, width, height, color_index);
}

ST_API void st_xres_circle_simple(int cx, int cy, int radius, uint8_t color_index) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");
    
    int buffer = ST_CONTEXT.display()->getActiveXResBuffer();
    st_xres_circle_fill_gpu(buffer, cx, cy, radius, color_index);
}

ST_API void st_xres_line_simple(int x0, int y0, int x1, int y1, uint8_t color_index) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");
    
    int buffer = ST_CONTEXT.display()->getActiveXResBuffer();
    st_xres_line_gpu(buffer, x0, y0, x1, y1, color_index);
}

// =============================================================================
// WRES Simplified Wrappers
// =============================================================================

ST_API void st_wres_rect_simple(int x, int y, int width, int height, uint8_t color_index) {
    // Just call fillrect
    st_wres_fillrect(x, y, width, height, color_index);
}

ST_API void st_wres_circle_simple(int cx, int cy, int radius, uint8_t color_index) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");
    
    int buffer = ST_CONTEXT.display()->getActiveWResBuffer();
    st_wres_circle_fill_gpu(buffer, cx, cy, radius, color_index);
}

ST_API void st_wres_line_simple(int x0, int y0, int x1, int y1, uint8_t color_index) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");
    
    int buffer = ST_CONTEXT.display()->getActiveWResBuffer();
    st_wres_line_gpu(buffer, x0, y0, x1, y1, color_index);
}

// =============================================================================
// URES Simplified Wrappers
// =============================================================================

ST_API void st_ures_rect_simple(int x, int y, int width, int height, uint16_t color) {
    // Just call fillrect
    st_ures_fillrect(x, y, width, height, color);
}

ST_API void st_ures_circle_simple(int cx, int cy, int radius, uint16_t color) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");
    
    int buffer = ST_CONTEXT.display()->getActiveUResBuffer();
    st_ures_circle_fill_gpu(buffer, cx, cy, radius, color);
}

ST_API void st_ures_line_simple(int x0, int y0, int x1, int y1, uint16_t color) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");
    
    int buffer = ST_CONTEXT.display()->getActiveUResBuffer();
    st_ures_line_gpu(buffer, x0, y0, x1, y1, color);
}

// =============================================================================
// Unified API - Buffer Management (Phase 1)
// =============================================================================

ST_API int st_video_get_max_buffers(void) {
    ST_LOCK;
    auto mgr = getVideoModeManager();
    if (!mgr) return 0;
    return mgr->getMaxBuffers();
}

ST_API int st_video_is_valid_buffer(int buffer_id) {
    ST_LOCK;
    auto mgr = getVideoModeManager();
    if (!mgr) return 0;
    return mgr->isValidBuffer(buffer_id) ? 1 : 0;
}

ST_API int st_video_get_current_buffer(void) {
    ST_LOCK;
    auto mgr = getVideoModeManager();
    if (!mgr) return 0;
    return mgr->getCurrentBuffer();
}

// =============================================================================
// Unified API - Feature Detection (Phase 1)
// =============================================================================

ST_API uint32_t st_video_get_feature_flags(void) {
    ST_LOCK;
    auto mgr = getVideoModeManager();
    if (!mgr) return 0;
    return mgr->getFeatureFlags();
}

ST_API int st_video_uses_palette(void) {
    ST_LOCK;
    auto mgr = getVideoModeManager();
    if (!mgr) return 0;
    return mgr->usesPalette() ? 1 : 0;
}

ST_API int st_video_has_gpu(void) {
    ST_LOCK;
    auto mgr = getVideoModeManager();
    if (!mgr) return 0;
    uint32_t flags = mgr->getFeatureFlags();
    return (flags & VideoModeManager::FEATURE_GPU_PRIMITIVES) ? 1 : 0;
}

ST_API int st_video_get_color_depth(void) {
    ST_LOCK;
    auto mgr = getVideoModeManager();
    if (!mgr) return 0;
    return mgr->getColorDepth();
}

// =============================================================================
// Unified API - Memory Queries (Phase 2)
// =============================================================================

ST_API size_t st_video_get_memory_per_buffer(void) {
    ST_LOCK;
    auto mgr = getVideoModeManager();
    if (!mgr) return 0;
    return mgr->getMemoryPerBuffer();
}

ST_API size_t st_video_get_memory_usage(void) {
    ST_LOCK;
    auto mgr = getVideoModeManager();
    if (!mgr) return 0;
    return mgr->getMemoryUsage();
}

ST_API size_t st_video_get_pixel_count(void) {
    ST_LOCK;
    auto mgr = getVideoModeManager();
    if (!mgr) return 0;
    return mgr->getPixelCount();
}

// =============================================================================
// Unified API - Palette Management (Phase 2)
// =============================================================================

ST_API void st_video_reset_palette_to_default(void) {
    ST_LOCK;
    auto mgr = getVideoModeManager();
    if (!mgr) return;
    mgr->resetPaletteToDefault();
}