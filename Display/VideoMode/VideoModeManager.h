//
// VideoModeManager.h
// SuperTerminal Framework
//
// Manages video mode state and provides unified drawing API
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef SUPERTERMINAL_VIDEO_MODE_MANAGER_H
#define SUPERTERMINAL_VIDEO_MODE_MANAGER_H

#include "VideoMode.h"
#include <memory>
#include <mutex>
#include <cstdint>

namespace SuperTerminal {

// Forward declarations
class DisplayManager;

/// VideoModeManager: Centralized video mode management and unified drawing API
///
/// Responsibilities:
/// - Track current video mode state
/// - Handle mode transitions
/// - Provide unified drawing API that dispatches to correct mode
/// - Thread-safe mode switching
///
/// Design Philosophy:
/// - User calls rect(), circle(), blit(), etc.
/// - Manager automatically dispatches to xres_rect(), ures_circle(), etc.
/// - Based on current active video mode
///
/// Example:
/// @code
///   videoModeManager->setVideoMode(VideoMode::XRES);
///   videoModeManager->rect(10, 10, 100, 50, 5);  // Calls xres_rect internally
///   
///   videoModeManager->setVideoMode(VideoMode::URES);
///   videoModeManager->rect(10, 10, 100, 50, 0xF00F);  // Calls ures_rect internally
/// @endcode
///
class VideoModeManager {
public:
    VideoModeManager();
    ~VideoModeManager();

    // Disable copy and move
    VideoModeManager(const VideoModeManager&) = delete;
    VideoModeManager& operator=(const VideoModeManager&) = delete;
    VideoModeManager(VideoModeManager&&) = delete;
    VideoModeManager& operator=(VideoModeManager&&) = delete;

    // ================================================================
    // Mode Management
    // ================================================================

    /// Set current video mode
    /// @param mode New video mode
    /// @return true if mode changed successfully
    bool setVideoMode(VideoMode mode);

    /// Get current video mode
    /// @return Current active video mode
    VideoMode getVideoMode() const;

    /// Check if a specific video mode is active
    /// @param mode Video mode to check
    /// @return true if specified mode is active
    bool isVideoModeActive(VideoMode mode) const;

    /// Disable video mode (return to text-only)
    void disableVideoMode();

    /// Check if any video mode is active
    /// @return true if video mode is active (not NONE)
    bool hasVideoMode() const;

    // ================================================================
    // Mode Queries (for backward compatibility)
    // ================================================================

    bool isLoResMode() const { return m_currentMode == VideoMode::LORES; }
    bool isXResMode() const { return m_currentMode == VideoMode::XRES; }
    bool isWResMode() const { return m_currentMode == VideoMode::WRES; }
    bool isUResMode() const { return m_currentMode == VideoMode::URES; }
    bool isPResMode() const { return m_currentMode == VideoMode::PRES; }

    // ================================================================
    // Resolution Info
    // ================================================================

    /// Get resolution of current video mode
    /// @param width Output width
    /// @param height Output height
    void getCurrentResolution(int& width, int& height) const;

    /// Get resolution of specific video mode
    /// @param mode Video mode
    /// @param width Output width
    /// @param height Output height
    static void getModeResolution(VideoMode mode, int& width, int& height);

    // ================================================================
    // Buffer Info and Queries (PHASE 1: NEW UNIFIED API)
    // ================================================================

    /// Get maximum number of buffers available in current mode
    /// @return Max buffer count (4 for XRES/WRES, 8 for PRES, etc.)
    int getMaxBuffers() const;

    /// Check if a buffer ID is valid for current mode
    /// @param bufferID Buffer ID to check
    /// @return true if buffer ID is within valid range for current mode
    bool isValidBuffer(int bufferID) const;

    /// Get current drawing buffer (same as getActiveBuffer for consistency)
    /// @return Current buffer ID
    int getCurrentBuffer() const;

    // ================================================================
    // Anti-Aliasing Support
    // ================================================================

    /// Enable or disable anti-aliasing for drawing operations
    /// @param enable true to enable AA, false to disable
    /// @return true if current mode supports AA functions, false otherwise
    /// @note When enabled, drawing functions will use AA variants if available
    ///       If AA functions are not available, falls back to non-AA versions
    bool enableAntialiasing(bool enable);

    /// Check if anti-aliasing is currently enabled
    /// @return true if AA is enabled
    bool isAntialiasingEnabled() const { return m_aaEnabled; }

    /// Check if current mode supports anti-aliasing functions
    /// @return true if AA functions exist for current mode
    bool supportsAntialiasing() const;

    /// Set line width for anti-aliased line drawing
    /// @param width Line width in pixels (1.0 = single pixel)
    void setLineWidth(float width) { m_lineWidth = width; }

    /// Get current line width setting
    /// @return Current line width in pixels
    float getLineWidth() const { return m_lineWidth; }

    // ================================================================
    // DisplayManager Integration
    // ================================================================

    /// Set DisplayManager reference (needed for drawing dispatch)
    /// @param displayManager Pointer to DisplayManager
    void setDisplayManager(DisplayManager* displayManager);

    // ================================================================
    // UNIFIED DRAWING API - Automatically dispatches to correct mode
    // ================================================================
    // These functions dispatch to mode-specific implementations:
    // - LORES: calls lores_* functions
    // - XRES:  calls xres_* functions
    // - WRES:  calls wres_* functions
    // - URES:  calls ures_* functions
    // ================================================================

    // --- Pixel Operations ---

    /// Set pixel at (x, y) with color
    /// Dispatches to: lores_pset, xres_pset, wres_pset, or ures_pset
    void pset(int x, int y, uint32_t color);

    /// Get pixel color at (x, y)
    /// Dispatches to: lores_pget, xres_pget, wres_pget, or ures_pget
    uint32_t pget(int x, int y);

    // --- Clear Operations ---

    /// Clear screen with color
    /// Dispatches to: lores_clear, xres_clear, wres_clear, or ures_clear
    void clear(uint32_t color);

    /// Clear GPU buffer with color
    /// Dispatches to: xres_clear_gpu, wres_clear_gpu, or ures_clear_gpu
    void clearGPU(int bufferID, uint32_t color);

    // --- Rectangle Operations ---

    /// Draw filled rectangle
    /// Dispatches to: lores_rect, xres_rect, wres_rect, or ures_rect
    void rect(int x, int y, int width, int height, uint32_t color);

    /// Draw filled rectangle (GPU-accelerated if available)
    /// Dispatches to: xres_rect_gpu, wres_rect_gpu, or ures_rect_gpu
    void rectGPU(int bufferID, int x, int y, int width, int height, uint32_t color);

    // --- Circle Operations ---

    /// Draw filled circle
    /// Dispatches to: lores_circle, xres_circle, wres_circle, or ures_circle
    void circle(int cx, int cy, int radius, uint32_t color);

    /// Draw filled circle (GPU-accelerated if available)
    /// Dispatches to: xres_circle_gpu, wres_circle_gpu, or ures_circle_gpu
    /// If AA is enabled and supported: uses xres_circle_fill_aa, wres_circle_fill_aa, or ures_circle_fill_aa
    void circleGPU(int bufferID, int cx, int cy, int radius, uint32_t color);

    // --- Line Operations ---

    /// Draw line from (x0, y0) to (x1, y1)
    /// Dispatches to: lores_line, xres_line, wres_line, or ures_line
    void line(int x0, int y0, int x1, int y1, uint32_t color);

    /// Draw line (GPU-accelerated if available)
    /// Dispatches to: xres_line_gpu, wres_line_gpu, or ures_line_gpu
    /// If AA is enabled and supported: uses xres_line_aa, wres_line_aa, or ures_line_aa
    void lineGPU(int bufferID, int x0, int y0, int x1, int y1, uint32_t color);

    // --- Blit Operations ---

    /// Blit rectangular region within current buffer
    /// Dispatches to: lores_blit, xres_blit, wres_blit, or ures_blit_from
    void blit(int srcX, int srcY, int width, int height, int dstX, int dstY);

    /// Blit with transparency (skip color 0 or alpha=0)
    /// Dispatches to: lores_blit_trans, xres_blit_trans, wres_blit_trans, or ures_blit_from_trans
    void blitTrans(int srcX, int srcY, int width, int height, int dstX, int dstY);

    /// Blit from source buffer to destination buffer
    /// Dispatches to: xres_blit_from, wres_blit_from, or ures_blit_from
    void blitFrom(int srcBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY);

    /// Blit from source buffer with transparency
    /// Dispatches to: xres_blit_from_trans, wres_blit_from_trans, or ures_blit_from_trans
    void blitFromTrans(int srcBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY);

    /// GPU-accelerated blit between buffers
    /// Dispatches to: xres_blit_gpu, wres_blit_gpu, or ures_blit_copy_gpu
    void blitGPU(int srcBufferID, int dstBufferID, int srcX, int srcY, 
                 int width, int height, int dstX, int dstY);

    /// GPU-accelerated transparent blit
    /// Dispatches to: xres_blit_trans_gpu, wres_blit_trans_gpu, or ures_blit_transparent_gpu
    void blitTransGPU(int srcBufferID, int dstBufferID, int srcX, int srcY,
                      int width, int height, int dstX, int dstY);

    // --- Buffer Operations ---

    /// Set active buffer for drawing
    /// Dispatches to: lores_buffer, xres_buffer, wres_buffer, or ures_buffer
    void setActiveBuffer(int bufferID);

    /// Get current active buffer
    /// Dispatches to: mode-specific get active buffer
    int getActiveBuffer() const;

    /// Sync buffer with GPU operations
    /// Dispatches to: xres_sync, wres_sync, or ures_sync
    void syncBuffer(int bufferID);
    
    /// Swap buffers (for double-buffering)
    /// Dispatches to: xres_swap, wres_swap, or ures_swap
    void swapBuffers(int bufferA, int bufferB);

    // --- URES-specific Operations (only work in URES mode) ---

    /// Fill rectangle with gradient (URES only)
    void rectGradient(int x, int y, int width, int height,
                     uint32_t topLeft, uint32_t topRight,
                     uint32_t bottomLeft, uint32_t bottomRight);

    /// Fill circle with radial gradient (URES only)
    void circleGradient(int cx, int cy, int radius,
                       uint32_t centerColor, uint32_t edgeColor);

    // ================================================================
    // Palette Management (PHASE 1: NEW UNIFIED API)
    // ================================================================

    /// Set global palette color (indices 16-255)
    /// Automatically dispatches to correct mode-specific palette function
    /// @param index Palette index (16-255 for global palette)
    /// @param r Red component (0-255)
    /// @param g Green component (0-255)
    /// @param b Blue component (0-255)
    /// @note For indices 0-15, use setPaletteRow() instead
    void setPaletteGlobal(int index, uint8_t r, uint8_t g, uint8_t b);

    /// Set per-row palette color (indices 0-15)
    /// Automatically dispatches to correct mode-specific palette function
    /// @param row Row index (0-239 for XRES/WRES, 0-719 for PRES)
    /// @param index Color index within row (0-15)
    /// @param r Red component (0-255)
    /// @param g Green component (0-255)
    /// @param b Blue component (0-255)
    void setPaletteRow(int row, int index, uint8_t r, uint8_t g, uint8_t b);

    /// Get palette color (reads from current mode's palette)
    /// @param index Palette index (0-255)
    /// @param r Output red component
    /// @param g Output green component
    /// @param b Output blue component
    /// @return true if successfully retrieved, false if not supported or out of range
    bool getPalette(int index, uint8_t& r, uint8_t& g, uint8_t& b) const;

    /// Get per-row palette color (reads from current mode's palette)
    /// @param row Row index (0-239 for XRES/WRES, 0-719 for PRES, 0-299 for LORES)
    /// @param index Color index within row (0-15)
    /// @param r Output red component
    /// @param g Output green component
    /// @param b Output blue component
    /// @return true if successfully retrieved, false if not supported or out of range
    bool getPaletteRow(int row, int index, uint8_t& r, uint8_t& g, uint8_t& b) const;

    /// Reset palette to default colors for current mode
    void resetPaletteToDefault();

    // ================================================================
    // GPU Batch Operations (PHASE 1: NEW UNIFIED API)
    // ================================================================

    /// Begin batching GPU drawing commands for optimal performance
    /// All GPU operations between beginBatch() and endBatch() will be
    /// submitted as a single command buffer (10-50x faster)
    /// @note Must call endBatch() before rendering
    /// @note Not supported in LORES mode (no-op)
    void beginBatch();

    /// End batching GPU drawing commands and submit to GPU
    /// @note Must be paired with beginBatch()
    void endBatch();

    /// Synchronize GPU operations (wait for completion)
    /// Automatically dispatches to correct mode
    void syncGPU();

    // ================================================================
    // Mode-Specific Features
    // ================================================================

    /// Get color depth of current mode
    int getColorDepth() const;

    /// Check if current mode uses palette
    bool usesPalette() const;

    /// Check if current mode supports alpha channel
    bool supportsAlpha() const;

    /// Get feature flags for current mode
    /// @return Bitmask of supported features (see FeatureFlags enum)
    uint32_t getFeatureFlags() const;

    /// Feature flags for capability querying
    enum FeatureFlags : uint32_t {
        FEATURE_NONE            = 0,
        FEATURE_GPU_PRIMITIVES  = 1 << 0,  ///< GPU-accelerated drawing
        FEATURE_ANTIALIASING    = 1 << 1,  ///< Anti-aliased rendering
        FEATURE_GRADIENTS       = 1 << 2,  ///< Gradient fills (URES only)
        FEATURE_ALPHA_CHANNEL   = 1 << 3,  ///< Per-pixel alpha
        FEATURE_PER_ROW_PALETTE = 1 << 4,  ///< Per-scanline palette
        FEATURE_GLOBAL_PALETTE  = 1 << 5,  ///< Global palette colors
        FEATURE_DIRECT_COLOR    = 1 << 6,  ///< Direct color (no palette)
    };

    // ================================================================
    // Memory Queries (PHASE 2)
    // ================================================================

    /// Get memory used per buffer in current mode
    /// @return Bytes per buffer
    size_t getMemoryPerBuffer() const;

    /// Get total memory used by all buffers in current mode
    /// @return Total bytes used
    size_t getMemoryUsage() const;

    /// Get total pixel count per buffer in current mode
    /// @return Number of pixels per buffer
    int getPixelCount() const;

    // ================================================================
    // Advanced Drawing Primitives (PHASE 2)
    // ================================================================

    /// Draw filled polygon (CPU)
    /// @param xPoints Array of X coordinates
    /// @param yPoints Array of Y coordinates
    /// @param numPoints Number of points
    /// @param color Fill color
    void polygon(const int* xPoints, const int* yPoints, int numPoints, uint32_t color);

    /// Draw filled polygon (GPU-accelerated)
    /// @param bufferID Target buffer
    /// @param xPoints Array of X coordinates
    /// @param yPoints Array of Y coordinates
    /// @param numPoints Number of points
    /// @param color Fill color
    void polygonGPU(int bufferID, const int* xPoints, const int* yPoints, int numPoints, uint32_t color);

    /// Draw filled triangle (CPU)
    /// @param x0 First vertex X
    /// @param y0 First vertex Y
    /// @param x1 Second vertex X
    /// @param y1 Second vertex Y
    /// @param x2 Third vertex X
    /// @param y2 Third vertex Y
    /// @param color Fill color
    void triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color);

    /// Draw filled triangle (GPU-accelerated)
    /// @param bufferID Target buffer
    /// @param x0 First vertex X
    /// @param y0 First vertex Y
    /// @param x1 Second vertex X
    /// @param y1 Second vertex Y
    /// @param x2 Third vertex X
    /// @param y2 Third vertex Y
    /// @param color Fill color
    void triangleGPU(int bufferID, int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color);

private:
    VideoMode m_currentMode;
    DisplayManager* m_displayManager;
    mutable std::mutex m_mutex;
    
    // Double-buffering state
    int m_frontBuffer;  // Buffer being displayed (0 or 1)
    int m_backBuffer;   // Buffer being drawn to (0 or 1)
    
    // Anti-aliasing state
    bool m_aaEnabled;   // Whether AA is enabled
    float m_lineWidth;  // Line width for AA rendering (default 1.0)

    // Helper to check if mode is active (throws error if not)
    void requireActiveMode() const;

public:
    // ================================================================
    // Buffer Management (Double-Buffering Support)
    // ================================================================
    
    /// Get the current back buffer (for drawing)
    /// @return Back buffer ID (0 or 1)
    int getBackBuffer() const { return m_backBuffer; }
    
    /// Get the current front buffer (being displayed)
    /// @return Front buffer ID (0 or 1)
    int getFrontBuffer() const { return m_frontBuffer; }
    
    /// Swap front and back buffers (for double-buffering)
    /// This is called by swapBuffers() internally
    void flipBuffers();
};

} // namespace SuperTerminal

#endif // SUPERTERMINAL_VIDEO_MODE_MANAGER_H