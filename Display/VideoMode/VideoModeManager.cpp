//
// VideoModeManager.cpp
// SuperTerminal Framework
//
// Implementation of VideoModeManager with unified drawing API dispatch
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "VideoModeManager.h"
#include "../DisplayManager.h"
#include "../../API/st_api_video_mode.h"
#include "../../API/st_api_video_palette.h"
#include "../../API/superterminal_api.h"
#include <stdexcept>

namespace SuperTerminal {

VideoModeManager::VideoModeManager()
    : m_currentMode(VideoMode::NONE)
    , m_displayManager(nullptr)
    , m_frontBuffer(0)
    , m_backBuffer(1)
    , m_aaEnabled(false)
    , m_lineWidth(1.0f) {
}

VideoModeManager::~VideoModeManager() {
}

// ================================================================
// Mode Management
// ================================================================

bool VideoModeManager::setVideoMode(VideoMode mode) {
    bool needsDefaultPalette = false;
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_currentMode == mode) {
            return true;  // Already in this mode
        }
        
        VideoMode previousMode = m_currentMode;
        m_currentMode = mode;
        
        // Reset buffer state when switching modes
        m_frontBuffer = 0;
        m_backBuffer = 1;
        
        // Reset AA state when switching modes
        m_aaEnabled = false;
        
        // Reset line width when switching modes
        m_lineWidth = 1.0f;
        
        // Check if we need to load default palette
        needsDefaultPalette = videoModeUsesPalette(mode);
        
        // Log mode change
        const char* prevName = videoModeToString(previousMode);
        const char* newName = videoModeToString(mode);
    } // Release mutex before calling palette function
    
    // Initialize default palette for palette-based modes outside the mutex
    // This prevents blank screens when users forget to set palettes
    if (needsDefaultPalette) {
        st_video_load_preset_palette(ST_PALETTE_IBM_RGBI);
    }
    
    // Mode change successful
    return true;
}

VideoMode VideoModeManager::getVideoMode() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentMode;
}

bool VideoModeManager::isVideoModeActive(VideoMode mode) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentMode == mode;
}

void VideoModeManager::disableVideoMode() {
    setVideoMode(VideoMode::NONE);
}

bool VideoModeManager::hasVideoMode() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentMode != VideoMode::NONE;
}

// ================================================================
// Resolution Info
// ================================================================

void VideoModeManager::getCurrentResolution(int& width, int& height) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    getVideoModeResolution(m_currentMode, width, height);
}

void VideoModeManager::getModeResolution(VideoMode mode, int& width, int& height) {
    getVideoModeResolution(mode, width, height);
}

// ================================================================
// DisplayManager Integration
// ================================================================

void VideoModeManager::setDisplayManager(DisplayManager* displayManager) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_displayManager = displayManager;
}

// ================================================================
// Helper Functions
// ================================================================

void VideoModeManager::requireActiveMode() const {
    if (m_currentMode == VideoMode::NONE) {
        throw std::runtime_error("No video mode active - call xres_mode(), wres_mode(), ures_mode(), or lores_mode() first");
    }
}

// ================================================================
// UNIFIED DRAWING API - Implementation with dispatch
// ================================================================

void VideoModeManager::pset(int x, int y, uint32_t color) {
    requireActiveMode();
    
    switch (m_currentMode) {
        case VideoMode::LORES:
            st_lores_pset(x, y, static_cast<uint8_t>(color & 0x0F), st_rgb(0, 0, 0));
            break;
        case VideoMode::XRES:
            st_xres_pset(x, y, static_cast<uint8_t>(color & 0xFF));
            break;
        case VideoMode::WRES:
            st_wres_pset(x, y, static_cast<uint8_t>(color & 0xFF));
            break;
        case VideoMode::URES:
            st_ures_pset(x, y, static_cast<uint16_t>(color & 0xFFFF));
            break;
        case VideoMode::PRES:
            st_pres_pset(x, y, static_cast<uint8_t>(color & 0xFF));
            break;
        default:
            break;
    }
}

uint32_t VideoModeManager::pget(int x, int y) {
    requireActiveMode();
    
    switch (m_currentMode) {
        case VideoMode::LORES:
            return static_cast<uint32_t>(st_lores_pget_simple(x, y));
        case VideoMode::XRES:
            return static_cast<uint32_t>(st_xres_pget(x, y));
        case VideoMode::WRES:
            return static_cast<uint32_t>(st_wres_pget(x, y));
        case VideoMode::URES:
            return static_cast<uint32_t>(st_ures_pget(x, y));
        case VideoMode::PRES:
            return static_cast<uint32_t>(st_pres_pget(x, y));
        default:
            return 0;
    }
}

void VideoModeManager::clear(uint32_t color) {
    requireActiveMode();
    
    switch (m_currentMode) {
        case VideoMode::LORES:
            st_lores_clear(st_rgb(0, 0, 0));
            break;
        case VideoMode::XRES:
            st_xres_clear(static_cast<uint8_t>(color & 0xFF));
            break;
        case VideoMode::WRES:
            st_wres_clear(static_cast<uint8_t>(color & 0xFF));
            break;
        case VideoMode::URES:
            st_ures_clear(static_cast<uint16_t>(color & 0xFFFF));
            break;
        case VideoMode::PRES:
            st_pres_clear(static_cast<uint8_t>(color & 0xFF));
            break;
        default:
            break;
    }
}

void VideoModeManager::clearGPU(int bufferID, uint32_t color) {
    requireActiveMode();
    
    switch (m_currentMode) {
        case VideoMode::LORES:
            st_lores_clear_gpu(bufferID, static_cast<uint8_t>(color & 0x0F));
            break;
        case VideoMode::XRES:
            st_xres_clear_gpu(bufferID, static_cast<uint8_t>(color & 0xFF));
            break;
        case VideoMode::WRES:
            st_wres_clear_gpu(bufferID, static_cast<uint8_t>(color & 0xFF));
            break;
        case VideoMode::URES:
            st_ures_clear_gpu(bufferID, static_cast<uint16_t>(color & 0xFFFF));
            break;
        case VideoMode::PRES:
            st_pres_clear_gpu(bufferID, static_cast<uint8_t>(color & 0xFF));
            break;
        default:
            break;
    }
}

void VideoModeManager::rect(int x, int y, int width, int height, uint32_t color) {
    requireActiveMode();
    
    switch (m_currentMode) {
        case VideoMode::LORES:
            st_lores_rect_simple(x, y, width, height, static_cast<uint8_t>(color & 0x0F));
            break;
        case VideoMode::XRES:
            st_xres_rect_simple(x, y, width, height, static_cast<uint8_t>(color & 0xFF));
            break;
        case VideoMode::WRES:
            st_wres_rect_simple(x, y, width, height, static_cast<uint8_t>(color & 0xFF));
            break;
        case VideoMode::URES:
            st_ures_rect_simple(x, y, width, height, static_cast<uint16_t>(color & 0xFFFF));
            break;
        case VideoMode::PRES:
            st_pres_fillrect(x, y, width, height, static_cast<uint8_t>(color & 0xFF));
            break;
        default:
            break;
    }
}

void VideoModeManager::rectGPU(int bufferID, int x, int y, int width, int height, uint32_t color) {
    requireActiveMode();
    
    switch (m_currentMode) {
        case VideoMode::LORES:
            st_lores_rect_fill_gpu(bufferID, x, y, width, height, static_cast<uint8_t>(color & 0x0F));
            break;
        case VideoMode::XRES:
            st_xres_rect_fill_gpu(bufferID, x, y, width, height, static_cast<uint8_t>(color & 0xFF));
            break;
        case VideoMode::WRES:
            st_wres_rect_fill_gpu(bufferID, x, y, width, height, static_cast<uint8_t>(color & 0xFF));
            break;
        case VideoMode::URES:
            st_ures_rect_fill_gpu(bufferID, x, y, width, height, static_cast<uint16_t>(color & 0xFFFF));
            break;
        case VideoMode::PRES:
            st_pres_rect_fill_gpu(bufferID, x, y, width, height, static_cast<uint8_t>(color & 0xFF));
            break;
        default:
            break;
    }
}

void VideoModeManager::circle(int cx, int cy, int radius, uint32_t color) {
    requireActiveMode();
    
    switch (m_currentMode) {
        case VideoMode::LORES:
            st_lores_circle_simple(cx, cy, radius, static_cast<uint8_t>(color & 0x0F));
            break;
        case VideoMode::XRES:
            st_xres_circle_simple(cx, cy, radius, static_cast<uint8_t>(color & 0xFF));
            break;
        case VideoMode::WRES:
            st_wres_circle_simple(cx, cy, radius, static_cast<uint8_t>(color & 0xFF));
            break;
        case VideoMode::URES:
            st_ures_circle_simple(cx, cy, radius, static_cast<uint16_t>(color & 0xFFFF));
            break;
        case VideoMode::PRES:
            st_pres_circle_simple(cx, cy, radius, static_cast<uint8_t>(color & 0xFF));
            break;
        default:
            break;
    }
}

void VideoModeManager::circleGPU(int bufferID, int cx, int cy, int radius, uint32_t color) {
    requireActiveMode();
    
    // Use AA version if enabled and supported
    if (m_aaEnabled && supportsAntialiasing()) {
        switch (m_currentMode) {
            case VideoMode::XRES:
                st_xres_circle_fill_aa(bufferID, cx, cy, radius, static_cast<uint8_t>(color & 0xFF));
                return;
            case VideoMode::WRES:
                st_wres_circle_fill_aa(bufferID, cx, cy, radius, static_cast<uint8_t>(color & 0xFF));
                return;
            case VideoMode::URES:
                st_ures_circle_fill_aa(bufferID, cx, cy, radius, static_cast<uint16_t>(color & 0xFFFF));
                return;
            case VideoMode::PRES:
                st_pres_circle_fill_aa(bufferID, cx, cy, radius, static_cast<uint8_t>(color & 0xFF));
                return;
            default:
                break;
        }
    }
    
    // Fall back to non-AA version
    switch (m_currentMode) {
        case VideoMode::XRES:
            st_xres_circle_fill_gpu(bufferID, cx, cy, radius, static_cast<uint8_t>(color & 0xFF));
            break;
        case VideoMode::WRES:
            st_wres_circle_fill_gpu(bufferID, cx, cy, radius, static_cast<uint8_t>(color & 0xFF));
            break;
        case VideoMode::URES:
            st_ures_circle_fill_gpu(bufferID, cx, cy, radius, static_cast<uint16_t>(color & 0xFFFF));
            break;
        case VideoMode::PRES:
            st_pres_circle_fill_gpu(bufferID, cx, cy, radius, static_cast<uint8_t>(color & 0xFF));
            break;
        case VideoMode::LORES:
            st_lores_circle_fill_gpu(bufferID, cx, cy, radius, static_cast<uint8_t>(color & 0x0F));
            break;
        default:
            break;
    }
}

void VideoModeManager::line(int x0, int y0, int x1, int y1, uint32_t color) {
    requireActiveMode();
    
    switch (m_currentMode) {
        case VideoMode::LORES:
            st_lores_line_simple(x0, y0, x1, y1, static_cast<uint8_t>(color & 0x0F));
            break;
        case VideoMode::XRES:
            st_xres_line_simple(x0, y0, x1, y1, static_cast<uint8_t>(color & 0xFF));
            break;
        case VideoMode::WRES:
            st_wres_line_simple(x0, y0, x1, y1, static_cast<uint8_t>(color & 0xFF));
            break;
        case VideoMode::URES:
            st_ures_line_simple(x0, y0, x1, y1, static_cast<uint16_t>(color & 0xFFFF));
            break;
        case VideoMode::PRES:
            st_pres_line_simple(x0, y0, x1, y1, static_cast<uint8_t>(color & 0xFF));
            break;
        default:
            break;
    }
}

void VideoModeManager::lineGPU(int bufferID, int x0, int y0, int x1, int y1, uint32_t color) {
    requireActiveMode();
    
    // Use AA version if enabled and supported
    if (m_aaEnabled && supportsAntialiasing()) {
        switch (m_currentMode) {
            case VideoMode::XRES:
                st_xres_line_aa(bufferID, x0, y0, x1, y1, static_cast<uint8_t>(color & 0xFF), m_lineWidth);
                return;
            case VideoMode::WRES:
                st_wres_line_aa(bufferID, x0, y0, x1, y1, static_cast<uint8_t>(color & 0xFF), m_lineWidth);
                return;
            case VideoMode::URES:
                st_ures_line_aa(bufferID, x0, y0, x1, y1, static_cast<uint16_t>(color & 0xFFFF), m_lineWidth);
                return;
            case VideoMode::PRES:
                st_pres_line_aa(bufferID, x0, y0, x1, y1, static_cast<uint8_t>(color & 0xFF), m_lineWidth);
                return;
            default:
                break;
        }
    }
    
    // Fall back to non-AA version
    switch (m_currentMode) {
        case VideoMode::XRES:
            st_xres_line_gpu(bufferID, x0, y0, x1, y1, static_cast<uint8_t>(color & 0xFF));
            break;
        case VideoMode::WRES:
            st_wres_line_gpu(bufferID, x0, y0, x1, y1, static_cast<uint8_t>(color & 0xFF));
            break;
        case VideoMode::URES:
            st_ures_line_gpu(bufferID, x0, y0, x1, y1, static_cast<uint16_t>(color & 0xFFFF));
            break;
        case VideoMode::PRES:
            st_pres_line_gpu(bufferID, x0, y0, x1, y1, static_cast<uint8_t>(color & 0xFF));
            break;
        case VideoMode::LORES:
            st_lores_line_gpu(bufferID, x0, y0, x1, y1, static_cast<uint8_t>(color & 0x0F));
            break;
        default:
            break;
    }
}

void VideoModeManager::blit(int srcX, int srcY, int width, int height, int dstX, int dstY) {
    requireActiveMode();
    
    switch (m_currentMode) {
        case VideoMode::LORES:
            st_lores_blit(srcX, srcY, width, height, dstX, dstY);
            break;
        case VideoMode::XRES:
            st_xres_blit(srcX, srcY, width, height, dstX, dstY);
            break;
        case VideoMode::WRES:
            st_wres_blit(srcX, srcY, width, height, dstX, dstY);
            break;
        case VideoMode::URES:
            st_ures_blit_from(0, srcX, srcY, width, height, dstX, dstY);
            break;
        case VideoMode::PRES:
            st_pres_blit(srcX, srcY, width, height, dstX, dstY);
            break;
        default:
            break;
    }
}

void VideoModeManager::blitTrans(int srcX, int srcY, int width, int height, int dstX, int dstY) {
    requireActiveMode();
    
    switch (m_currentMode) {
        case VideoMode::LORES:
            st_lores_blit_trans(srcX, srcY, width, height, dstX, dstY, 0);
            break;
        case VideoMode::XRES:
            st_xres_blit_trans(srcX, srcY, width, height, dstX, dstY);
            break;
        case VideoMode::WRES:
            st_wres_blit_trans(srcX, srcY, width, height, dstX, dstY);
            break;
        case VideoMode::URES:
            st_ures_blit_from_trans(0, srcX, srcY, width, height, dstX, dstY);
            break;
        case VideoMode::PRES:
            st_pres_blit_trans(srcX, srcY, width, height, dstX, dstY);
            break;
        default:
            break;
    }
}

void VideoModeManager::blitFrom(int srcBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY) {
    requireActiveMode();
    
    switch (m_currentMode) {
        case VideoMode::LORES:
            // LORES uses different API - need to get destination buffer
            st_lores_blit_buffer(srcBufferID, 0, srcX, srcY, width, height, dstX, dstY);
            break;
        case VideoMode::XRES:
            st_xres_blit_from(srcBufferID, srcX, srcY, width, height, dstX, dstY);
            break;
        case VideoMode::WRES:
            st_wres_blit_from(srcBufferID, srcX, srcY, width, height, dstX, dstY);
            break;
        case VideoMode::URES:
            st_ures_blit_from(srcBufferID, srcX, srcY, width, height, dstX, dstY);
            break;
        case VideoMode::PRES:
            st_pres_blit_from(srcBufferID, srcX, srcY, width, height, dstX, dstY);
            break;
        default:
            break;
    }
}

void VideoModeManager::blitFromTrans(int srcBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY) {
    requireActiveMode();
    
    switch (m_currentMode) {
        case VideoMode::LORES:
            st_lores_blit_buffer_trans(srcBufferID, 0, srcX, srcY, width, height, dstX, dstY, 0);
            break;
        case VideoMode::XRES:
            st_xres_blit_from_trans(srcBufferID, srcX, srcY, width, height, dstX, dstY);
            break;
        case VideoMode::WRES:
            st_wres_blit_from_trans(srcBufferID, srcX, srcY, width, height, dstX, dstY);
            break;
        case VideoMode::URES:
            st_ures_blit_from_trans(srcBufferID, srcX, srcY, width, height, dstX, dstY);
            break;
        case VideoMode::PRES:
            st_pres_blit_from_trans(srcBufferID, srcX, srcY, width, height, dstX, dstY);
            break;
        default:
            break;
    }
}

void VideoModeManager::blitGPU(int srcBufferID, int dstBufferID, int srcX, int srcY, 
                                int width, int height, int dstX, int dstY) {
    requireActiveMode();
    
    switch (m_currentMode) {
        case VideoMode::LORES:
            st_lores_blit_gpu(srcBufferID, dstBufferID, srcX, srcY, width, height, dstX, dstY);
            break;
        case VideoMode::XRES:
            st_xres_blit_gpu(srcBufferID, dstBufferID, srcX, srcY, width, height, dstX, dstY);
            break;
        case VideoMode::WRES:
            st_wres_blit_gpu(srcBufferID, dstBufferID, srcX, srcY, width, height, dstX, dstY);
            break;
        case VideoMode::URES:
            st_ures_blit_copy_gpu(srcBufferID, dstBufferID, srcX, srcY, width, height, dstX, dstY);
            break;
        case VideoMode::PRES:
            st_pres_blit_gpu(srcBufferID, dstBufferID, srcX, srcY, width, height, dstX, dstY);
            break;
        default:
            break;
    }
}

void VideoModeManager::blitTransGPU(int srcBufferID, int dstBufferID, int srcX, int srcY,
                                     int width, int height, int dstX, int dstY) {
    requireActiveMode();
    
    switch (m_currentMode) {
        case VideoMode::LORES:
            st_lores_blit_trans_gpu(srcBufferID, dstBufferID, srcX, srcY, width, height, dstX, dstY, 0);
            break;
        case VideoMode::XRES:
            st_xres_blit_trans_gpu(srcBufferID, dstBufferID, srcX, srcY, width, height, dstX, dstY, 0);
            break;
        case VideoMode::WRES:
            st_wres_blit_trans_gpu(srcBufferID, dstBufferID, srcX, srcY, width, height, dstX, dstY, 0);
            break;
        case VideoMode::URES:
            st_ures_blit_transparent_gpu(srcBufferID, dstBufferID, srcX, srcY, width, height, dstX, dstY);
            break;
        case VideoMode::PRES:
            st_pres_blit_trans_gpu(srcBufferID, dstBufferID, srcX, srcY, width, height, dstX, dstY, 0);
            break;
        default:
            break;
    }
}

void VideoModeManager::setActiveBuffer(int bufferID) {
    requireActiveMode();
    
    switch (m_currentMode) {
        case VideoMode::LORES:
            st_lores_buffer(bufferID);
            break;
        case VideoMode::XRES:
            st_xres_buffer(bufferID);
            break;
        case VideoMode::WRES:
            st_wres_buffer(bufferID);
            break;
        case VideoMode::URES:
            st_ures_buffer(bufferID);
            break;
        case VideoMode::PRES:
            st_pres_buffer(bufferID);
            break;
        default:
            break;
    }
}

int VideoModeManager::getActiveBuffer() const {
    if (m_currentMode == VideoMode::NONE) {
        return 0;
    }
    
    if (!m_displayManager) {
        return 0;
    }
    
    switch (m_currentMode) {
        case VideoMode::LORES:
            return m_displayManager->getActiveLoResBuffer();
        case VideoMode::XRES:
            return m_displayManager->getActiveXResBuffer();
        case VideoMode::WRES:
            return m_displayManager->getActiveWResBuffer();
        case VideoMode::URES:
            return m_displayManager->getActiveUResBuffer();
        case VideoMode::PRES:
            return m_displayManager->getActivePResBuffer();
        default:
            return 0;
    }
}

int VideoModeManager::getCurrentBuffer() const {
    return getActiveBuffer();
}

void VideoModeManager::syncBuffer(int bufferID) {
    requireActiveMode();
    
    // All GPU operations use the same sync mechanism
    st_gpu_sync();
}

void VideoModeManager::swapBuffers(int bufferA, int bufferB) {
    requireActiveMode();
    
    // Call the mode-specific flip which swaps the actual buffer pointers
    // This also updates what the renderer displays
    switch (m_currentMode) {
        case VideoMode::XRES:
            st_xres_flip();
            break;
        case VideoMode::WRES:
            st_wres_flip();
            break;
        case VideoMode::URES:
            st_ures_flip();  // This swaps the buffer pointers
            break;
        case VideoMode::PRES:
            st_pres_flip();
            break;
        case VideoMode::LORES:
            st_lores_flip();
            break;
        default:
            break;
    }
    
    // Now update our front/back tracking to match the actual buffer swap
    flipBuffers();
}

void VideoModeManager::flipBuffers() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::swap(m_frontBuffer, m_backBuffer);
}

// ================================================================
// Anti-Aliasing Support
// ================================================================

bool VideoModeManager::enableAntialiasing(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_aaEnabled = enable;
    return supportsAntialiasing();
}

bool VideoModeManager::supportsAntialiasing() const {
    // AA functions are available for XRES, WRES, and URES
    // LORES does not have AA support
    switch (m_currentMode) {
        case VideoMode::XRES:
        case VideoMode::WRES:
        case VideoMode::URES:
        case VideoMode::PRES:
            return true;
        case VideoMode::LORES:
        case VideoMode::NONE:
        default:
            return false;
    }
}

void VideoModeManager::rectGradient(int x, int y, int width, int height,
                                     uint32_t topLeft, uint32_t topRight,
                                     uint32_t bottomLeft, uint32_t bottomRight) {
    if (m_currentMode != VideoMode::URES) {
        throw std::runtime_error("Gradient rectangles are only supported in URES mode");
    }
    
    st_ures_rect_fill_gradient_gpu(0, x, y, width, height,
                                    static_cast<uint16_t>(topLeft & 0xFFFF),
                                    static_cast<uint16_t>(topRight & 0xFFFF),
                                    static_cast<uint16_t>(bottomLeft & 0xFFFF),
                                    static_cast<uint16_t>(bottomRight & 0xFFFF));
}

void VideoModeManager::circleGradient(int cx, int cy, int radius,
                                       uint32_t centerColor, uint32_t edgeColor) {
    if (m_currentMode != VideoMode::URES) {
        throw std::runtime_error("Gradient circles are only supported in URES mode");
    }
    
    st_ures_circle_fill_gradient_gpu(0, cx, cy, static_cast<float>(radius),
                                      static_cast<uint16_t>(centerColor & 0xFFFF),
                                      static_cast<uint16_t>(edgeColor & 0xFFFF));
}

// ================================================================
// Mode-Specific Features
// ================================================================

int VideoModeManager::getColorDepth() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return getVideoModeColorDepth(m_currentMode);
}

bool VideoModeManager::usesPalette() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return videoModeUsesPalette(m_currentMode);
}

bool VideoModeManager::supportsAlpha() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return videoModeSupportsAlpha(m_currentMode);
}

uint32_t VideoModeManager::getFeatureFlags() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t flags = FEATURE_NONE;
    
    switch (m_currentMode) {
        case VideoMode::XRES:
        case VideoMode::WRES:
        case VideoMode::PRES:
            flags |= FEATURE_GPU_PRIMITIVES;
            flags |= FEATURE_ANTIALIASING;
            flags |= FEATURE_PER_ROW_PALETTE;
            flags |= FEATURE_GLOBAL_PALETTE;
            break;
            
        case VideoMode::URES:
            flags |= FEATURE_GPU_PRIMITIVES;
            flags |= FEATURE_ANTIALIASING;
            flags |= FEATURE_GRADIENTS;
            flags |= FEATURE_ALPHA_CHANNEL;
            flags |= FEATURE_DIRECT_COLOR;
            break;
            
        case VideoMode::LORES:
            flags |= FEATURE_GPU_PRIMITIVES;
            flags |= FEATURE_ALPHA_CHANNEL;
            flags |= FEATURE_PER_ROW_PALETTE;
            break;
            
        default:
            break;
    }
    
    return flags;
}

// ================================================================
// Buffer Info and Queries
// ================================================================

int VideoModeManager::getMaxBuffers() const {
    // All modes support 8 buffers (0-7)
    std::lock_guard<std::mutex> lock(m_mutex);
    
    switch (m_currentMode) {
        case VideoMode::LORES:
        case VideoMode::XRES:
        case VideoMode::WRES:
        case VideoMode::URES:
        case VideoMode::PRES:
            return 8;
        default:
            return 0;
    }
}

bool VideoModeManager::isValidBuffer(int bufferID) const {
    // All modes use 8 buffers (0-7)
    return bufferID >= 0 && bufferID < 8 && hasVideoMode();
}

// ================================================================
// Palette Management
// ================================================================

void VideoModeManager::setPaletteGlobal(int index, uint8_t r, uint8_t g, uint8_t b) {
    if (index < 16 || index > 255) return;  // Global palette is 16-255
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    switch (m_currentMode) {
        case VideoMode::XRES:
            st_xres_palette_global(index, r, g, b);
            break;
        case VideoMode::WRES:
            st_wres_palette_global(index, r, g, b);
            break;
        case VideoMode::PRES:
            st_pres_palette_global(index, r, g, b);
            break;
        case VideoMode::URES:
            // URES uses direct color, no palette
            break;
        case VideoMode::LORES:
            // LORES only has 16 colors, no global palette
            break;
        default:
            break;
    }
}

void VideoModeManager::setPaletteRow(int row, int index, uint8_t r, uint8_t g, uint8_t b) {
    if (index < 0 || index > 15) return;  // Per-row palette is 0-15
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    switch (m_currentMode) {
        case VideoMode::XRES:
            if (row >= 0 && row < 240)
                st_xres_palette_row(row, index, r, g, b);
            break;
        case VideoMode::WRES:
            if (row >= 0 && row < 240)
                st_wres_palette_row(row, index, r, g, b);
            break;
        case VideoMode::PRES:
            if (row >= 0 && row < 720)
                st_pres_palette_row(row, index, r, g, b);
            break;
        case VideoMode::URES:
            // URES uses direct color, no palette
            break;
        case VideoMode::LORES:
            // LORES uses palette_poke API instead of palette_row
            if (row >= 0 && row < 75) {
                uint32_t rgba = (r << 24) | (g << 16) | (b << 8) | 0xFF;
                st_lores_palette_poke(row, index, rgba);
            }
            break;
        default:
            break;
    }
}

bool VideoModeManager::getPalette(int index, uint8_t& r, uint8_t& g, uint8_t& b) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Use the unified C API function which handles all modes
    uint32_t rgba = st_video_get_palette(index);
    
    if (rgba == 0 && index != 0) {
        // Likely an error (unless color 0 is actually black)
        // Check if we're in a valid palette mode
        switch (m_currentMode) {
            case VideoMode::LORES:
            case VideoMode::XRES:
            case VideoMode::WRES:
            case VideoMode::PRES:
                break;
            case VideoMode::URES:
            default:
                return false;
        }
    }
    
    // Unpack ARGB (0xAARRGGBB) to RGB components
    r = (rgba >> 16) & 0xFF;
    g = (rgba >> 8) & 0xFF;
    b = rgba & 0xFF;
    
    return true;
}

bool VideoModeManager::getPaletteRow(int row, int index, uint8_t& r, uint8_t& g, uint8_t& b) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Use the unified C API function which handles all modes
    uint32_t rgba = st_video_get_palette_row(row, index);
    
    if (rgba == 0 && index != 0) {
        // Likely an error (unless color 0 is actually black)
        // Check if we're in a valid palette mode
        switch (m_currentMode) {
            case VideoMode::LORES:
            case VideoMode::XRES:
            case VideoMode::WRES:
            case VideoMode::PRES:
                break;
            case VideoMode::URES:
            default:
                return false;
        }
    }
    
    // Unpack ARGB (0xAARRGGBB) to RGB components
    r = (rgba >> 16) & 0xFF;
    g = (rgba >> 8) & 0xFF;
    b = rgba & 0xFF;
    
    return true;
}

void VideoModeManager::resetPaletteToDefault() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    switch (m_currentMode) {
        case VideoMode::XRES:
            st_xres_palette_reset();
            break;
        case VideoMode::WRES:
            st_wres_palette_reset();
            break;
        case VideoMode::PRES:
            st_pres_palette_reset();
            break;
        case VideoMode::URES:
            // URES uses direct color, no palette to reset
            break;
        case VideoMode::LORES:
            // TODO: LORES palette reset not yet implemented
            break;
        default:
            break;
    }
}

// ================================================================
// GPU Batch Operations
// ================================================================

void VideoModeManager::beginBatch() {
    requireActiveMode();
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    switch (m_currentMode) {
        case VideoMode::XRES:
        case VideoMode::WRES:
        case VideoMode::URES:
        case VideoMode::PRES:
            st_begin_blit_batch();
            break;
        case VideoMode::LORES:
            // LORES doesn't have GPU support, no-op
            break;
        default:
            break;
    }
}

void VideoModeManager::endBatch() {
    requireActiveMode();
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    switch (m_currentMode) {
        case VideoMode::XRES:
        case VideoMode::WRES:
        case VideoMode::URES:
        case VideoMode::PRES:
            st_end_blit_batch();
            break;
        case VideoMode::LORES:
            // LORES doesn't have GPU support, no-op
            break;
        default:
            break;
    }
}

void VideoModeManager::syncGPU() {
    requireActiveMode();
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    switch (m_currentMode) {
        case VideoMode::XRES:
        case VideoMode::WRES:
        case VideoMode::URES:
        case VideoMode::PRES:
            st_gpu_sync();
            break;
        case VideoMode::LORES:
            // LORES doesn't have GPU support, no-op
            break;
        default:
            break;
    }
}

// ================================================================
// Memory Queries
// ================================================================

size_t VideoModeManager::getMemoryPerBuffer() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    int width, height;
    getVideoModeResolution(m_currentMode, width, height);
    int bpp = getVideoModeBitsPerPixel(m_currentMode);
    
    return (width * height * bpp) / 8;
}

size_t VideoModeManager::getMemoryUsage() const {
    return getMemoryPerBuffer() * 8;  // All modes use 8 buffers
}

int VideoModeManager::getPixelCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    int width, height;
    getVideoModeResolution(m_currentMode, width, height);
    
    return width * height;
}

// ================================================================
// Advanced Drawing Primitives
// ================================================================

void VideoModeManager::polygon(const int* xPoints, const int* yPoints, int numPoints, uint32_t color) {
    requireActiveMode();
    
    // TODO: Implement polygon drawing
    // Need to add unified polygon API across all modes
    (void)xPoints;
    (void)yPoints;
    (void)numPoints;
    (void)color;
}

void VideoModeManager::polygonGPU(int bufferID, const int* xPoints, const int* yPoints, int numPoints, uint32_t color) {
    requireActiveMode();
    
    // TODO: Implement GPU polygon drawing
    (void)bufferID;
    (void)xPoints;
    (void)yPoints;
    (void)numPoints;
    (void)color;
}

void VideoModeManager::triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color) {
    requireActiveMode();
    
    // TODO: Implement triangle drawing as optimized 3-point polygon
    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
    (void)color;
}

void VideoModeManager::triangleGPU(int bufferID, int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color) {
    requireActiveMode();
    
    // TODO: Implement GPU triangle drawing
    (void)bufferID;
    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
    (void)color;
}

} // namespace SuperTerminal