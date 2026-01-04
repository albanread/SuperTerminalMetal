//
// st_api_display.cpp
// SuperTerminal v2 - C API Display Implementation
//
// Text and graphics API functions
//

#include "superterminal_api.h"
#include "st_api_context.h"
#include "st_api_circles.h"
#include "st_sprite_drawing_utils.h"
#include "../Display/TextGrid.h"
#include "../Display/GraphicsLayer.h"
#include "../Display/DisplayManager.h"
#include "../Display/TextDisplayManager.h"
#include "../Display/VideoMode/VideoModeManager.h"
#include "../Display/LoResPaletteManager.h"
#include "../Display/XResPaletteManager.h"
#include "../Display/WResPaletteManager.h"
#include "../Display/PResPaletteManager.h"
#include "../Display/LoResBuffer.h"
#include "../Display/UResBuffer.h"
#include "../Display/XResBuffer.h"
#include "../Display/WResBuffer.h"
#include "../Display/PResBuffer.h"
#include "../Metal/MetalRenderer.h"

using namespace SuperTerminal;
using namespace STApi;

// =============================================================================
// Display API - Text Layer
// =============================================================================

ST_API void st_text_putchar(int x, int y, uint32_t character, STColor fg, STColor bg) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.textGrid(), "TextGrid");

    ST_CONTEXT.textGrid()->putChar(x, y, character, fg, bg);
}

ST_API void st_text_put(int x, int y, const char* text, STColor fg, STColor bg) {
    if (!text) {
        ST_SET_ERROR("Text string is null");
        return;
    }

    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.textGrid(), "TextGrid");

    ST_CONTEXT.textGrid()->putString(x, y, text, fg, bg);
}

ST_API void st_text_clear(void) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.textGrid(), "TextGrid");

    ST_CONTEXT.textGrid()->clear();
}

ST_API void st_text_clear_region(int x, int y, int width, int height) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.textGrid(), "TextGrid");

    // Clear by filling with spaces
    for (int row = y; row < y + height; ++row) {
        for (int col = x; col < x + width; ++col) {
            ST_CONTEXT.textGrid()->putChar(col, row, ' ', 0xFFFFFFFF, 0xFF000000);
        }
    }
}

ST_API void st_text_set_size(int width, int height) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.textGrid(), "TextGrid");

    ST_CONTEXT.textGrid()->resize(width, height);
}

ST_API void st_text_get_size(int* width, int* height) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.textGrid(), "TextGrid");

    if (width) {
        *width = ST_CONTEXT.textGrid()->getWidth();
    }
    if (height) {
        *height = ST_CONTEXT.textGrid()->getHeight();
    }
}

ST_API void st_text_scroll(int lines) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.textGrid(), "TextGrid");

    ST_CONTEXT.textGrid()->scroll(lines);
}

// =============================================================================
// Free-form Text Display API
// =============================================================================

ST_API int st_text_display_at(float x, float y, const char* text,
                              float scale_x, float scale_y, float rotation,
                              STColor color, STTextAlignment alignment, int layer) {
    if (!text) {
        ST_SET_ERROR("Text string is null");
        return -1;
    }

    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.textDisplay(), "TextDisplayManager", -1);

    SuperTerminal::TextAlignment align = static_cast<SuperTerminal::TextAlignment>(alignment);
    return ST_CONTEXT.textDisplay()->displayTextAt(x, y, text, scale_x, scale_y, rotation,
                                                    color, align, layer);
}

ST_API int st_text_display_shear(float x, float y, const char* text,
                                 float scale_x, float scale_y, float rotation,
                                 float shear_x, float shear_y,
                                 STColor color, STTextAlignment alignment, int layer) {
    if (!text) {
        ST_SET_ERROR("Text string is null");
        return -1;
    }

    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.textDisplay(), "TextDisplayManager", -1);

    SuperTerminal::TextAlignment align = static_cast<SuperTerminal::TextAlignment>(alignment);
    return ST_CONTEXT.textDisplay()->displayTextAtWithShear(x, y, text, scale_x, scale_y, rotation,
                                                             shear_x, shear_y, color, align, layer);
}

ST_API int st_text_display_with_effects(float x, float y, const char* text,
                                        float scale_x, float scale_y, float rotation,
                                        STColor color, STTextAlignment alignment, int layer,
                                        STTextEffect effect, STColor effect_color,
                                        float effect_intensity, float effect_size) {
    if (!text) {
        ST_SET_ERROR("Text string is null");
        return -1;
    }

    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.textDisplay(), "TextDisplayManager", -1);

    SuperTerminal::TextAlignment align = static_cast<SuperTerminal::TextAlignment>(alignment);
    SuperTerminal::TextEffect textEffect = static_cast<SuperTerminal::TextEffect>(effect);
    return ST_CONTEXT.textDisplay()->displayTextWithEffects(x, y, text, scale_x, scale_y, rotation,
                                                             color, align, layer, textEffect,
                                                             effect_color, effect_intensity, effect_size);
}

ST_API int st_text_update_item(int item_id, const char* text, float x, float y) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.textDisplay(), "TextDisplayManager", 0);

    std::string textStr = text ? text : "";
    return ST_CONTEXT.textDisplay()->updateTextItem(item_id, textStr, x, y) ? 1 : 0;
}

ST_API int st_text_remove_item(int item_id) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.textDisplay(), "TextDisplayManager", 0);

    return ST_CONTEXT.textDisplay()->removeTextItem(item_id) ? 1 : 0;
}

ST_API void st_text_clear_displayed(void) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.textDisplay(), "TextDisplayManager");

    ST_CONTEXT.textDisplay()->clearDisplayedText();
}

ST_API int st_text_set_item_visible(int item_id, int visible) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.textDisplay(), "TextDisplayManager", 0);

    return ST_CONTEXT.textDisplay()->setTextItemVisible(item_id, visible != 0) ? 1 : 0;
}

ST_API int st_text_set_item_layer(int item_id, int layer) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.textDisplay(), "TextDisplayManager", 0);

    return ST_CONTEXT.textDisplay()->setTextItemLayer(item_id, layer) ? 1 : 0;
}

ST_API int st_text_set_item_color(int item_id, uint32_t color) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.textDisplay(), "TextDisplayManager", 0);

    return ST_CONTEXT.textDisplay()->setTextItemColor(item_id, color) ? 1 : 0;
}

ST_API int st_text_get_item_count(void) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.textDisplay(), "TextDisplayManager", 0);

    return static_cast<int>(ST_CONTEXT.textDisplay()->getTextItemCount());
}

ST_API int st_text_get_visible_count(void) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.textDisplay(), "TextDisplayManager", 0);

    return static_cast<int>(ST_CONTEXT.textDisplay()->getVisibleTextItemCount());
}

// =============================================================================
// Display API - Chunky Pixel Graphics (Sextants)
// =============================================================================
// Uses Unicode sextant characters (U+1FB00-U+1FB3F) to create a low-res
// graphics system. Each character cell = 2x3 grid of sub-pixels.
// Each sub-pixel can be one of 16 RGBI palette colors.

// Helper: Pack 6 RGBI color indices into foreground color
static uint32_t pack_sextant_colors(const uint8_t colors[6], uint8_t alpha) {
    // Bit index mapping: bit_index = sub_y * 2 + sub_x
    //   0 = top-left,    1 = top-right      (top row)
    //   2 = mid-left,    3 = mid-right      (middle row)
    //   4 = bottom-left, 5 = bottom-right   (bottom row)
    // Pack into RGB bytes:
    //   R byte: colors[0] (high nibble), colors[1] (low nibble)
    //   G byte: colors[2] (high nibble), colors[3] (low nibble)
    //   B byte: colors[4] (high nibble), colors[5] (low nibble)
    uint8_t r = (colors[0] << 4) | colors[1];
    uint8_t g = (colors[2] << 4) | colors[3];
    uint8_t b = (colors[4] << 4) | colors[5];
    return (r << 24) | (g << 16) | (b << 8) | alpha;
}

ST_API void st_lores_pset(int pixel_x, int pixel_y, uint8_t color_index, STColor background) {
    ST_LOCK;

    // Get active LORES buffer from display manager
    auto display = ST_CONTEXT.display();
    if (!display) {
        ST_SET_ERROR("DisplayManager not initialized");
        return;
    }

    int activeBufferID = display->getActiveLoResBuffer();
    auto loResBuffer = display->getLoResBuffer(activeBufferID);
    if (!loResBuffer) {
        ST_SET_ERROR("LoResBuffer not initialized");
        return;
    }

    // Set pixel in buffer (buffer handles bounds checking and clamping)
    loResBuffer->setPixel(pixel_x, pixel_y, color_index);
}

ST_API void st_lores_line(int x1, int y1, int x2, int y2, uint8_t color_index, STColor background) {
    ST_LOCK;

    // Get active LORES buffer
    auto display = ST_CONTEXT.display();
    if (!display) {
        ST_SET_ERROR("DisplayManager not initialized");
        return;
    }

    int activeBufferID = display->getActiveLoResBuffer();
    auto loResBuffer = display->getLoResBuffer(activeBufferID);

    if (!loResBuffer) {
        ST_SET_ERROR("LoResBuffer not initialized");
        return;
    }

    // Helper to set pixel with alpha (0.0 = transparent, 1.0 = opaque)
    auto setPixelAlpha = [&](int x, int y, uint8_t color, float alpha) {
        if (alpha <= 0.01f) return;  // Skip fully transparent

        // Convert alpha from 0.0-1.0 to 0-15
        uint8_t alphaValue = static_cast<uint8_t>(alpha * 15.0f + 0.5f);
        if (alphaValue > 15) alphaValue = 15;
        if (alphaValue == 0) alphaValue = 1;  // Ensure at least some visibility

        loResBuffer->setPixelAlpha(x, y, color, alphaValue);
    };

    // Special case: perfectly horizontal or vertical lines (no anti-aliasing needed)
    if (y1 == y2) {
        // Horizontal line - draw solid
        int x_start = std::min(x1, x2);
        int x_end = std::max(x1, x2);
        for (int x = x_start; x <= x_end; x++) {
            loResBuffer->setPixel(x, y1, color_index);
        }
        return;
    }
    if (x1 == x2) {
        // Vertical line - draw solid
        int y_start = std::min(y1, y2);
        int y_end = std::max(y1, y2);
        for (int y = y_start; y <= y_end; y++) {
            loResBuffer->setPixel(x1, y, color_index);
        }
        return;
    }

    // Xiaolin Wu's anti-aliased line algorithm
    bool steep = abs(y2 - y1) > abs(x2 - x1);

    if (steep) {
        std::swap(x1, y1);
        std::swap(x2, y2);
    }
    if (x1 > x2) {
        std::swap(x1, x2);
        std::swap(y1, y2);
    }

    float dx = static_cast<float>(x2 - x1);
    float dy = static_cast<float>(y2 - y1);
    float gradient = (dx == 0.0f) ? 1.0f : dy / dx;

    // Handle first endpoint
    int xend = static_cast<int>(std::round(x1));
    float yend = y1 + gradient * (xend - x1);
    float xgap = 1.0f - std::fmod(x1 + 0.5f, 1.0f);
    int xpxl1 = xend;
    int ypxl1 = static_cast<int>(yend);

    if (steep) {
        setPixelAlpha(ypxl1, xpxl1, color_index, (1.0f - std::fmod(yend, 1.0f)) * xgap);
        setPixelAlpha(ypxl1 + 1, xpxl1, color_index, std::fmod(yend, 1.0f) * xgap);
    } else {
        setPixelAlpha(xpxl1, ypxl1, color_index, (1.0f - std::fmod(yend, 1.0f)) * xgap);
        setPixelAlpha(xpxl1, ypxl1 + 1, color_index, std::fmod(yend, 1.0f) * xgap);
    }

    float intery = yend + gradient;

    // Handle second endpoint
    xend = static_cast<int>(std::round(x2));
    yend = y2 + gradient * (xend - x2);
    xgap = std::fmod(x2 + 0.5f, 1.0f);
    int xpxl2 = xend;
    int ypxl2 = static_cast<int>(yend);

    if (steep) {
        setPixelAlpha(ypxl2, xpxl2, color_index, (1.0f - std::fmod(yend, 1.0f)) * xgap);
        setPixelAlpha(ypxl2 + 1, xpxl2, color_index, std::fmod(yend, 1.0f) * xgap);
    } else {
        setPixelAlpha(xpxl2, ypxl2, color_index, (1.0f - std::fmod(yend, 1.0f)) * xgap);
        setPixelAlpha(xpxl2, ypxl2 + 1, color_index, std::fmod(yend, 1.0f) * xgap);
    }

    // Main loop - draw line between endpoints
    for (int x = xpxl1 + 1; x < xpxl2; x++) {
        int y = static_cast<int>(intery);

        if (steep) {
            setPixelAlpha(y, x, color_index, 1.0f - std::fmod(intery, 1.0f));
            setPixelAlpha(y + 1, x, color_index, std::fmod(intery, 1.0f));
        } else {
            setPixelAlpha(x, y, color_index, 1.0f - std::fmod(intery, 1.0f));
            setPixelAlpha(x, y + 1, color_index, std::fmod(intery, 1.0f));
        }

        intery += gradient;
    }
}

ST_API void st_lores_rect(int x, int y, int width, int height, uint8_t color_index, STColor background) {
    // Draw rectangle outline using hline and vline for solid lines
    // Top and bottom edges (excluding right vertical edge area to avoid overlap)
    st_lores_hline(x, y, width - 2, color_index, background);                    // Top
    st_lores_hline(x, y + height - 1, width - 2, color_index, background);       // Bottom

    // Left and right edges (2 pixels wide for visibility, full height)
    if (height > 0) {
        // Left edge - 2 pixels wide
        st_lores_vline(x, y, height, color_index, background);
        st_lores_vline(x + 1, y, height, color_index, background);

        // Right edge - 2 pixels wide
        st_lores_vline(x + width - 2, y, height, color_index, background);
        st_lores_vline(x + width - 1, y, height, color_index, background);
    }
}

ST_API void st_lores_fillrect(int x, int y, int width, int height, uint8_t color_index, STColor background) {
    ST_LOCK;

    // Get active LORES buffer from display manager
    auto display = ST_CONTEXT.display();
    if (!display) {
        ST_SET_ERROR("DisplayManager not initialized");
        return;
    }

    int activeBufferID = display->getActiveLoResBuffer();
    auto loResBuffer = display->getLoResBuffer(activeBufferID);
    if (!loResBuffer) {
        ST_SET_ERROR("LoResBuffer not initialized");
        return;
    }

    // Fill rectangle directly in buffer
    for (int py = y; py < y + height; py++) {
        for (int px = x; px < x + width; px++) {
            loResBuffer->setPixel(px, py, color_index);
        }
    }
}

ST_API void st_lores_hline(int x, int y, int width, uint8_t color_index, STColor background) {
    ST_LOCK;

    // Get active LORES buffer from display manager
    auto display = ST_CONTEXT.display();
    if (!display) {
        ST_SET_ERROR("DisplayManager not initialized");
        return;
    }

    int activeBufferID = display->getActiveLoResBuffer();
    auto loResBuffer = display->getLoResBuffer(activeBufferID);
    if (!loResBuffer) {
        ST_SET_ERROR("LoResBuffer not initialized");
        return;
    }

    // Draw horizontal line directly in buffer
    for (int px = x; px < x + width; px++) {
        loResBuffer->setPixel(px, y, color_index);
    }
}

ST_API void st_lores_vline(int x, int y, int height, uint8_t color_index, STColor background) {
    ST_LOCK;

    // Get active LORES buffer from display manager
    auto display = ST_CONTEXT.display();
    if (!display) {
        ST_SET_ERROR("DisplayManager not initialized");
        return;
    }

    int activeBufferID = display->getActiveLoResBuffer();
    auto loResBuffer = display->getLoResBuffer(activeBufferID);
    if (!loResBuffer) {
        ST_SET_ERROR("LoResBuffer not initialized");
        return;
    }

    // Draw vertical line directly in buffer
    for (int py = y; py < y + height; py++) {
        loResBuffer->setPixel(x, py, color_index);
    }
}

ST_API void st_lores_clear(STColor background) {
    ST_LOCK;

    // Get active LORES buffer from display manager
    auto display = ST_CONTEXT.display();
    if (!display) {
        ST_SET_ERROR("DisplayManager not initialized");
        return;
    }

    int activeBufferID = display->getActiveLoResBuffer();
    auto loResBuffer = display->getLoResBuffer(activeBufferID);
    if (!loResBuffer) {
        ST_SET_ERROR("LoResBuffer not initialized");
        return;
    }

    // Clear buffer to color 0 (black)
    loResBuffer->clear(0);
}

ST_API void st_lores_resolution(int* width, int* height) {
    ST_LOCK;

    // Get active LORES buffer from display manager (use buffer 0 for resolution query)
    auto display = ST_CONTEXT.display();
    if (!display) {
        ST_SET_ERROR("DisplayManager not initialized");
        return;
    }

    auto loResBuffer = display->getLoResBuffer(0);
    if (!loResBuffer) {
        ST_SET_ERROR("LoResBuffer not initialized");
        return;
    }

    if (width) {
        *width = loResBuffer->getWidth();
    }
    if (height) {
        *height = loResBuffer->getHeight();
    }
}

// =============================================================================
// Display API - LORES Mode Management
// =============================================================================

// Store the last set mode number to avoid enum/int mismatch
static int g_current_mode = 0;

ST_API int st_mode_get(void) {
    return g_current_mode;
}

ST_API void st_mode(int mode) {
    ST_LOCK;
    
    // Store the mode number so st_mode_get can return it
    g_current_mode = mode;
    
    printf("[ST_MODE DEBUG] st_mode called with mode=%d (0=TEXT 1=LORES 2=MIDRES 3=HIRES 4=URES 5=XRES 6=WRES 7=PRES)\n", mode);

    // Get display manager
    auto display = ST_CONTEXT.display();
    if (!display) {
        ST_SET_ERROR("DisplayManager not initialized");
        return;
    }

    // Get VideoModeManager
    auto videoModeManager = display->getVideoModeManager();
    if (!videoModeManager) {
        ST_SET_ERROR("VideoModeManager not initialized");
        return;
    }

    // Get both front and back buffers
    auto frontBuffer = display->getLoResBuffer(0);
    auto backBuffer = display->getLoResBuffer(1);
    if (!frontBuffer || !backBuffer) {
        ST_SET_ERROR("LoResBuffer not initialized");
        return;
    }

    // =========================================================================
    // CRITICAL: Cleanup current graphics mode before switching
    // =========================================================================
    // When switching modes (especially to TEXT mode), we must properly clean up
    // the current graphics mode to avoid:
    // - Incomplete GPU batch operations
    // - Dangling GPU command buffers
    // - Resource leaks
    // - Visual artifacts
    // - Crashes from accessing freed resources
    //
    // This is particularly important when scripts call text_mode() to exit:
    // - URES mode may have active batch blits
    // - XRES/WRES modes may have pending GPU operations
    // - All modes need GPU sync before buffer/texture cleanup
    
    VideoMode currentMode = videoModeManager->getVideoMode();
    
    // ALWAYS end any active batch operations before switching modes
    // This is critical when:
    // 1. Switching from graphics mode to text mode (exiting)
    // 2. Switching from one graphics mode to another (mode change)
    // 3. Re-entering the same graphics mode (script restart)
    auto renderer = display->getRenderer();
    if (renderer) {
        // Step 1: End any active batch operations
        // If begin_blit_batch() was called but not end_blit_batch(),
        // we must end it now to submit the command buffer
        // This prevents "Already in blit batch" warnings on script restart
        if (renderer->isInBlitBatch()) {
            renderer->endBlitBatch();
        }
        
        // Step 2: Sync GPU to ensure all operations complete (only if leaving graphics mode)
        // This blocks until GPU finishes all pending work:
        // - Blits, draws, clears, etc.
        // - Ensures textures/buffers are in a stable state
        // - Prevents race conditions during mode switch
        if (currentMode != VideoMode::NONE) {
            renderer->syncGPU();
        }
    }

    // Switch display mode and resolution
    // 0 = TEXT, 1 = LORES (160×75), 2 = MIDRES (320×150), 3 = HIRES (640×300), 4 = URES (1280×720), 5 = XRES (320×240), 6 = WRES (432×240), 7 = PRES (1280×720, 256-color palette)
    if (mode == 0) {
        printf("[ST_MODE DEBUG] Setting TEXT mode\n");
        // TEXT mode - disable all graphics modes
        display->setLoResMode(false);
        display->setUResMode(false);
        display->setXResMode(false);
        display->setWResMode(false);
        display->setPResMode(false);
        videoModeManager->setVideoMode(VideoMode::NONE);
    } else if (mode == 1) {
        // LORES mode: 160×75
        frontBuffer->resize(LoResBuffer::LORES_WIDTH, LoResBuffer::LORES_HEIGHT);
        backBuffer->resize(LoResBuffer::LORES_WIDTH, LoResBuffer::LORES_HEIGHT);
        display->setLoResMode(true);
        display->setUResMode(false);
        display->setXResMode(false);
        display->setWResMode(false);
        display->setPResMode(false);
        videoModeManager->setVideoMode(VideoMode::LORES);
    } else if (mode == 2) {
        // MIDRES mode: 320×150
        frontBuffer->resize(LoResBuffer::MIDRES_WIDTH, LoResBuffer::MIDRES_HEIGHT);
        backBuffer->resize(LoResBuffer::MIDRES_WIDTH, LoResBuffer::MIDRES_HEIGHT);
        display->setLoResMode(true);
        display->setUResMode(false);
        display->setXResMode(false);
        display->setWResMode(false);
        display->setPResMode(false);
        videoModeManager->setVideoMode(VideoMode::LORES);
    } else if (mode == 3) {
        // HIRES mode: 640×300
        frontBuffer->resize(LoResBuffer::HIRES_WIDTH, LoResBuffer::HIRES_HEIGHT);
        backBuffer->resize(LoResBuffer::HIRES_WIDTH, LoResBuffer::HIRES_HEIGHT);
        display->setLoResMode(true);
        display->setUResMode(false);
        display->setXResMode(false);
        display->setWResMode(false);
        display->setPResMode(false);
        videoModeManager->setVideoMode(VideoMode::LORES);
    } else if (mode == 4) {
        // URES mode: 1280×720 direct color (no buffer resize needed, uses separate URES buffers)
        display->setLoResMode(false);
        display->setUResMode(true);
        display->setXResMode(false);
        display->setWResMode(false);
        display->setPResMode(false);
        videoModeManager->setVideoMode(VideoMode::URES);
    } else if (mode == 5) {
        printf("[ST_MODE DEBUG] Setting XRES mode (320x240 indexed)\n");
        // XRES mode: 320×240 with 256-color palette (Mode X inspired, uses separate XRES buffers)
        display->setLoResMode(false);
        display->setUResMode(false);
        display->setXResMode(true);
        display->setWResMode(false);
        display->setPResMode(false);
        videoModeManager->setVideoMode(VideoMode::XRES);
    } else if (mode == 6) {
        // WRES mode: 432×240 with 256-color palette (Wide mode for 16:9 displays, uses separate WRES buffers)
        display->setLoResMode(false);
        display->setUResMode(false);
        display->setXResMode(false);
        display->setWResMode(false);
        display->setPResMode(false);
        display->setWResMode(true);
        videoModeManager->setVideoMode(VideoMode::WRES);
    } else if (mode == 7) {
        // PRES mode: 1280×720 with 256-color palette (Premium Resolution, uses separate PRES buffers)
        display->setLoResMode(false);
        display->setUResMode(false);
        display->setXResMode(false);
        display->setWResMode(false);
        display->setPResMode(true);
        videoModeManager->setVideoMode(VideoMode::PRES);
    } else {
        printf("[ST_MODE DEBUG] ERROR: Invalid mode %d\n", mode);
        ST_SET_ERROR("Invalid mode (0=TEXT, 1=LORES, 2=MIDRES, 3=HIRES, 4=URES, 5=XRES, 6=WRES, 7=PRES)");
    }
    
    printf("[ST_MODE DEBUG] Mode set complete, current mode should be: %d\n", mode);
}

// =============================================================================
// Display API - LORES Palette Management
// =============================================================================

ST_API void st_lores_palette_set(const char* mode) {
    if (!mode) {
        ST_SET_ERROR("Mode string is null");
        return;
    }

    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.loresPalette(), "LoResPaletteManager");

    std::string modeStr(mode);
    if (modeStr == "IBM" || modeStr == "ibm") {
        ST_CONTEXT.loresPalette()->setAllPalettes(LoResPaletteType::IBM);
    } else if (modeStr == "C64" || modeStr == "c64") {
        ST_CONTEXT.loresPalette()->setAllPalettes(LoResPaletteType::C64);
    } else {
        ST_SET_ERROR("Invalid palette mode (use 'IBM' or 'C64')");
    }
}

ST_API void st_lores_palette_poke(int row, int index, uint32_t rgba) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.loresPalette(), "LoResPaletteManager");

    if (row < 0 || row > 299) {
        ST_SET_ERROR("Palette row must be 0-299");
        return;
    }
    if (index < 0 || index > 15) {
        ST_SET_ERROR("Palette index must be 0-15");
        return;
    }

    ST_CONTEXT.loresPalette()->setPaletteEntry(row, index, rgba);
}

ST_API uint32_t st_lores_palette_peek(int row, int index) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.loresPalette(), "LoResPaletteManager", 0);

    if (row < 0 || row > 299 || index < 0 || index > 15) {
        ST_SET_ERROR("Invalid palette row (0-299) or index (0-15)");
        return 0;
    }

    return ST_CONTEXT.loresPalette()->getPaletteEntry(row, index);
}

// =============================================================================
// Display API - LORES Buffer Management
// =============================================================================

ST_API void st_lores_buffer(int bufferID) {
    ST_LOCK;

    auto display = ST_CONTEXT.display();
    if (!display) {
        ST_SET_ERROR("DisplayManager not initialized");
        return;
    }

    if (bufferID < 0 || bufferID > 7) {
        ST_SET_ERROR("Invalid buffer ID (must be 0-7)");
        return;
    }

    display->setActiveLoResBuffer(bufferID);
}

ST_API int st_lores_buffer_get(void) {
    ST_LOCK;

    auto display = ST_CONTEXT.display();
    if (!display) {
        ST_SET_ERROR("DisplayManager not initialized");
        return 0;
    }

    return display->getActiveLoResBuffer();
}

ST_API void st_lores_flip(void) {
    ST_LOCK;

    auto display = ST_CONTEXT.display();
    if (!display) {
        ST_SET_ERROR("DisplayManager not initialized");
        return;
    }

    display->flipLoResBuffers();
}

// =============================================================================
// Display API - LORES Blitter Functions
// =============================================================================

ST_API void st_lores_blit(int src_x, int src_y, int width, int height, int dst_x, int dst_y) {
    ST_LOCK;

    auto display = ST_CONTEXT.display();
    if (!display) {
        ST_SET_ERROR("DisplayManager not initialized");
        return;
    }

    int activeBufferID = display->getActiveLoResBuffer();
    auto buffer = display->getLoResBuffer(activeBufferID);
    if (!buffer) {
        ST_SET_ERROR("Active buffer not initialized");
        return;
    }

    buffer->blit(src_x, src_y, width, height, dst_x, dst_y);
}

ST_API void st_lores_blit_trans(int src_x, int src_y, int width, int height,
                                  int dst_x, int dst_y, uint8_t transparent_color) {
    ST_LOCK;

    auto display = ST_CONTEXT.display();
    if (!display) {
        ST_SET_ERROR("DisplayManager not initialized");
        return;
    }

    int activeBufferID = display->getActiveLoResBuffer();
    auto buffer = display->getLoResBuffer(activeBufferID);
    if (!buffer) {
        ST_SET_ERROR("Active buffer not initialized");
        return;
    }

    buffer->blitTransparent(src_x, src_y, width, height, dst_x, dst_y, transparent_color);
}

ST_API void st_lores_blit_buffer(int src_buffer, int dst_buffer,
                                   int src_x, int src_y, int width, int height,
                                   int dst_x, int dst_y) {
    ST_LOCK;

    auto display = ST_CONTEXT.display();
    if (!display) {
        ST_SET_ERROR("DisplayManager not initialized");
        return;
    }

    if (src_buffer < 0 || src_buffer > 1 || dst_buffer < 0 || dst_buffer > 1) {
        ST_SET_ERROR("Invalid buffer ID (must be 0 or 1)");
        return;
    }

    auto srcBuf = display->getLoResBuffer(src_buffer);
    auto dstBuf = display->getLoResBuffer(dst_buffer);

    if (!srcBuf || !dstBuf) {
        ST_SET_ERROR("Buffer not initialized");
        return;
    }

    dstBuf->blitFrom(srcBuf.get(), src_x, src_y, width, height, dst_x, dst_y);
}

ST_API void st_lores_blit_buffer_trans(int src_buffer, int dst_buffer,
                                         int src_x, int src_y, int width, int height,
                                         int dst_x, int dst_y, uint8_t transparent_color) {
    ST_LOCK;

    auto display = ST_CONTEXT.display();
    if (!display) {
        ST_SET_ERROR("DisplayManager not initialized");
        return;
    }

    if (src_buffer < 0 || src_buffer > 1 || dst_buffer < 0 || dst_buffer > 1) {
        ST_SET_ERROR("Invalid buffer ID (must be 0 or 1)");
        return;
    }

    auto srcBuf = display->getLoResBuffer(src_buffer);
    auto dstBuf = display->getLoResBuffer(dst_buffer);

    if (!srcBuf || !dstBuf) {
        ST_SET_ERROR("Buffer not initialized");
        return;
    }

    dstBuf->blitFromTransparent(srcBuf.get(), src_x, src_y, width, height, dst_x, dst_y, transparent_color);
}

// =============================================================================
// Display API - Sixel Graphics (Legacy - Deprecated)
// =============================================================================

ST_API uint32_t st_sixel_pack_colors(const uint8_t colors[6]) {
    if (!colors) {
        ST_SET_ERROR("Color array is null");
        return 0x000000FF; // Return black with full alpha
    }

    // Pack 6 RGBI color indices (4 bits each) into uint32_t
    // IMPORTANT: The renderer converts this as RGBA bytes:
    //   bits 24-31 (R): color[0] << 4 | color[1]
    //   bits 16-23 (G): color[2] << 4 | color[3]
    //   bits 8-15  (B): color[4] << 4 | color[5]
    //   bits 0-7   (A): 255 (full alpha, sixel marker)

    uint32_t packed = 0x000000FF; // Start with full alpha in low byte

    // Pack pairs of colors into R, G, B bytes
    uint8_t r = ((colors[0] & 0x0F) << 4) | (colors[1] & 0x0F);
    uint8_t g = ((colors[2] & 0x0F) << 4) | (colors[3] & 0x0F);
    uint8_t b = ((colors[4] & 0x0F) << 4) | (colors[5] & 0x0F);

    packed |= (r << 24) | (g << 16) | (b << 8);

    return packed;
}

ST_API void st_text_putsixel(int x, int y, uint32_t sixel_char, const uint8_t colors[6], STColor bg) {
    if (!colors) {
        ST_SET_ERROR("Color array is null");
        return;
    }

    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.textGrid(), "TextGrid");

    // Each character cell contains a 2×3 grid of sixels (6 total positions)
    // The 6-bit pattern maps to:
    //   bit 0: top-left,    bit 3: top-right
    //   bit 1: middle-left, bit 4: middle-right
    //   bit 2: bottom-left, bit 5: bottom-right

    // Convert (x, y) coordinate to cell position and sub-position
    int cell_x = x / 2;           // which character column
    int cell_y = y / 3;           // which character row
    int sub_x = x % 2;            // left (0) or right (1)
    int sub_y = y % 3;            // top (0), middle (1), or bottom (2)

    // Calculate which bit in the 6-bit pattern
    int bit_index = sub_y + (sub_x * 3);  // 0-5
    uint32_t bit_mask = (1 << bit_index);

    // Read existing cell to get current sixel pattern (if any)
    auto cell = ST_CONTEXT.textGrid()->getCell(cell_x, cell_y);

    // Extract existing pattern if it's a sixel character, otherwise 0
    uint32_t existing_pattern = 0;
    if (cell.character >= 0x1FB00 && cell.character <= 0x1FB3B) {
        existing_pattern = cell.character & 0x3F;
    }

    // Combine: add our bit to existing pattern
    uint32_t combined_pattern = existing_pattern | bit_mask;

    // Calculate the correct sixel character
    uint32_t final_char = 0x1FB00 | combined_pattern;

    // Pack the 6 colors into a single uint32_t
    uint32_t packed = st_sixel_pack_colors(colors);

    // Store the combined sixel character with packed colors
    ST_CONTEXT.textGrid()->putChar(cell_x, cell_y, final_char, packed, bg);
}

ST_API void st_text_putsixel_packed(int x, int y, uint32_t sixel_char, uint32_t packed_colors, STColor bg) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.textGrid(), "TextGrid");

    // Read existing cell to combine sixel patterns
    auto cell = ST_CONTEXT.textGrid()->getCell(x, y);

    // Extract 6-bit pattern from new sixel character
    uint32_t new_pattern = sixel_char & 0x3F;

    // Extract existing pattern if it's a sixel character
    uint32_t existing_pattern = 0;
    if (cell.character >= 0x1FB00 && cell.character <= 0x1FB3B) {
        existing_pattern = cell.character & 0x3F;
    }

    // Combine patterns with OR
    uint32_t combined_pattern = existing_pattern | new_pattern;

    // Calculate final character
    uint32_t final_char = 0x1FB00 | combined_pattern;

    // Store the combined sixel Unicode character (U+1FB00 - U+1FB3B)
    // The shader will detect this range and render colored stripes
    // Note: character won't be in font atlas, but shader bypasses atlas for sixels
    ST_CONTEXT.textGrid()->putChar(x, y, final_char, packed_colors, bg);
}

ST_API void st_sixel_set_stripe(int x, int y, int stripe_index, uint8_t color_index) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.textGrid(), "TextGrid");

    // Validate stripe index
    if (stripe_index < 0 || stripe_index > 5) {
        ST_SET_ERROR("Stripe index must be 0-5");
        return;
    }

    // Clamp color index to 4 bits (0-15)
    color_index &= 0x0F;

    // Get current cell
    auto cell = ST_CONTEXT.textGrid()->getCell(x, y);

    // Extract current packed colors from foreground
    uint32_t packed = cell.foreground;

    // Unpack the RGBA bytes back to 6 colors
    uint8_t r = (packed >> 24) & 0xFF;
    uint8_t g = (packed >> 16) & 0xFF;
    uint8_t b = (packed >> 8) & 0xFF;

    // Each byte holds 2 colors (4 bits each)
    // Determine which byte and which nibble
    int byte_index = stripe_index / 2;  // 0,1->0  2,3->1  4,5->2
    int is_high = (stripe_index % 2 == 0); // Even stripes in high nibble

    uint8_t* target_byte = (byte_index == 0) ? &r : (byte_index == 1) ? &g : &b;

    // Clear and set the nibble (ARM BFI optimization)
    if (is_high) {
        *target_byte = (*target_byte & 0x0F) | ((color_index & 0x0F) << 4);
    } else {
        *target_byte = (*target_byte & 0xF0) | (color_index & 0x0F);
    }

    // Repack with full alpha
    packed = (r << 24) | (g << 16) | (b << 8) | 0xFF;

    // Update the cell (preserve character and background)
    ST_CONTEXT.textGrid()->putChar(x, y, cell.character, packed, cell.background);
}

ST_API uint8_t st_sixel_get_stripe(int x, int y, int stripe_index) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.textGrid(), "TextGrid", 0);

    // Validate stripe index
    if (stripe_index < 0 || stripe_index > 5) {
        return 0;
    }

    // Get current cell
    auto cell = ST_CONTEXT.textGrid()->getCell(x, y);

    // Extract packed colors from foreground (RGBA byte order)
    uint32_t packed = cell.foreground;
    uint8_t r = (packed >> 24) & 0xFF;
    uint8_t g = (packed >> 16) & 0xFF;
    uint8_t b = (packed >> 8) & 0xFF;

    // Each byte holds 2 colors (4 bits each)
    int byte_index = stripe_index / 2;
    int is_high = (stripe_index % 2 == 0);

    uint8_t source_byte = (byte_index == 0) ? r : (byte_index == 1) ? g : b;
    uint8_t color = is_high ? (source_byte >> 4) : (source_byte & 0x0F);

    return color;
}

ST_API void st_sixel_gradient(int x, int y, uint8_t top_color, uint8_t bottom_color, STColor bg) {
    // Create a 6-stripe gradient from top to bottom
    uint8_t colors[6];

    // Clamp colors to 4 bits
    top_color &= 0x0F;
    bottom_color &= 0x0F;

    // Linear interpolation across 6 stripes
    for (int i = 0; i < 6; i++) {
        float t = (float)i / 5.0f; // 0.0 to 1.0
        int interpolated = (int)((1.0f - t) * top_color + t * bottom_color);
        colors[i] = (uint8_t)(interpolated & 0x0F);
    }

    st_text_putsixel(x, y, 0x1FB00, colors, bg);
}

ST_API void st_sixel_hline(int x, int y, int width, const uint8_t colors[6], STColor bg) {
    if (!colors) {
        ST_SET_ERROR("Color array is null");
        return;
    }

    // Pack colors once for efficiency
    uint32_t packed = st_sixel_pack_colors(colors);

    // Draw horizontal line
    for (int i = 0; i < width; i++) {
        st_text_putsixel_packed(x + i, y, 0x1FB00, packed, bg);
    }
}

ST_API void st_sixel_fill_rect(int x, int y, int width, int height, const uint8_t colors[6], STColor bg) {
    if (!colors) {
        ST_SET_ERROR("Color array is null");
        return;
    }

    // Pack colors once for efficiency
    uint32_t packed = st_sixel_pack_colors(colors);

    // Fill rectangle
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            st_text_putsixel_packed(x + col, y + row, 0x1FB00, packed, bg);
        }
    }
}

// =============================================================================
// Display API - Graphics Layer
// =============================================================================

ST_API void st_gfx_clear(void) {
    ST_LOCK;

    // Check if we're drawing into a sprite
    if (ST_CONTEXT.isDrawingIntoSprite()) {
        CGContextRef context = ST_CONTEXT.getSpriteDrawContext();
        int width = ST_CONTEXT.getSpriteDrawWidth();
        int height = ST_CONTEXT.getSpriteDrawHeight();
        st_sprite_clear(context, width, height);
        return;
    }

    ST_CHECK_PTR(ST_CONTEXT.graphics(), "GraphicsLayer");

    // Debug: Check buffer states before clear
    size_t frontCount = ST_CONTEXT.graphics()->getCommandCount();
    size_t backCount = ST_CONTEXT.graphics()->getBackBufferCommandCount();
    printf("[DEBUG] st_gfx_clear called: front buffer has %zu commands, back buffer has %zu commands\n",
           frontCount, backCount);

    ST_CONTEXT.graphics()->clear();

    // Debug: Check buffer states after clear
    size_t newBackCount = ST_CONTEXT.graphics()->getBackBufferCommandCount();
    printf("[DEBUG] st_gfx_clear complete: back buffer now has %zu commands (should be 0)\n", newBackCount);
}

ST_API void st_clear_all_layers(void) {
    ST_LOCK;

    // Clear text grid
    if (ST_CONTEXT.textGrid()) {
        ST_CONTEXT.textGrid()->clear();
    }

    // Clear text display overlay
    if (ST_CONTEXT.textDisplay()) {
        ST_CONTEXT.textDisplay()->clearDisplayedText();
    }

    // Clear graphics layer
    if (ST_CONTEXT.graphics()) {
        ST_CONTEXT.graphics()->clear();
    }

    // Clear rectangles - call the C API function to avoid C++/ObjC++ include issues
    st_rect_delete_all();

    // Clear circles - call the C API function to avoid C++/ObjC++ include issues
    st_circle_delete_all();

    // Clear lines - call the C API function to avoid C++/ObjC++ include issues
    st_line_delete_all();

    // Note: Particles, sprites, and tilemaps require explicit cleanup by the user
    // Use PARTCLEAR, sprite deletion commands, or TILEMAP_CLEAR as needed
    // as they may contain loaded assets
}

ST_API void st_gfx_rect(int x, int y, int width, int height, STColor color) {
    ST_LOCK;

    // Extract RGBA components
    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;
    uint8_t a = color & 0xFF;

    // Check if we're drawing into a sprite
    printf("[st_gfx_rect] isDrawingIntoSprite=%d\n", ST_CONTEXT.isDrawingIntoSprite());
    if (ST_CONTEXT.isDrawingIntoSprite()) {
        CGContextRef context = ST_CONTEXT.getSpriteDrawContext();
        printf("[st_gfx_rect] Drawing to sprite context: x=%d y=%d w=%d h=%d color=0x%08X\n", x, y, width, height, color);
        st_sprite_draw_rect(context, x, y, width, height,
                           r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
        printf("[st_gfx_rect] Sprite draw complete\n");
        return;
    }

    // Check if we're drawing to a file
    if (ST_CONTEXT.isDrawingToFile()) {
        CGContextRef context = ST_CONTEXT.getFileDrawContext();
        printf("[st_gfx_rect] Drawing to file context: x=%d y=%d w=%d h=%d color=0x%08X\n", x, y, width, height, color);
        st_sprite_draw_rect(context, x, y, width, height,
                           r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
        printf("[st_gfx_rect] File draw complete\n");
        return;
    }

    // Check if we're drawing to a tileset
    if (ST_CONTEXT.isDrawingToTileset()) {
        CGContextRef context = ST_CONTEXT.getTilesetDrawContext();
        printf("[st_gfx_rect] Drawing to tileset context: x=%d y=%d w=%d h=%d color=0x%08X\n", x, y, width, height, color);
        // Note: coordinates are already relative to current tile due to transform in st_tileset_draw_tile
        st_sprite_draw_rect(context, x, y, width, height,
                           r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
        printf("[st_gfx_rect] Tileset draw complete\n");
        return;
    }

    ST_CHECK_PTR(ST_CONTEXT.graphics(), "GraphicsLayer");

    // Debug logging
    printf("[DEBUG] st_gfx_rect called: x=%d, y=%d, w=%d, h=%d, color=0x%08X (r=%d, g=%d, b=%d, a=%d)\n",
           x, y, width, height, color, r, g, b, a);

    // Check back buffer command count before and after (since that's where new commands go)
    size_t beforeCount = ST_CONTEXT.graphics()->getBackBufferCommandCount();
    size_t frontCount = ST_CONTEXT.graphics()->getCommandCount();

    ST_CONTEXT.graphics()->fillRect(
        x, y, width, height,
        r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f
    );

    size_t afterCount = ST_CONTEXT.graphics()->getBackBufferCommandCount();
    printf("[DEBUG] Back buffer command count: %zu -> %zu (added %zu commands)\n",
           beforeCount, afterCount, afterCount - beforeCount);
    printf("[DEBUG] Front buffer has %zu commands (visible)\n", frontCount);
}

ST_API void st_gfx_rect_outline(int x, int y, int width, int height, STColor color, int thickness) {
    ST_LOCK;

    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;
    uint8_t a = color & 0xFF;

    // Check if we're drawing into a sprite
    if (ST_CONTEXT.isDrawingIntoSprite()) {
        CGContextRef context = ST_CONTEXT.getSpriteDrawContext();
        st_sprite_draw_rect_outline(context, x, y, width, height,
                                    r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f, thickness);
        return;
    }

    ST_CHECK_PTR(ST_CONTEXT.graphics(), "GraphicsLayer");

    ST_CONTEXT.graphics()->drawRect(
        x, y, width, height,
        r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f,
        thickness
    );
}

ST_API void st_gfx_circle(int x, int y, int radius, STColor color) {
    ST_LOCK;

    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;
    uint8_t a = color & 0xFF;

    // Check if we're drawing into a sprite
    if (ST_CONTEXT.isDrawingIntoSprite()) {
        CGContextRef context = ST_CONTEXT.getSpriteDrawContext();
        st_sprite_draw_circle(context, x, y, radius,
                            r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
        return;
    }

    // Check if we're drawing to a file
    if (ST_CONTEXT.isDrawingToFile()) {
        CGContextRef context = ST_CONTEXT.getFileDrawContext();
        st_sprite_draw_circle(context, x, y, radius,
                            r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
        return;
    }

    // Check if we're drawing to a tileset
    if (ST_CONTEXT.isDrawingToTileset()) {
        CGContextRef context = ST_CONTEXT.getTilesetDrawContext();
        st_sprite_draw_circle(context, x, y, radius,
                            r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
        return;
    }

    ST_CHECK_PTR(ST_CONTEXT.graphics(), "GraphicsLayer");

    ST_CONTEXT.graphics()->fillCircle(
        x, y, radius,
        r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f
    );
}

ST_API void st_gfx_circle_outline(int x, int y, int radius, STColor color, int thickness) {
    ST_LOCK;

    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;
    uint8_t a = color & 0xFF;

    // Check if we're drawing into a sprite
    if (ST_CONTEXT.isDrawingIntoSprite()) {
        CGContextRef context = ST_CONTEXT.getSpriteDrawContext();
        st_sprite_draw_circle_outline(context, x, y, radius,
                                      r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f, thickness);
        return;
    }

    ST_CHECK_PTR(ST_CONTEXT.graphics(), "GraphicsLayer");

    ST_CONTEXT.graphics()->drawCircle(
        x, y, radius,
        r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f,
        thickness
    );
}

ST_API void st_gfx_arc(int x, int y, int radius, float start_angle, float end_angle, STColor color) {
    ST_LOCK;

    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;
    uint8_t a = color & 0xFF;

    // Convert degrees to radians
    float start_rad = start_angle * M_PI / 180.0f;
    float end_rad = end_angle * M_PI / 180.0f;

    // Check if we're drawing into a sprite
    if (ST_CONTEXT.isDrawingIntoSprite()) {
        CGContextRef context = ST_CONTEXT.getSpriteDrawContext();
        st_sprite_draw_arc(context, x, y, radius, start_rad, end_rad,
                          r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
        return;
    }

    ST_CHECK_PTR(ST_CONTEXT.graphics(), "GraphicsLayer");

    // Draw arc using line segments
    const int segments = 32;
    float angle_range = end_rad - start_rad;
    float angle_step = angle_range / segments;

    for (int i = 0; i < segments; i++) {
        float a1 = start_rad + angle_step * i;
        float a2 = start_rad + angle_step * (i + 1);

        int x1 = x + radius * cos(a1);
        int y1 = y + radius * sin(a1);
        int x2 = x + radius * cos(a2);
        int y2 = y + radius * sin(a2);

        ST_CONTEXT.graphics()->drawLine(x1, y1, x2, y2,
                                        r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f, 1);
    }
}

ST_API void st_gfx_arc_filled(int x, int y, int radius, float start_angle, float end_angle, STColor color) {
    ST_LOCK;

    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;
    uint8_t a = color & 0xFF;

    // Convert degrees to radians
    float start_rad = start_angle * M_PI / 180.0f;
    float end_rad = end_angle * M_PI / 180.0f;

    // Check if we're drawing into a sprite
    if (ST_CONTEXT.isDrawingIntoSprite()) {
        CGContextRef context = ST_CONTEXT.getSpriteDrawContext();
        st_sprite_draw_arc_filled(context, x, y, radius, start_rad, end_rad,
                                  r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
        return;
    }

    ST_CHECK_PTR(ST_CONTEXT.graphics(), "GraphicsLayer");

    // Draw filled arc using triangles from center
    const int segments = 32;
    float angle_range = end_rad - start_rad;
    float angle_step = angle_range / segments;

    for (int i = 0; i < segments; i++) {
        float a1 = start_rad + angle_step * i;
        float a2 = start_rad + angle_step * (i + 1);

        int x1 = x + radius * cos(a1);
        int y1 = y + radius * sin(a1);
        int x2 = x + radius * cos(a2);
        int y2 = y + radius * sin(a2);

        // Draw triangle from center to arc edge
        ST_CONTEXT.graphics()->drawLine(x, y, x1, y1,
                                        r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f, 1);
        ST_CONTEXT.graphics()->drawLine(x1, y1, x2, y2,
                                        r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f, 1);
        ST_CONTEXT.graphics()->drawLine(x2, y2, x, y,
                                        r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f, 1);
    }
}

ST_API void st_gfx_line(int x1, int y1, int x2, int y2, STColor color, int thickness) {
    ST_LOCK;

    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;
    uint8_t a = color & 0xFF;

    // Check if we're drawing into a sprite
    if (ST_CONTEXT.isDrawingIntoSprite()) {
        CGContextRef context = ST_CONTEXT.getSpriteDrawContext();
        st_sprite_draw_line(context, x1, y1, x2, y2,
                          r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f, thickness);
        return;
    }

    ST_CHECK_PTR(ST_CONTEXT.graphics(), "GraphicsLayer");

    ST_CONTEXT.graphics()->drawLine(
        x1, y1, x2, y2,
        r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f,
        thickness
    );
}

ST_API void st_gfx_point(int x, int y, STColor color) {
    ST_LOCK;

    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;
    uint8_t a = color & 0xFF;

    // Check if we're drawing into a sprite
    if (ST_CONTEXT.isDrawingIntoSprite()) {
        CGContextRef context = ST_CONTEXT.getSpriteDrawContext();
        st_sprite_draw_pixel(context, x, y,
                           r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
        return;
    }

    ST_CHECK_PTR(ST_CONTEXT.graphics(), "GraphicsLayer");

    // Draw as a 1-pixel filled rect
    ST_CONTEXT.graphics()->fillRect(
        x, y, 1, 1,
        r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f
    );
}

ST_API void st_gfx_swap(void) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.graphics(), "GraphicsLayer");

    printf("[DEBUG] st_gfx_swap called - swapping front and back buffers\n");

    // Get command counts before swap for debugging
    size_t frontCount = ST_CONTEXT.graphics()->getCommandCount();
    size_t backCount = ST_CONTEXT.graphics()->getBackBufferCommandCount();

    printf("[DEBUG] Before swap: front buffer has %zu commands, back buffer has %zu commands\n",
           frontCount, backCount);

    // Perform the actual buffer swap
    ST_CONTEXT.graphics()->swapBuffers();

    // Get command counts after swap for debugging
    size_t newFrontCount = ST_CONTEXT.graphics()->getCommandCount();
    size_t newBackCount = ST_CONTEXT.graphics()->getBackBufferCommandCount();

    printf("[DEBUG] After swap: front buffer has %zu commands, back buffer has %zu commands\n",
           newFrontCount, newBackCount);
}

// =============================================================================
// Display API - Layers
// =============================================================================

ST_API void st_layer_set_visible(STLayer layer, bool visible) {
    // TODO: Implement layer visibility when we add layer management
    ST_SET_ERROR("Layer visibility not yet implemented");
}

ST_API void st_layer_set_alpha(STLayer layer, float alpha) {
    // TODO: Implement layer alpha when we add layer management
    ST_SET_ERROR("Layer alpha not yet implemented");
}

ST_API void st_layer_set_order(STLayer layer, int order) {
    // TODO: Implement layer ordering when we add layer management
    ST_SET_ERROR("Layer ordering not yet implemented");
}

// =============================================================================
// Display API - Screen
// =============================================================================

ST_API void st_display_size(int* width, int* height) {
    ST_LOCK;

    if (ST_CONTEXT.display()) {
        uint32_t w, h;
        ST_CONTEXT.display()->getWindowSize(w, h);
        if (width) *width = static_cast<int>(w);
        if (height) *height = static_cast<int>(h);
    } else {
        // Default size if display not available
        if (width) *width = 800;
        if (height) *height = 600;
    }
}

ST_API void st_cell_size(int* width, int* height) {
    ST_LOCK;

    // Cell size is a fixed configuration value for now
    // TODO: Store DisplayConfig in Context to allow runtime queries
    // Default cell size matches the DisplayConfig defaults
    if (width) *width = 8;
    if (height) *height = 16;
}

// =============================================================================
// Display API - URES Mode (Ultra Resolution 1280×720 direct color)
// =============================================================================

ST_API void st_ures_pset(int x, int y, int color) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveUResBuffer();
    auto uresBuffer = display->getUResBuffer(bufferID);

    if (!uresBuffer) {
        ST_SET_ERROR("UResBuffer not available");
        return;
    }

    // color is 16-bit ARGB4444 format: 0xARGB
    uresBuffer->setPixel(x, y, static_cast<uint16_t>(color & 0xFFFF));
}

ST_API int st_ures_pget(int x, int y) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.display(), "DisplayManager", 0);

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveUResBuffer();
    auto uresBuffer = display->getUResBuffer(bufferID);

    if (!uresBuffer) {
        ST_SET_ERROR("UResBuffer not available");
        return 0;
    }

    return static_cast<int>(uresBuffer->getPixel(x, y));
}

ST_API void st_ures_clear(int color) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveUResBuffer();
    auto uresBuffer = display->getUResBuffer(bufferID);

    if (!uresBuffer) {
        ST_SET_ERROR("UResBuffer not available");
        return;
    }

    uresBuffer->clear(static_cast<uint16_t>(color & 0xFFFF));
}

ST_API void st_ures_fillrect(int x, int y, int width, int height, int color) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveUResBuffer();
    auto uresBuffer = display->getUResBuffer(bufferID);

    if (!uresBuffer) {
        ST_SET_ERROR("UResBuffer not available");
        return;
    }

    uresBuffer->fillRect(x, y, width, height, static_cast<uint16_t>(color & 0xFFFF));
}

ST_API void st_ures_hline(int x, int y, int width, int color) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveUResBuffer();
    auto uresBuffer = display->getUResBuffer(bufferID);

    if (!uresBuffer) {
        ST_SET_ERROR("UResBuffer not available");
        return;
    }

    uresBuffer->hline(x, y, width, static_cast<uint16_t>(color & 0xFFFF));
}

ST_API void st_ures_vline(int x, int y, int height, int color) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveUResBuffer();
    auto uresBuffer = display->getUResBuffer(bufferID);

    if (!uresBuffer) {
        ST_SET_ERROR("UResBuffer not available");
        return;
    }

    uresBuffer->vline(x, y, height, static_cast<uint16_t>(color & 0xFFFF));
}

// =============================================================================
// Display API - URES Buffer Management
// =============================================================================

ST_API void st_ures_buffer(int bufferID) {
    ST_LOCK;

    auto display = ST_CONTEXT.display();
    if (!display) {
        ST_SET_ERROR("DisplayManager not initialized");
        return;
    }

    if (bufferID < 0 || bufferID > 7) {
        ST_SET_ERROR("Invalid buffer ID (must be 0-7)");
        return;
    }

    display->setActiveUResBuffer(bufferID);
}

ST_API int st_ures_buffer_get(void) {
    ST_LOCK;

    auto display = ST_CONTEXT.display();
    if (!display) {
        ST_SET_ERROR("DisplayManager not initialized");
        return 0;
    }

    return display->getActiveUResBuffer();
}

ST_API void st_ures_flip(void) {
    ST_LOCK;

    auto display = ST_CONTEXT.display();
    if (!display) {
        ST_SET_ERROR("DisplayManager not initialized");
        return;
    }

    display->flipUResBuffers();
}

ST_API void st_ures_gpu_flip(void) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->uresGPUFlip();
}

ST_API void st_ures_sync(int bufferID) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    // For GPU operations, just sync the GPU
    // (GPU primitives write directly to GPU textures, no CPU upload needed)
    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }
    renderer->syncGPU();
}

ST_API void st_ures_swap(int bufferID) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    // Swap the specified buffer to buffer 0 (display)
    // Currently only supports swapping buffers 0 and 1
    if (bufferID == 0 || bufferID == 1) {
        auto display = ST_CONTEXT.display();
        auto renderer = display->getRenderer();
        if (!renderer) {
            ST_SET_ERROR("MetalRenderer not available");
            return;
        }
        renderer->uresGPUFlip();
    } else {
        ST_SET_ERROR("ures_swap only supports buffers 0 and 1");
    }
}

ST_API void st_ures_blit_from(int srcBufferID, int srcX, int srcY,
                               int width, int height, int dstX, int dstY) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int dstBufferID = display->getActiveUResBuffer();

    auto srcBuffer = display->getUResBuffer(srcBufferID);
    auto dstBuffer = display->getUResBuffer(dstBufferID);

    if (!srcBuffer || !dstBuffer) {
        ST_SET_ERROR("UResBuffer not available");
        return;
    }

    // Use UResBuffer's built-in blitFrom method which handles all the details
    dstBuffer->blitFrom(srcBuffer.get(), srcX, srcY, width, height, dstX, dstY);
}

ST_API void st_ures_blit_from_trans(int srcBufferID, int srcX, int srcY,
                                     int width, int height, int dstX, int dstY) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int dstBufferID = display->getActiveUResBuffer();

    auto srcBuffer = display->getUResBuffer(srcBufferID);
    auto dstBuffer = display->getUResBuffer(dstBufferID);

    if (!srcBuffer || !dstBuffer) {
        ST_SET_ERROR("UResBuffer not available");
        return;
    }

    // Use UResBuffer's built-in blitFromTransparent method
    dstBuffer->blitFromTransparent(srcBuffer.get(), srcX, srcY, width, height, dstX, dstY);
}

ST_API int st_urgb(int r, int g, int b) {
    // Clamp and convert 4-bit RGB (0-15) to ARGB4444 with full opacity
    r = (r < 0) ? 0 : (r > 15) ? 15 : r;
    g = (g < 0) ? 0 : (g > 15) ? 15 : g;
    b = (b < 0) ? 0 : (b > 15) ? 15 : b;

    return 0xF000 | (r << 8) | (g << 4) | b;
}

ST_API int st_urgba(int r, int g, int b, int a) {
    // Clamp and convert 4-bit RGBA (0-15 each) to ARGB4444
    r = (r < 0) ? 0 : (r > 15) ? 15 : r;
    g = (g < 0) ? 0 : (g > 15) ? 15 : g;
    b = (b < 0) ? 0 : (b > 15) ? 15 : b;
    a = (a < 0) ? 0 : (a > 15) ? 15 : a;

    return (a << 12) | (r << 8) | (g << 4) | b;
}

// =============================================================================
// Display API - XRES Buffer Operations (Mode X: 320×240, 256-color palette)
// =============================================================================

ST_API void st_xres_pset(int x, int y, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveXResBuffer();
    auto xresBuffer = display->getXResBuffer(bufferID);

    if (!xresBuffer) {
        ST_SET_ERROR("XResBuffer not available");
        return;
    }

    xresBuffer->setPixel(x, y, static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API int st_xres_pget(int x, int y) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.display(), "DisplayManager", 0);

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveXResBuffer();
    auto xresBuffer = display->getXResBuffer(bufferID);

    if (!xresBuffer) {
        ST_SET_ERROR("XResBuffer not available");
        return 0;
    }

    return xresBuffer->getPixel(x, y);
}

ST_API void st_xres_clear(int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveXResBuffer();
    auto xresBuffer = display->getXResBuffer(bufferID);

    if (!xresBuffer) {
        ST_SET_ERROR("XResBuffer not available");
        return;
    }

    xresBuffer->clear(static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API void st_xres_fillrect(int x, int y, int width, int height, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveXResBuffer();
    auto xresBuffer = display->getXResBuffer(bufferID);

    if (!xresBuffer) {
        ST_SET_ERROR("XResBuffer not available");
        return;
    }

    static int callCount = 0;
    if (++callCount <= 10) {
        printf("[XRES FILLRECT] Call #%d: x=%d y=%d w=%d h=%d color=%d buffer=%d\n",
              callCount, x, y, width, height, colorIndex, bufferID);
        fflush(stdout);
    }

    xresBuffer->fillRect(x, y, width, height, static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API void st_xres_hline(int x, int y, int width, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveXResBuffer();
    auto xresBuffer = display->getXResBuffer(bufferID);

    if (!xresBuffer) {
        ST_SET_ERROR("XResBuffer not available");
        return;
    }

    xresBuffer->hline(x, y, width, static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API void st_xres_vline(int x, int y, int height, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveXResBuffer();
    auto xresBuffer = display->getXResBuffer(bufferID);

    if (!xresBuffer) {
        ST_SET_ERROR("XResBuffer not available");
        return;
    }

    xresBuffer->vline(x, y, height, static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API void st_xres_buffer(int bufferID) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    display->setActiveXResBuffer(bufferID);
}

ST_API void st_xres_flip() {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    printf("[ST_XRES_FLIP DEBUG] st_xres_flip() called\n");
    auto display = ST_CONTEXT.display();
    printf("[ST_XRES_FLIP DEBUG] Calling display->flipXResBuffers()\n");
    display->flipXResBuffers();
    printf("[ST_XRES_FLIP DEBUG] flipXResBuffers() complete\n");
}

ST_API void st_xres_blit(int srcX, int srcY, int width, int height, int dstX, int dstY) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveXResBuffer();
    auto xresBuffer = display->getXResBuffer(bufferID);

    if (!xresBuffer) {
        ST_SET_ERROR("XResBuffer not available");
        return;
    }

    xresBuffer->blit(srcX, srcY, width, height, dstX, dstY);
}

ST_API void st_xres_blit_trans(int srcX, int srcY, int width, int height, int dstX, int dstY) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveXResBuffer();
    auto xresBuffer = display->getXResBuffer(bufferID);

    if (!xresBuffer) {
        ST_SET_ERROR("XResBuffer not available");
        return;
    }

    xresBuffer->blitTransparent(srcX, srcY, width, height, dstX, dstY);
}

ST_API void st_xres_blit_from(int srcBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int dstBufferID = display->getActiveXResBuffer();
    auto srcBuffer = display->getXResBuffer(srcBufferID);
    auto dstBuffer = display->getXResBuffer(dstBufferID);

    if (!srcBuffer || !dstBuffer) {
        ST_SET_ERROR("XResBuffer not available");
        return;
    }

    dstBuffer->blitFrom(srcBuffer.get(), srcX, srcY, width, height, dstX, dstY);
}

ST_API void st_xres_blit_from_trans(int srcBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int dstBufferID = display->getActiveXResBuffer();
    auto srcBuffer = display->getXResBuffer(srcBufferID);
    auto dstBuffer = display->getXResBuffer(dstBufferID);

    if (!srcBuffer || !dstBuffer) {
        ST_SET_ERROR("XResBuffer not available");
        return;
    }

    dstBuffer->blitFromTransparent(srcBuffer.get(), srcX, srcY, width, height, dstX, dstY);
}

// =============================================================================
// GPU-accelerated LORES blit operations
// =============================================================================

ST_API void st_lores_blit_gpu(int srcBufferID, int dstBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }
    renderer->loresBlitGPU(srcBufferID, dstBufferID, srcX, srcY, width, height, dstX, dstY);
}

ST_API void st_lores_blit_trans_gpu(int srcBufferID, int dstBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY, int transparentColor) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }
    renderer->loresBlitTransparentGPU(srcBufferID, dstBufferID, srcX, srcY, width, height, dstX, dstY, static_cast<uint8_t>(transparentColor));
}

ST_API void st_lores_clear_gpu(int bufferID, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->loresClearGPU(bufferID, static_cast<uint8_t>(colorIndex));
}

ST_API void st_lores_rect_fill_gpu(int bufferID, int x, int y, int width, int height, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->loresRectFillGPU(bufferID, x, y, width, height, static_cast<uint8_t>(colorIndex));
}

ST_API void st_lores_circle_fill_gpu(int bufferID, int cx, int cy, int radius, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->loresCircleFillGPU(bufferID, cx, cy, radius, static_cast<uint8_t>(colorIndex));
}

ST_API void st_lores_line_gpu(int bufferID, int x0, int y0, int x1, int y1, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->loresLineGPU(bufferID, x0, y0, x1, y1, static_cast<uint8_t>(colorIndex));
}

// =============================================================================
// GPU-accelerated XRES blit operations
// =============================================================================

ST_API void st_xres_blit_gpu(int srcBufferID, int dstBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }
    renderer->xresBlitGPU(srcBufferID, dstBufferID, srcX, srcY, width, height, dstX, dstY);
}

ST_API void st_xres_blit_trans_gpu(int srcBufferID, int dstBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY, int transparentColor) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }
    renderer->xresBlitTransparentGPU(srcBufferID, dstBufferID, srcX, srcY, width, height, dstX, dstY, static_cast<uint8_t>(transparentColor));
}

// XRES Hybrid Palette Management
// Per-row palette: indices 0-15 (16 colors per row × 240 rows)
// Global palette: indices 16-255 (240 shared colors)

ST_API void st_xres_palette_row(int row, int index, int r, int g, int b) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.xresPalette(), "XResPaletteManager");

    // Validate parameters
    if (row < 0 || row >= 240) {
        ST_SET_ERROR("Row must be 0-239");
        return;
    }
    if (index < 0 || index >= 16) {
        ST_SET_ERROR("Per-row palette index must be 0-15");
        return;
    }

    // Clamp RGB to 0-255
    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;

    // Set per-row palette entry in XResPaletteManager
    uint32_t rgba = 0xFF000000 | (r << 16) | (g << 8) | b;
    ST_CONTEXT.xresPalette()->setPerRowColor(row, index, rgba);
}

ST_API void st_xres_palette_global(int index, int r, int g, int b) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.xresPalette(), "XResPaletteManager");

    // Validate parameters
    if (index < 16 || index >= 256) {
        ST_SET_ERROR("Global palette index must be 16-255");
        return;
    }

    // Clamp RGB to 0-255
    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;

    // Set global palette entry in XResPaletteManager
    uint32_t rgba = 0xFF000000 | (r << 16) | (g << 8) | b;
    ST_CONTEXT.xresPalette()->setGlobalColor(index, rgba);
}

ST_API int st_xrgb(int r, int g, int b) {
    // Calculate global palette index for 6×8×5 RGB cube (indices 16-255)
    // 6 red levels (0-5) × 8 green levels (0-7) × 5 blue levels (0-4) = 240 colors

    // Clamp RGB to 0-255
    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;

    // Map 0-255 to cube indices
    int rIndex = (r * 5) / 255;  // 0-5
    int gIndex = (g * 7) / 255;  // 0-7
    int bIndex = (b * 4) / 255;  // 0-4

    // Clamp to valid ranges
    rIndex = (rIndex < 0) ? 0 : (rIndex > 5) ? 5 : rIndex;
    gIndex = (gIndex < 0) ? 0 : (gIndex > 7) ? 7 : gIndex;
    bIndex = (bIndex < 0) ? 0 : (bIndex > 4) ? 4 : bIndex;

    // Calculate index: 16 + r*40 + g*5 + b
    int index = 16 + (rIndex * 40) + (gIndex * 5) + bIndex;

    // Safety clamp
    if (index < 16) index = 16;
    if (index > 255) index = 255;

    return index;
}

ST_API void st_xres_palette_reset() {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.xresPalette(), "XResPaletteManager");
    
    // Reset to default palette (IBM RGBI for 0-15, RGB cube for 16-255)
    ST_CONTEXT.xresPalette()->loadPresetPalette(XResPalettePreset::IBM_RGBI);
    ST_CONTEXT.xresPalette()->loadPresetPalette(XResPalettePreset::RGB_CUBE_6x8x5);
}

// =============================================================================
// Display API - WRES Buffer Operations (Wide Mode: 432×240, 256-color palette)
// =============================================================================

ST_API void st_wres_pset(int x, int y, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveWResBuffer();
    auto wresBuffer = display->getWResBuffer(bufferID);

    if (!wresBuffer) {
        ST_SET_ERROR("WResBuffer not available");
        return;
    }

    wresBuffer->setPixel(x, y, static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API int st_wres_pget(int x, int y) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.display(), "DisplayManager", 0);

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveWResBuffer();
    auto wresBuffer = display->getWResBuffer(bufferID);

    if (!wresBuffer) {
        ST_SET_ERROR("WResBuffer not available");
        return 0;
    }

    return wresBuffer->getPixel(x, y);
}

ST_API void st_wres_clear(int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveWResBuffer();
    auto wresBuffer = display->getWResBuffer(bufferID);

    if (!wresBuffer) {
        ST_SET_ERROR("WResBuffer not available");
        return;
    }

    wresBuffer->clear(static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API void st_wres_fillrect(int x, int y, int width, int height, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveWResBuffer();
    auto wresBuffer = display->getWResBuffer(bufferID);

    if (!wresBuffer) {
        ST_SET_ERROR("WResBuffer not available");
        return;
    }

    wresBuffer->fillRect(x, y, width, height, static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API void st_wres_hline(int x, int y, int width, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveWResBuffer();
    auto wresBuffer = display->getWResBuffer(bufferID);

    if (!wresBuffer) {
        ST_SET_ERROR("WResBuffer not available");
        return;
    }

    wresBuffer->hline(x, y, width, static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API void st_wres_vline(int x, int y, int height, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveWResBuffer();
    auto wresBuffer = display->getWResBuffer(bufferID);

    if (!wresBuffer) {
        ST_SET_ERROR("WResBuffer not available");
        return;
    }

    wresBuffer->vline(x, y, height, static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API void st_wres_buffer(int bufferID) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    display->setActiveWResBuffer(bufferID);
}

ST_API void st_wres_flip() {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    display->flipWResBuffers();
}

ST_API void st_wres_blit(int srcX, int srcY, int width, int height, int dstX, int dstY) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveWResBuffer();
    auto wresBuffer = display->getWResBuffer(bufferID);

    if (!wresBuffer) {
        ST_SET_ERROR("WResBuffer not available");
        return;
    }

    wresBuffer->blit(srcX, srcY, width, height, dstX, dstY);
}

ST_API void st_wres_blit_trans(int srcX, int srcY, int width, int height, int dstX, int dstY) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveWResBuffer();
    auto wresBuffer = display->getWResBuffer(bufferID);

    if (!wresBuffer) {
        ST_SET_ERROR("WResBuffer not available");
        return;
    }

    wresBuffer->blitTransparent(srcX, srcY, width, height, dstX, dstY);
}

ST_API void st_wres_blit_from(int srcBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int dstBufferID = display->getActiveWResBuffer();
    auto srcBuffer = display->getWResBuffer(srcBufferID);
    auto dstBuffer = display->getWResBuffer(dstBufferID);

    if (!srcBuffer || !dstBuffer) {
        ST_SET_ERROR("WResBuffer not available");
        return;
    }

    dstBuffer->blitFrom(srcBuffer.get(), srcX, srcY, width, height, dstX, dstY);
}

ST_API void st_wres_blit_from_trans(int srcBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int dstBufferID = display->getActiveWResBuffer();
    auto srcBuffer = display->getWResBuffer(srcBufferID);
    auto dstBuffer = display->getWResBuffer(dstBufferID);

    if (!srcBuffer || !dstBuffer) {
        ST_SET_ERROR("WResBuffer not available");
        return;
    }

    dstBuffer->blitFromTransparent(srcBuffer.get(), srcX, srcY, width, height, dstX, dstY);
}

// GPU-accelerated WRES blit operations
ST_API void st_wres_blit_gpu(int srcBufferID, int dstBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }
    renderer->wresBlitGPU(srcBufferID, dstBufferID, srcX, srcY, width, height, dstX, dstY);
}

ST_API void st_wres_blit_trans_gpu(int srcBufferID, int dstBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY, int transparentColor) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }
    renderer->wresBlitTransparentGPU(srcBufferID, dstBufferID, srcX, srcY, width, height, dstX, dstY, static_cast<uint8_t>(transparentColor));
}

// GPU synchronization
ST_API void st_gpu_sync() {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }
    renderer->syncGPU();

    // Clear dirty flags for all XRES/WRES buffers to prevent CPU upload
    // from overwriting GPU blit results
    if (display->isXResMode()) {
        for (int i = 0; i < 4; i++) {
            auto xresBuffer = display->getXResBuffer(i);
            if (xresBuffer) {
                xresBuffer->clearDirty();
            }
        }
    }
    if (display->isWResMode()) {
        for (int i = 0; i < 4; i++) {
            auto wresBuffer = display->getWResBuffer(i);
            if (wresBuffer) {
                wresBuffer->clearDirty();
            }
        }
    }
}

// GPU blitter batching for performance
ST_API void st_begin_blit_batch() {
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

ST_API void st_end_blit_batch() {
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

ST_API void st_xres_clear_gpu(int bufferID, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->xresClearGPU(bufferID, static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API void st_wres_clear_gpu(int bufferID, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->wresClearGPU(bufferID, static_cast<uint8_t>(colorIndex & 0xFF));
}

// GPU Primitive Drawing APIs

ST_API void st_xres_rect_fill_gpu(int bufferID, int x, int y, int width, int height, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->xresRectFillGPU(bufferID, x, y, width, height, static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API void st_wres_rect_fill_gpu(int bufferID, int x, int y, int width, int height, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->wresRectFillGPU(bufferID, x, y, width, height, static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API void st_xres_circle_fill_gpu(int bufferID, int cx, int cy, int radius, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->xresCircleFillGPU(bufferID, cx, cy, radius, static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API void st_wres_circle_fill_gpu(int bufferID, int cx, int cy, int radius, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->wresCircleFillGPU(bufferID, cx, cy, radius, static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API void st_xres_line_gpu(int bufferID, int x0, int y0, int x1, int y1, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->xresLineGPU(bufferID, x0, y0, x1, y1, static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API void st_wres_line_gpu(int bufferID, int x0, int y0, int x1, int y1, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->wresLineGPU(bufferID, x0, y0, x1, y1, static_cast<uint8_t>(colorIndex & 0xFF));
}

// GPU Anti-Aliased Primitive Drawing APIs

ST_API void st_xres_circle_fill_aa(int bufferID, int cx, int cy, int radius, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->xresCircleFillAA(bufferID, cx, cy, radius, static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API void st_wres_circle_fill_aa(int bufferID, int cx, int cy, int radius, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->wresCircleFillAA(bufferID, cx, cy, radius, static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API void st_xres_line_aa(int bufferID, int x0, int y0, int x1, int y1, int colorIndex, float lineWidth) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->xresLineAA(bufferID, x0, y0, x1, y1, static_cast<uint8_t>(colorIndex & 0xFF), lineWidth);
}

ST_API void st_wres_line_aa(int bufferID, int x0, int y0, int x1, int y1, int colorIndex, float lineWidth) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->wresLineAA(bufferID, x0, y0, x1, y1, static_cast<uint8_t>(colorIndex & 0xFF), lineWidth);
}

// =============================================================================
// PRES Buffer API (Premium Resolution 1280×720, 256-color palette)
// =============================================================================

ST_API void st_pres_pset(int x, int y, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActivePResBuffer();
    auto presBuffer = display->getPResBuffer(bufferID);

    if (!presBuffer) {
        ST_SET_ERROR("PResBuffer not available");
        return;
    }

    presBuffer->setPixel(x, y, static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API int st_pres_pget(int x, int y) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.display(), "DisplayManager", 0);

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActivePResBuffer();
    auto presBuffer = display->getPResBuffer(bufferID);

    if (!presBuffer) {
        ST_SET_ERROR("PResBuffer not available");
        return 0;
    }

    return presBuffer->getPixel(x, y);
}

ST_API void st_pres_clear(int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActivePResBuffer();
    auto presBuffer = display->getPResBuffer(bufferID);

    if (!presBuffer) {
        ST_SET_ERROR("PResBuffer not available");
        return;
    }

    presBuffer->clear(static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API void st_pres_fillrect(int x, int y, int width, int height, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActivePResBuffer();
    auto presBuffer = display->getPResBuffer(bufferID);

    if (!presBuffer) {
        ST_SET_ERROR("PResBuffer not available");
        return;
    }

    presBuffer->fillRect(x, y, width, height, static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API void st_pres_hline(int x, int y, int width, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActivePResBuffer();
    auto presBuffer = display->getPResBuffer(bufferID);

    if (!presBuffer) {
        ST_SET_ERROR("PResBuffer not available");
        return;
    }

    presBuffer->hline(x, y, width, static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API void st_pres_vline(int x, int y, int height, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActivePResBuffer();
    auto presBuffer = display->getPResBuffer(bufferID);

    if (!presBuffer) {
        ST_SET_ERROR("PResBuffer not available");
        return;
    }

    presBuffer->vline(x, y, height, static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API void st_pres_circle_simple(int cx, int cy, int radius, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActivePResBuffer();
    auto presBuffer = display->getPResBuffer(bufferID);

    if (!presBuffer) {
        ST_SET_ERROR("PResBuffer not available");
        return;
    }

    presBuffer->circle(cx, cy, radius, static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API void st_pres_line_simple(int x0, int y0, int x1, int y1, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActivePResBuffer();
    auto presBuffer = display->getPResBuffer(bufferID);

    if (!presBuffer) {
        ST_SET_ERROR("PResBuffer not available");
        return;
    }

    presBuffer->line(x0, y0, x1, y1, static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API void st_pres_buffer(int bufferID) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    display->setActivePResBuffer(bufferID);
}

ST_API void st_pres_flip() {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    display->flipPResBuffers();
    
    // Also swap GPU textures for immediate effect
    auto renderer = display->getRenderer();
    if (renderer) {
        renderer->presGPUFlip();
    }
}

ST_API void st_pres_blit(int srcX, int srcY, int width, int height, int dstX, int dstY) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActivePResBuffer();
    auto presBuffer = display->getPResBuffer(bufferID);

    if (!presBuffer) {
        ST_SET_ERROR("PResBuffer not available");
        return;
    }

    presBuffer->blit(srcX, srcY, width, height, dstX, dstY);
}

ST_API void st_pres_blit_trans(int srcX, int srcY, int width, int height, int dstX, int dstY) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActivePResBuffer();
    auto presBuffer = display->getPResBuffer(bufferID);

    if (!presBuffer) {
        ST_SET_ERROR("PResBuffer not available");
        return;
    }

    presBuffer->blitTransparent(srcX, srcY, width, height, dstX, dstY);
}

ST_API void st_pres_blit_from(int srcBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int dstBufferID = display->getActivePResBuffer();
    auto srcBuffer = display->getPResBuffer(srcBufferID);
    auto dstBuffer = display->getPResBuffer(dstBufferID);

    if (!srcBuffer || !dstBuffer) {
        ST_SET_ERROR("PResBuffer not available");
        return;
    }

    dstBuffer->blitFrom(srcBuffer.get(), srcX, srcY, width, height, dstX, dstY);
}

ST_API void st_pres_blit_from_trans(int srcBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int dstBufferID = display->getActivePResBuffer();
    auto srcBuffer = display->getPResBuffer(srcBufferID);
    auto dstBuffer = display->getPResBuffer(dstBufferID);

    if (!srcBuffer || !dstBuffer) {
        ST_SET_ERROR("PResBuffer not available");
        return;
    }

    dstBuffer->blitFromTransparent(srcBuffer.get(), srcX, srcY, width, height, dstX, dstY);
}

// GPU-accelerated PRES functions
ST_API void st_pres_blit_gpu(int srcBufferID, int dstBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }
    renderer->presBlitGPU(srcBufferID, dstBufferID, srcX, srcY, width, height, dstX, dstY);
}

ST_API void st_pres_blit_trans_gpu(int srcBufferID, int dstBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY, int transparentColor) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }
    renderer->presBlitTransparentGPU(srcBufferID, dstBufferID, srcX, srcY, width, height, dstX, dstY, static_cast<uint8_t>(transparentColor));
}

ST_API void st_pres_clear_gpu(int bufferID, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->presClearGPU(bufferID, static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API void st_pres_rect_fill_gpu(int bufferID, int x, int y, int width, int height, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->presRectFillGPU(bufferID, x, y, width, height, static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API void st_pres_circle_fill_gpu(int bufferID, int cx, int cy, int radius, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->presCircleFillGPU(bufferID, cx, cy, radius, static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API void st_pres_line_gpu(int bufferID, int x0, int y0, int x1, int y1, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->presLineGPU(bufferID, x0, y0, x1, y1, static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API void st_pres_circle_fill_aa(int bufferID, int cx, int cy, int radius, int colorIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->presCircleFillAA(bufferID, cx, cy, radius, static_cast<uint8_t>(colorIndex & 0xFF));
}

ST_API void st_pres_line_aa(int bufferID, int x0, int y0, int x1, int y1, int colorIndex, float lineWidth) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->presLineAA(bufferID, x0, y0, x1, y1, static_cast<uint8_t>(colorIndex & 0xFF), lineWidth);
}

// =============================================================================
// URES GPU Blitter API (Direct Color ARGB4444)
// =============================================================================

ST_API void st_ures_blit_copy_gpu(int srcBufferID, int dstBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->uresBlitCopyGPU(srcBufferID, dstBufferID, srcX, srcY, width, height, dstX, dstY);
}

ST_API void st_ures_blit_transparent_gpu(int srcBufferID, int dstBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->uresBlitTransparentGPU(srcBufferID, dstBufferID, srcX, srcY, width, height, dstX, dstY);
}

ST_API void st_ures_blit_alpha_composite_gpu(int srcBufferID, int dstBufferID, int srcX, int srcY, int width, int height, int dstX, int dstY) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->uresBlitAlphaCompositeGPU(srcBufferID, dstBufferID, srcX, srcY, width, height, dstX, dstY);
}

ST_API void st_ures_clear_gpu(int bufferID, int color) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->uresClearGPU(bufferID, static_cast<uint16_t>(color & 0xFFFF));
}

// =============================================================================
// URES GPU Primitive Drawing API (Direct Color ARGB4444)
// =============================================================================

ST_API void st_ures_rect_fill_gpu(int bufferID, int x, int y, int width, int height, int color) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->uresRectFillGPU(bufferID, x, y, width, height, static_cast<uint16_t>(color & 0xFFFF));
}

ST_API void st_ures_circle_fill_gpu(int bufferID, int cx, int cy, int radius, int color) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->uresCircleFillGPU(bufferID, cx, cy, radius, static_cast<uint16_t>(color & 0xFFFF));
}

ST_API void st_ures_line_gpu(int bufferID, int x0, int y0, int x1, int y1, int color) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->uresLineGPU(bufferID, x0, y0, x1, y1, static_cast<uint16_t>(color & 0xFFFF));
}

// =============================================================================
// URES GPU Anti-Aliased Primitive Drawing API (with TRUE alpha blending!)
// =============================================================================

ST_API void st_ures_circle_fill_aa(int bufferID, int cx, int cy, int radius, int color) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->uresCircleFillAA(bufferID, cx, cy, radius, static_cast<uint16_t>(color & 0xFFFF));
}

ST_API void st_ures_line_aa(int bufferID, int x0, int y0, int x1, int y1, int color, float lineWidth) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->uresLineAA(bufferID, x0, y0, x1, y1, static_cast<uint16_t>(color & 0xFFFF), lineWidth);
}

// =============================================================================
// URES GPU Gradient Primitive Drawing API
// =============================================================================

ST_API void st_ures_rect_fill_gradient_gpu(int bufferID, int x, int y, int width, int height,
                                           int colorTopLeft, int colorTopRight,
                                           int colorBottomLeft, int colorBottomRight) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->uresRectFillGradientGPU(bufferID, x, y, width, height,
                                      static_cast<uint16_t>(colorTopLeft & 0xFFFF),
                                      static_cast<uint16_t>(colorTopRight & 0xFFFF),
                                      static_cast<uint16_t>(colorBottomLeft & 0xFFFF),
                                      static_cast<uint16_t>(colorBottomRight & 0xFFFF));
}

ST_API void st_ures_circle_fill_gradient_gpu(int bufferID, int cx, int cy, int radius,
                                             int centerColor, int edgeColor) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->uresCircleFillGradientGPU(bufferID, cx, cy, radius,
                                        static_cast<uint16_t>(centerColor & 0xFFFF),
                                        static_cast<uint16_t>(edgeColor & 0xFFFF));
}

ST_API void st_ures_circle_fill_gradient_aa(int bufferID, int cx, int cy, int radius,
                                            int centerColor, int edgeColor) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto renderer = display->getRenderer();
    if (!renderer) {
        ST_SET_ERROR("MetalRenderer not available");
        return;
    }

    renderer->uresCircleFillGradientAA(bufferID, cx, cy, radius,
                                       static_cast<uint16_t>(centerColor & 0xFFFF),
                                       static_cast<uint16_t>(edgeColor & 0xFFFF));
}

// =============================================================================
// URES Color Utilities
// =============================================================================

ST_API int st_ures_pack_argb4(int a, int r, int g, int b) {
    // Clamp to 4-bit range (0-15)
    a = (a < 0) ? 0 : (a > 15) ? 15 : a;
    r = (r < 0) ? 0 : (r > 15) ? 15 : r;
    g = (g < 0) ? 0 : (g > 15) ? 15 : g;
    b = (b < 0) ? 0 : (b > 15) ? 15 : b;

    // Pack into ARGB4444: 0xARGB
    return (a << 12) | (r << 8) | (g << 4) | b;
}

ST_API int st_ures_pack_argb8(int a, int r, int g, int b) {
    // Clamp to 8-bit range (0-255)
    a = (a < 0) ? 0 : (a > 255) ? 255 : a;
    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;

    // Convert 8-bit to 4-bit by dividing by 17 (or >> 4)
    // Using division by 17 gives better rounding than shift
    return st_ures_pack_argb4(a / 17, r / 17, g / 17, b / 17);
}

ST_API void st_ures_unpack_argb4(int color, int* out_a, int* out_r, int* out_g, int* out_b) {
    if (out_a) *out_a = (color >> 12) & 0xF;
    if (out_r) *out_r = (color >> 8) & 0xF;
    if (out_g) *out_g = (color >> 4) & 0xF;
    if (out_b) *out_b = color & 0xF;
}

ST_API void st_ures_unpack_argb8(int color, int* out_a, int* out_r, int* out_g, int* out_b) {
    // Extract 4-bit components
    int a4 = (color >> 12) & 0xF;
    int r4 = (color >> 8) & 0xF;
    int g4 = (color >> 4) & 0xF;
    int b4 = color & 0xF;

    // Convert to 8-bit by multiplying by 17 (scales 0-15 to 0-255)
    if (out_a) *out_a = a4 * 17;
    if (out_r) *out_r = r4 * 17;
    if (out_g) *out_g = g4 * 17;
    if (out_b) *out_b = b4 * 17;
}

ST_API int st_ures_blend_colors(int src, int dst) {
    // Unpack source and destination
    int sa = (src >> 12) & 0xF;
    int sr = (src >> 8) & 0xF;
    int sg = (src >> 4) & 0xF;
    int sb = src & 0xF;

    int da = (dst >> 12) & 0xF;
    int dr = (dst >> 8) & 0xF;
    int dg = (dst >> 4) & 0xF;
    int db = dst & 0xF;

    // Porter-Duff "over": out = src + dst * (1 - src.alpha)
    // Scale alpha from 0-15 to 0-255 for better precision, then scale back
    float src_alpha = sa / 15.0f;
    float inv_alpha = 1.0f - src_alpha;

    int out_a = sa + static_cast<int>(da * inv_alpha + 0.5f);
    int out_r = sr + static_cast<int>(dr * inv_alpha + 0.5f);
    int out_g = sg + static_cast<int>(dg * inv_alpha + 0.5f);
    int out_b = sb + static_cast<int>(db * inv_alpha + 0.5f);

    // Clamp to 4-bit range
    out_a = (out_a > 15) ? 15 : out_a;
    out_r = (out_r > 15) ? 15 : out_r;
    out_g = (out_g > 15) ? 15 : out_g;
    out_b = (out_b > 15) ? 15 : out_b;

    return (out_a << 12) | (out_r << 8) | (out_g << 4) | out_b;
}

ST_API int st_ures_lerp_colors(int color1, int color2, float t) {
    // Clamp t to [0, 1]
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    // Unpack both colors
    int a1 = (color1 >> 12) & 0xF;
    int r1 = (color1 >> 8) & 0xF;
    int g1 = (color1 >> 4) & 0xF;
    int b1 = color1 & 0xF;

    int a2 = (color2 >> 12) & 0xF;
    int r2 = (color2 >> 8) & 0xF;
    int g2 = (color2 >> 4) & 0xF;
    int b2 = color2 & 0xF;

    // Linear interpolation
    int a = static_cast<int>(a1 + (a2 - a1) * t + 0.5f);
    int r = static_cast<int>(r1 + (r2 - r1) * t + 0.5f);
    int g = static_cast<int>(g1 + (g2 - g1) * t + 0.5f);
    int b = static_cast<int>(b1 + (b2 - b1) * t + 0.5f);

    return (a << 12) | (r << 8) | (g << 4) | b;
}

ST_API int st_ures_color_from_hsv(float h, float s, float v, int a) {
    // Clamp inputs
    while (h < 0.0f) h += 360.0f;
    while (h >= 360.0f) h -= 360.0f;
    if (s < 0.0f) s = 0.0f;
    if (s > 1.0f) s = 1.0f;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    if (a < 0) a = 0;
    if (a > 15) a = 15;

    // HSV to RGB conversion
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;

    float r_f, g_f, b_f;

    if (h < 60.0f) {
        r_f = c; g_f = x; b_f = 0.0f;
    } else if (h < 120.0f) {
        r_f = x; g_f = c; b_f = 0.0f;
    } else if (h < 180.0f) {
        r_f = 0.0f; g_f = c; b_f = x;
    } else if (h < 240.0f) {
        r_f = 0.0f; g_f = x; b_f = c;
    } else if (h < 300.0f) {
        r_f = x; g_f = 0.0f; b_f = c;
    } else {
        r_f = c; g_f = 0.0f; b_f = x;
    }

    // Add m and scale to 0-15 range
    int r = static_cast<int>((r_f + m) * 15.0f + 0.5f);
    int g = static_cast<int>((g_f + m) * 15.0f + 0.5f);
    int b = static_cast<int>((b_f + m) * 15.0f + 0.5f);

    // Clamp (shouldn't be necessary but just in case)
    r = (r > 15) ? 15 : r;
    g = (g > 15) ? 15 : g;
    b = (b > 15) ? 15 : b;

    return (a << 12) | (r << 8) | (g << 4) | b;
}

ST_API int st_ures_adjust_brightness(int color, float factor) {
    // Unpack color
    int a = (color >> 12) & 0xF;
    int r = (color >> 8) & 0xF;
    int g = (color >> 4) & 0xF;
    int b = color & 0xF;

    // Apply brightness factor
    r = static_cast<int>(r * factor + 0.5f);
    g = static_cast<int>(g * factor + 0.5f);
    b = static_cast<int>(b * factor + 0.5f);

    // Clamp to 4-bit range
    r = (r < 0) ? 0 : (r > 15) ? 15 : r;
    g = (g < 0) ? 0 : (g > 15) ? 15 : g;
    b = (b < 0) ? 0 : (b > 15) ? 15 : b;

    return (a << 12) | (r << 8) | (g << 4) | b;
}

ST_API int st_ures_set_alpha(int color, int alpha) {
    // Clamp alpha to 4-bit range
    alpha = (alpha < 0) ? 0 : (alpha > 15) ? 15 : alpha;

    // Replace alpha bits, keep RGB
    return (color & 0x0FFF) | (alpha << 12);
}

ST_API int st_ures_get_alpha(int color) {
    return (color >> 12) & 0xF;
}

ST_API void st_wres_palette_row(int row, int index, int r, int g, int b) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.wresPalette(), "WResPaletteManager");

    // Validate parameters
    if (row < 0 || row >= 240) {
        ST_SET_ERROR("Row must be 0-239");
        return;
    }
    if (index < 0 || index >= 16) {
        ST_SET_ERROR("Per-row palette index must be 0-15");
        return;
    }

    // Clamp RGB to 0-255
    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;

    // Set per-row palette entry in WResPaletteManager
    uint32_t rgba = 0xFF000000 | (r << 16) | (g << 8) | b;
    ST_CONTEXT.wresPalette()->setPerRowColor(row, index, rgba);
}

ST_API void st_wres_palette_global(int index, int r, int g, int b) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.wresPalette(), "WResPaletteManager");

    // Validate parameters
    if (index < 16 || index >= 256) {
        ST_SET_ERROR("Global palette index must be 16-255");
        return;
    }

    // Clamp RGB to 0-255
    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;

    // Set global palette entry in WResPaletteManager
    uint32_t rgba = 0xFF000000 | (r << 16) | (g << 8) | b;
    ST_CONTEXT.wresPalette()->setGlobalColor(index, rgba);
}

ST_API int st_wrgb(int r, int g, int b) {
    // Calculate global palette index for 6×8×5 RGB cube (indices 16-255)
    // 6 red levels (0-5) × 8 green levels (0-7) × 5 blue levels (0-4) = 240 colors
    // Same cube as XRES

    // Clamp RGB to 0-255
    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;

    // Map 0-255 to cube indices
    int rIndex = (r * 5) / 255;  // 0-5
    int gIndex = (g * 7) / 255;  // 0-7
    int bIndex = (b * 4) / 255;  // 0-4

    // Clamp to valid ranges
    rIndex = (rIndex < 0) ? 0 : (rIndex > 5) ? 5 : rIndex;
    gIndex = (gIndex < 0) ? 0 : (gIndex > 7) ? 7 : gIndex;
    bIndex = (bIndex < 0) ? 0 : (bIndex > 4) ? 4 : bIndex;

    // Calculate index: 16 + r*40 + g*5 + b
    int index = 16 + (rIndex * 40) + (gIndex * 5) + bIndex;

    // Safety clamp
    if (index < 16) index = 16;
    if (index > 255) index = 255;

    return index;
}

ST_API void st_wres_palette_reset() {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.wresPalette(), "WResPaletteManager");
    
    // Reset to default palette (IBM RGBI for 0-15, RGB cube for 16-255)
    ST_CONTEXT.wresPalette()->loadPresetPalette(WResPalettePreset::IBM_RGBI);
    ST_CONTEXT.wresPalette()->loadPresetPalette(WResPalettePreset::RGB_CUBE_6x8x5);
}

// --- WRES Palette Automation (Copper-style effects) ---

ST_API void st_wres_palette_auto_gradient(int paletteIndex, int startRow, int endRow,
                                           int startR, int startG, int startB,
                                           int endR, int endG, int endB,
                                           float speed) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.wresPalette(), "WResPaletteManager");
    
    ST_CONTEXT.wresPalette()->enableGradientAutomation(paletteIndex, startRow, endRow,
                                                        startR, startG, startB,
                                                        endR, endG, endB,
                                                        speed);
}

ST_API void st_wres_palette_auto_bars(int paletteIndex, int startRow, int endRow,
                                       int barHeight, int numColors,
                                       int r1, int g1, int b1,
                                       int r2, int g2, int b2,
                                       int r3, int g3, int b3,
                                       int r4, int g4, int b4,
                                       float speed) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.wresPalette(), "WResPaletteManager");
    
    // Build colors array
    uint8_t colors[4][3] = {
        {static_cast<uint8_t>(r1), static_cast<uint8_t>(g1), static_cast<uint8_t>(b1)},
        {static_cast<uint8_t>(r2), static_cast<uint8_t>(g2), static_cast<uint8_t>(b2)},
        {static_cast<uint8_t>(r3), static_cast<uint8_t>(g3), static_cast<uint8_t>(b3)},
        {static_cast<uint8_t>(r4), static_cast<uint8_t>(g4), static_cast<uint8_t>(b4)}
    };
    
    ST_CONTEXT.wresPalette()->enableBarsAutomation(paletteIndex, startRow, endRow,
                                                    barHeight, colors, numColors,
                                                    speed);
}

ST_API void st_wres_palette_auto_stop() {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.wresPalette(), "WResPaletteManager");
    
    ST_CONTEXT.wresPalette()->disableAutomation();
}

ST_API void st_wres_palette_auto_update(float deltaTime) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.wresPalette(), "WResPaletteManager");
    
    ST_CONTEXT.wresPalette()->updateAutomation(deltaTime);
}

// =============================================================================
// Display API - PRES Buffer Operations (Premium Resolution: 1280×720, 256-color palette)
// =============================================================================

ST_API void st_pres_palette_row(int row, int index, int r, int g, int b) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.presPalette(), "PResPaletteManager");

    // Validate parameters
    if (row < 0 || row >= 720) {
        ST_SET_ERROR("Row must be 0-719");
        return;
    }
    if (index < 0 || index >= 16) {
        ST_SET_ERROR("Per-row palette index must be 0-15");
        return;
    }

    // Clamp RGB to 0-255
    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;

    // Set per-row palette entry in PResPaletteManager
    uint32_t rgba = 0xFF000000 | (r << 16) | (g << 8) | b;
    ST_CONTEXT.presPalette()->setPerRowColor(row, index, rgba);
}

ST_API void st_pres_palette_global(int index, int r, int g, int b) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.presPalette(), "PResPaletteManager");

    // Validate parameters
    if (index < 16 || index >= 256) {
        ST_SET_ERROR("Global palette index must be 16-255");
        return;
    }

    // Clamp RGB to 0-255
    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;

    // Set global palette entry in PResPaletteManager
    uint32_t rgba = 0xFF000000 | (r << 16) | (g << 8) | b;
    ST_CONTEXT.presPalette()->setGlobalColor(index, rgba);
}

ST_API int st_prgb(int r, int g, int b) {
    // Calculate global palette index for 6×8×5 RGB cube (indices 16-255)
    // 6 red levels (0-5) × 8 green levels (0-7) × 5 blue levels (0-4) = 240 colors
    // Same cube as XRES/WRES

    // Clamp RGB to 0-255
    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;

    // Map 0-255 to cube indices
    int rIndex = (r * 5) / 255;  // 0-5
    int gIndex = (g * 7) / 255;  // 0-7
    int bIndex = (b * 4) / 255;  // 0-4

    // Clamp to valid ranges
    rIndex = (rIndex < 0) ? 0 : (rIndex > 5) ? 5 : rIndex;
    gIndex = (gIndex < 0) ? 0 : (gIndex > 7) ? 7 : gIndex;
    bIndex = (bIndex < 0) ? 0 : (bIndex > 4) ? 4 : bIndex;

    // Calculate index: 16 + r*40 + g*5 + b
    int index = 16 + (rIndex * 40) + (gIndex * 5) + bIndex;

    // Safety clamp
    if (index < 16) index = 16;
    if (index > 255) index = 255;

    return index;
}

ST_API void st_pres_palette_reset() {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.presPalette(), "PResPaletteManager");
    
    // Reset to default palette (IBM RGBI for 0-15, RGB cube for 16-255)
    ST_CONTEXT.presPalette()->loadPresetPalette(PResPalettePreset::IBM_RGBI);
    ST_CONTEXT.presPalette()->loadPresetPalette(PResPalettePreset::RGB_CUBE_6x8x5);
}

// --- PRES Palette Automation (Copper-style effects) ---

ST_API void st_pres_palette_auto_gradient(int paletteIndex, int startRow, int endRow,
                                           int startR, int startG, int startB,
                                           int endR, int endG, int endB,
                                           float speed) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.presPalette(), "PResPaletteManager");
    
    ST_CONTEXT.presPalette()->enableGradientAutomation(paletteIndex, startRow, endRow,
                                                        startR, startG, startB,
                                                        endR, endG, endB,
                                                        speed);
}

ST_API void st_pres_palette_auto_bars(int paletteIndex, int startRow, int endRow,
                                       int barHeight, int numColors,
                                       int r1, int g1, int b1,
                                       int r2, int g2, int b2,
                                       int r3, int g3, int b3,
                                       int r4, int g4, int b4,
                                       float speed) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.presPalette(), "PResPaletteManager");
    
    // Build colors array
    uint8_t colors[4][3] = {
        {static_cast<uint8_t>(r1), static_cast<uint8_t>(g1), static_cast<uint8_t>(b1)},
        {static_cast<uint8_t>(r2), static_cast<uint8_t>(g2), static_cast<uint8_t>(b2)},
        {static_cast<uint8_t>(r3), static_cast<uint8_t>(g3), static_cast<uint8_t>(b3)},
        {static_cast<uint8_t>(r4), static_cast<uint8_t>(g4), static_cast<uint8_t>(b4)}
    };
    
    ST_CONTEXT.presPalette()->enableBarsAutomation(paletteIndex, startRow, endRow,
                                                    barHeight, colors, numColors,
                                                    speed);
}

ST_API void st_pres_palette_auto_stop() {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.presPalette(), "PResPaletteManager");
    
    ST_CONTEXT.presPalette()->disableAutomation();
}

ST_API void st_pres_palette_auto_update(float deltaTime) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.presPalette(), "PResPaletteManager");
    
    ST_CONTEXT.presPalette()->updateAutomation(deltaTime);
}

// =============================================================================
// Palette Animation Helpers
// =============================================================================

// --- XRES Palette Animation ---

ST_API void st_xres_palette_rotate_row(int row, int startIndex, int endIndex, int direction) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.xresPalette(), "XResPaletteManager");

    // Validate parameters
    if (row < 0 || row >= 240) {
        ST_SET_ERROR("Row must be 0-239");
        return;
    }
    if (startIndex < 0 || startIndex >= 16 || endIndex < 0 || endIndex >= 16) {
        ST_SET_ERROR("Indices must be 0-15");
        return;
    }
    if (startIndex > endIndex) {
        ST_SET_ERROR("startIndex must be <= endIndex");
        return;
    }

    auto palette = ST_CONTEXT.xresPalette();
    int count = endIndex - startIndex + 1;
    if (count <= 1) return; // Nothing to rotate

    // Get all colors in range
    std::vector<uint32_t> colors(count);
    for (int i = 0; i < count; i++) {
        colors[i] = palette->getPerRowColor(row, startIndex + i);
    }

    // Rotate
    if (direction > 0) {
        // Forward: last becomes first
        uint32_t temp = colors[count - 1];
        for (int i = count - 1; i > 0; i--) {
            colors[i] = colors[i - 1];
        }
        colors[0] = temp;
    } else if (direction < 0) {
        // Backward: first becomes last
        uint32_t temp = colors[0];
        for (int i = 0; i < count - 1; i++) {
            colors[i] = colors[i + 1];
        }
        colors[count - 1] = temp;
    }

    // Write back
    for (int i = 0; i < count; i++) {
        palette->setPerRowColor(row, startIndex + i, colors[i]);
    }
}

ST_API void st_xres_palette_rotate_global(int startIndex, int endIndex, int direction) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.xresPalette(), "XResPaletteManager");

    // Validate parameters
    if (startIndex < 16 || startIndex >= 256 || endIndex < 16 || endIndex >= 256) {
        ST_SET_ERROR("Indices must be 16-255");
        return;
    }
    if (startIndex > endIndex) {
        ST_SET_ERROR("startIndex must be <= endIndex");
        return;
    }

    auto palette = ST_CONTEXT.xresPalette();
    int count = endIndex - startIndex + 1;
    if (count <= 1) return;

    // Get all colors in range
    std::vector<uint32_t> colors(count);
    for (int i = 0; i < count; i++) {
        colors[i] = palette->getGlobalColor(startIndex + i);
    }

    // Rotate
    if (direction > 0) {
        uint32_t temp = colors[count - 1];
        for (int i = count - 1; i > 0; i--) {
            colors[i] = colors[i - 1];
        }
        colors[0] = temp;
    } else if (direction < 0) {
        uint32_t temp = colors[0];
        for (int i = 0; i < count - 1; i++) {
            colors[i] = colors[i + 1];
        }
        colors[count - 1] = temp;
    }

    // Write back
    for (int i = 0; i < count; i++) {
        palette->setGlobalColor(startIndex + i, colors[i]);
    }
}

ST_API void st_xres_palette_copy_row(int srcRow, int dstRow) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.xresPalette(), "XResPaletteManager");

    if (srcRow < 0 || srcRow >= 240 || dstRow < 0 || dstRow >= 240) {
        ST_SET_ERROR("Row must be 0-239");
        return;
    }

    auto palette = ST_CONTEXT.xresPalette();
    for (int i = 0; i < 16; i++) {
        uint32_t color = palette->getPerRowColor(srcRow, i);
        palette->setPerRowColor(dstRow, i, color);
    }
}

ST_API void st_xres_palette_lerp_row(int row, int index, int r1, int g1, int b1, int r2, int g2, int b2, float t) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.xresPalette(), "XResPaletteManager");

    if (row < 0 || row >= 240) {
        ST_SET_ERROR("Row must be 0-239");
        return;
    }
    if (index < 0 || index >= 16) {
        ST_SET_ERROR("Index must be 0-15");
        return;
    }

    // Clamp t
    t = (t < 0.0f) ? 0.0f : (t > 1.0f) ? 1.0f : t;

    // Interpolate
    int r = (int)(r1 + (r2 - r1) * t);
    int g = (int)(g1 + (g2 - g1) * t);
    int b = (int)(b1 + (b2 - b1) * t);

    // Clamp to 0-255
    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;

    uint32_t rgba = 0xFF000000 | (r << 16) | (g << 8) | b;
    ST_CONTEXT.xresPalette()->setPerRowColor(row, index, rgba);
}

ST_API void st_xres_palette_lerp_global(int index, int r1, int g1, int b1, int r2, int g2, int b2, float t) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.xresPalette(), "XResPaletteManager");

    if (index < 16 || index >= 256) {
        ST_SET_ERROR("Index must be 16-255");
        return;
    }

    // Clamp t
    t = (t < 0.0f) ? 0.0f : (t > 1.0f) ? 1.0f : t;

    // Interpolate
    int r = (int)(r1 + (r2 - r1) * t);
    int g = (int)(g1 + (g2 - g1) * t);
    int b = (int)(b1 + (b2 - b1) * t);

    // Clamp to 0-255
    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;

    uint32_t rgba = 0xFF000000 | (r << 16) | (g << 8) | b;
    ST_CONTEXT.xresPalette()->setGlobalColor(index, rgba);
}

// --- XRES Palette Automation (Copper-style effects) ---

ST_API void st_xres_palette_auto_gradient(int paletteIndex, int startRow, int endRow,
                                           int startR, int startG, int startB,
                                           int endR, int endG, int endB,
                                           float speed) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.xresPalette(), "XResPaletteManager");
    
    ST_CONTEXT.xresPalette()->enableGradientAutomation(paletteIndex, startRow, endRow,
                                                        startR, startG, startB,
                                                        endR, endG, endB,
                                                        speed);
}

ST_API void st_xres_palette_auto_bars(int paletteIndex, int startRow, int endRow,
                                       int barHeight, int numColors,
                                       int r1, int g1, int b1,
                                       int r2, int g2, int b2,
                                       int r3, int g3, int b3,
                                       int r4, int g4, int b4,
                                       float speed) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.xresPalette(), "XResPaletteManager");
    
    // Build colors array
    uint8_t colors[4][3] = {
        {static_cast<uint8_t>(r1), static_cast<uint8_t>(g1), static_cast<uint8_t>(b1)},
        {static_cast<uint8_t>(r2), static_cast<uint8_t>(g2), static_cast<uint8_t>(b2)},
        {static_cast<uint8_t>(r3), static_cast<uint8_t>(g3), static_cast<uint8_t>(b3)},
        {static_cast<uint8_t>(r4), static_cast<uint8_t>(g4), static_cast<uint8_t>(b4)}
    };
    
    ST_CONTEXT.xresPalette()->enableBarsAutomation(paletteIndex, startRow, endRow,
                                                    barHeight, colors, numColors,
                                                    speed);
}

ST_API void st_xres_palette_auto_stop() {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.xresPalette(), "XResPaletteManager");
    
    ST_CONTEXT.xresPalette()->disableAutomation();
}

ST_API void st_xres_palette_auto_update(float deltaTime) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.xresPalette(), "XResPaletteManager");
    
    ST_CONTEXT.xresPalette()->updateAutomation(deltaTime);
}

// --- XRES Gradient Drawing Functions ---

ST_API void st_xres_palette_make_ramp(int row, int startIndex, int endIndex, 
                                       int r1, int g1, int b1, int r2, int g2, int b2) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.xresPalette(), "XResPaletteManager");

    if (startIndex < 0 || startIndex > 255 || endIndex < 0 || endIndex > 255) {
        ST_SET_ERROR("Indices must be 0-255");
        return;
    }
    if (startIndex > endIndex) {
        ST_SET_ERROR("startIndex must be <= endIndex");
        return;
    }

    auto palette = ST_CONTEXT.xresPalette();
    int count = endIndex - startIndex + 1;

    // Generate gradient ramp
    for (int i = 0; i < count; i++) {
        float t = (count > 1) ? (float)i / (float)(count - 1) : 0.0f;
        int r = (int)(r1 + (r2 - r1) * t);
        int g = (int)(g1 + (g2 - g1) * t);
        int b = (int)(b1 + (b2 - b1) * t);

        // Clamp
        r = (r < 0) ? 0 : (r > 255) ? 255 : r;
        g = (g < 0) ? 0 : (g > 255) ? 255 : g;
        b = (b < 0) ? 0 : (b > 255) ? 255 : b;

        uint32_t rgba = 0xFF000000 | (r << 16) | (g << 8) | b;
        int index = startIndex + i;

        if (row == -1) {
            // Global palette
            if (index >= 16 && index < 256) {
                palette->setGlobalColor(index, rgba);
            }
        } else {
            // Per-row palette
            if (row >= 0 && row < 240 && index >= 0 && index < 16) {
                palette->setPerRowColor(row, index, rgba);
            }
        }
    }
}

ST_API void st_xres_gradient_h(int x, int y, int width, int height, 
                                uint8_t startIndex, uint8_t endIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveXResBuffer();
    auto xresBuffer = display->getXResBuffer(bufferID);

    if (!xresBuffer) {
        ST_SET_ERROR("XResBuffer not available");
        return;
    }

    if (width <= 0 || height <= 0) return;

    // Clip to buffer bounds
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > 320) width = 320 - x;
    if (y + height > 240) height = 240 - y;
    if (width <= 0 || height <= 0) return;

    // Draw horizontal gradient
    for (int dy = 0; dy < height; dy++) {
        for (int dx = 0; dx < width; dx++) {
            float t = (width > 1) ? (float)dx / (float)(width - 1) : 0.0f;
            int idx = (int)(startIndex + (endIndex - startIndex) * t);
            idx = (idx < 0) ? 0 : (idx > 255) ? 255 : idx;
            xresBuffer->setPixel(x + dx, y + dy, (uint8_t)idx);
        }
    }
}

ST_API void st_xres_gradient_v(int x, int y, int width, int height, 
                                uint8_t startIndex, uint8_t endIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveXResBuffer();
    auto xresBuffer = display->getXResBuffer(bufferID);

    if (!xresBuffer) {
        ST_SET_ERROR("XResBuffer not available");
        return;
    }

    if (width <= 0 || height <= 0) return;

    // Clip to buffer bounds
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > 320) width = 320 - x;
    if (y + height > 240) height = 240 - y;
    if (width <= 0 || height <= 0) return;

    // Draw vertical gradient
    for (int dy = 0; dy < height; dy++) {
        float t = (height > 1) ? (float)dy / (float)(height - 1) : 0.0f;
        int idx = (int)(startIndex + (endIndex - startIndex) * t);
        idx = (idx < 0) ? 0 : (idx > 255) ? 255 : idx;
        xresBuffer->hline(x, y + dy, width, (uint8_t)idx);
    }
}

ST_API void st_xres_gradient_radial(int cx, int cy, int radius, 
                                     uint8_t centerIndex, uint8_t edgeIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveXResBuffer();
    auto xresBuffer = display->getXResBuffer(bufferID);

    if (!xresBuffer) {
        ST_SET_ERROR("XResBuffer not available");
        return;
    }

    if (radius <= 0) return;

    float radiusSq = (float)(radius * radius);

    // Bounding box
    int minX = cx - radius;
    int maxX = cx + radius;
    int minY = cy - radius;
    int maxY = cy + radius;

    // Clip to buffer bounds
    if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;
    if (maxX >= 320) maxX = 319;
    if (maxY >= 240) maxY = 239;

    // Draw radial gradient
    for (int py = minY; py <= maxY; py++) {
        for (int px = minX; px <= maxX; px++) {
            int dx = px - cx;
            int dy = py - cy;
            float distSq = (float)(dx * dx + dy * dy);

            if (distSq <= radiusSq) {
                float t = (radius > 0) ? sqrtf(distSq) / (float)radius : 0.0f;
                t = (t > 1.0f) ? 1.0f : t;
                int idx = (int)(centerIndex + (edgeIndex - centerIndex) * t);
                idx = (idx < 0) ? 0 : (idx > 255) ? 255 : idx;
                xresBuffer->setPixel(px, py, (uint8_t)idx);
            }
        }
    }
}

ST_API void st_xres_gradient_corners(int x, int y, int width, int height, 
                                      uint8_t tlIndex, uint8_t trIndex, 
                                      uint8_t blIndex, uint8_t brIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveXResBuffer();
    auto xresBuffer = display->getXResBuffer(bufferID);

    if (!xresBuffer) {
        ST_SET_ERROR("XResBuffer not available");
        return;
    }

    if (width <= 0 || height <= 0) return;

    // Clip to buffer bounds
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > 320) width = 320 - x;
    if (y + height > 240) height = 240 - y;
    if (width <= 0 || height <= 0) return;

    // Draw four-corner gradient (bilinear interpolation)
    for (int dy = 0; dy < height; dy++) {
        float ty = (height > 1) ? (float)dy / (float)(height - 1) : 0.0f;
        for (int dx = 0; dx < width; dx++) {
            float tx = (width > 1) ? (float)dx / (float)(width - 1) : 0.0f;

            // Bilinear interpolation
            float top = tlIndex + (trIndex - tlIndex) * tx;
            float bottom = blIndex + (brIndex - blIndex) * tx;
            float value = top + (bottom - top) * ty;

            int idx = (int)value;
            idx = (idx < 0) ? 0 : (idx > 255) ? 255 : idx;
            xresBuffer->setPixel(x + dx, y + dy, (uint8_t)idx);
        }
    }
}

// --- WRES Palette Animation ---

ST_API void st_wres_palette_rotate_row(int row, int startIndex, int endIndex, int direction) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.wresPalette(), "WResPaletteManager");

    if (row < 0 || row >= 240) {
        ST_SET_ERROR("Row must be 0-239");
        return;
    }
    if (startIndex < 0 || startIndex >= 16 || endIndex < 0 || endIndex >= 16) {
        ST_SET_ERROR("Indices must be 0-15");
        return;
    }
    if (startIndex > endIndex) {
        ST_SET_ERROR("startIndex must be <= endIndex");
        return;
    }

    auto palette = ST_CONTEXT.wresPalette();
    int count = endIndex - startIndex + 1;
    if (count <= 1) return;

    std::vector<uint32_t> colors(count);
    for (int i = 0; i < count; i++) {
        colors[i] = palette->getPerRowColor(row, startIndex + i);
    }

    if (direction > 0) {
        uint32_t temp = colors[count - 1];
        for (int i = count - 1; i > 0; i--) {
            colors[i] = colors[i - 1];
        }
        colors[0] = temp;
    } else if (direction < 0) {
        uint32_t temp = colors[0];
        for (int i = 0; i < count - 1; i++) {
            colors[i] = colors[i + 1];
        }
        colors[count - 1] = temp;
    }

    for (int i = 0; i < count; i++) {
        palette->setPerRowColor(row, startIndex + i, colors[i]);
    }
}

ST_API void st_wres_palette_rotate_global(int startIndex, int endIndex, int direction) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.wresPalette(), "WResPaletteManager");

    if (startIndex < 16 || startIndex >= 256 || endIndex < 16 || endIndex >= 256) {
        ST_SET_ERROR("Indices must be 16-255");
        return;
    }
    if (startIndex > endIndex) {
        ST_SET_ERROR("startIndex must be <= endIndex");
        return;
    }

    auto palette = ST_CONTEXT.wresPalette();
    int count = endIndex - startIndex + 1;
    if (count <= 1) return;

    std::vector<uint32_t> colors(count);
    for (int i = 0; i < count; i++) {
        colors[i] = palette->getGlobalColor(startIndex + i);
    }

    if (direction > 0) {
        uint32_t temp = colors[count - 1];
        for (int i = count - 1; i > 0; i--) {
            colors[i] = colors[i - 1];
        }
        colors[0] = temp;
    } else if (direction < 0) {
        uint32_t temp = colors[0];
        for (int i = 0; i < count - 1; i++) {
            colors[i] = colors[i + 1];
        }
        colors[count - 1] = temp;
    }

    for (int i = 0; i < count; i++) {
        palette->setGlobalColor(startIndex + i, colors[i]);
    }
}

ST_API void st_wres_palette_copy_row(int srcRow, int dstRow) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.wresPalette(), "WResPaletteManager");

    if (srcRow < 0 || srcRow >= 240 || dstRow < 0 || dstRow >= 240) {
        ST_SET_ERROR("Row must be 0-239");
        return;
    }

    auto palette = ST_CONTEXT.wresPalette();
    for (int i = 0; i < 16; i++) {
        uint32_t color = palette->getPerRowColor(srcRow, i);
        palette->setPerRowColor(dstRow, i, color);
    }
}

ST_API void st_wres_palette_lerp_row(int row, int index, int r1, int g1, int b1, int r2, int g2, int b2, float t) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.wresPalette(), "WResPaletteManager");

    if (row < 0 || row >= 240) {
        ST_SET_ERROR("Row must be 0-239");
        return;
    }
    if (index < 0 || index >= 16) {
        ST_SET_ERROR("Index must be 0-15");
        return;
    }

    t = (t < 0.0f) ? 0.0f : (t > 1.0f) ? 1.0f : t;

    int r = (int)(r1 + (r2 - r1) * t);
    int g = (int)(g1 + (g2 - g1) * t);
    int b = (int)(b1 + (b2 - b1) * t);

    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;

    uint32_t rgba = 0xFF000000 | (r << 16) | (g << 8) | b;
    ST_CONTEXT.wresPalette()->setPerRowColor(row, index, rgba);
}

ST_API void st_wres_palette_lerp_global(int index, int r1, int g1, int b1, int r2, int g2, int b2, float t) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.wresPalette(), "WResPaletteManager");

    if (index < 16 || index >= 256) {
        ST_SET_ERROR("Index must be 16-255");
        return;
    }

    t = (t < 0.0f) ? 0.0f : (t > 1.0f) ? 1.0f : t;

    int r = (int)(r1 + (r2 - r1) * t);
    int g = (int)(g1 + (g2 - g1) * t);
    int b = (int)(b1 + (b2 - b1) * t);

    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;

    uint32_t rgba = 0xFF000000 | (r << 16) | (g << 8) | b;
    ST_CONTEXT.wresPalette()->setGlobalColor(index, rgba);
}

// --- WRES Gradient Drawing Functions ---

ST_API void st_wres_palette_make_ramp(int row, int startIndex, int endIndex, 
                                       int r1, int g1, int b1, int r2, int g2, int b2) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.wresPalette(), "WResPaletteManager");

    if (startIndex < 0 || startIndex > 255 || endIndex < 0 || endIndex > 255) {
        ST_SET_ERROR("Indices must be 0-255");
        return;
    }
    if (startIndex > endIndex) {
        ST_SET_ERROR("startIndex must be <= endIndex");
        return;
    }

    auto palette = ST_CONTEXT.wresPalette();
    int count = endIndex - startIndex + 1;

    // Generate gradient ramp
    for (int i = 0; i < count; i++) {
        float t = (count > 1) ? (float)i / (float)(count - 1) : 0.0f;
        int r = (int)(r1 + (r2 - r1) * t);
        int g = (int)(g1 + (g2 - g1) * t);
        int b = (int)(b1 + (b2 - b1) * t);

        // Clamp
        r = (r < 0) ? 0 : (r > 255) ? 255 : r;
        g = (g < 0) ? 0 : (g > 255) ? 255 : g;
        b = (b < 0) ? 0 : (b > 255) ? 255 : b;

        uint32_t rgba = 0xFF000000 | (r << 16) | (g << 8) | b;
        int index = startIndex + i;

        if (row == -1) {
            // Global palette
            if (index >= 16 && index < 256) {
                palette->setGlobalColor(index, rgba);
            }
        } else {
            // Per-row palette
            if (row >= 0 && row < 240 && index >= 0 && index < 16) {
                palette->setPerRowColor(row, index, rgba);
            }
        }
    }
}

ST_API void st_wres_gradient_h(int x, int y, int width, int height, 
                                uint8_t startIndex, uint8_t endIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveWResBuffer();
    auto wresBuffer = display->getWResBuffer(bufferID);

    if (!wresBuffer) {
        ST_SET_ERROR("WResBuffer not available");
        return;
    }

    if (width <= 0 || height <= 0) return;

    // Clip to buffer bounds
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > 432) width = 432 - x;
    if (y + height > 240) height = 240 - y;
    if (width <= 0 || height <= 0) return;

    // Draw horizontal gradient
    for (int dy = 0; dy < height; dy++) {
        for (int dx = 0; dx < width; dx++) {
            float t = (width > 1) ? (float)dx / (float)(width - 1) : 0.0f;
            int idx = (int)(startIndex + (endIndex - startIndex) * t);
            idx = (idx < 0) ? 0 : (idx > 255) ? 255 : idx;
            wresBuffer->setPixel(x + dx, y + dy, (uint8_t)idx);
        }
    }
}

ST_API void st_wres_gradient_v(int x, int y, int width, int height, 
                                uint8_t startIndex, uint8_t endIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveWResBuffer();
    auto wresBuffer = display->getWResBuffer(bufferID);

    if (!wresBuffer) {
        ST_SET_ERROR("WResBuffer not available");
        return;
    }

    if (width <= 0 || height <= 0) return;

    // Clip to buffer bounds
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > 432) width = 432 - x;
    if (y + height > 240) height = 240 - y;
    if (width <= 0 || height <= 0) return;

    // Draw vertical gradient
    for (int dy = 0; dy < height; dy++) {
        float t = (height > 1) ? (float)dy / (float)(height - 1) : 0.0f;
        int idx = (int)(startIndex + (endIndex - startIndex) * t);
        idx = (idx < 0) ? 0 : (idx > 255) ? 255 : idx;
        wresBuffer->hline(x, y + dy, width, (uint8_t)idx);
    }
}

ST_API void st_wres_gradient_radial(int cx, int cy, int radius, 
                                     uint8_t centerIndex, uint8_t edgeIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveWResBuffer();
    auto wresBuffer = display->getWResBuffer(bufferID);

    if (!wresBuffer) {
        ST_SET_ERROR("WResBuffer not available");
        return;
    }

    if (radius <= 0) return;

    float radiusSq = (float)(radius * radius);

    // Bounding box
    int minX = cx - radius;
    int maxX = cx + radius;
    int minY = cy - radius;
    int maxY = cy + radius;

    // Clip to buffer bounds
    if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;
    if (maxX >= 432) maxX = 431;
    if (maxY >= 240) maxY = 239;

    // Draw radial gradient
    for (int py = minY; py <= maxY; py++) {
        for (int px = minX; px <= maxX; px++) {
            int dx = px - cx;
            int dy = py - cy;
            float distSq = (float)(dx * dx + dy * dy);

            if (distSq <= radiusSq) {
                float t = (radius > 0) ? sqrtf(distSq) / (float)radius : 0.0f;
                t = (t > 1.0f) ? 1.0f : t;
                int idx = (int)(centerIndex + (edgeIndex - centerIndex) * t);
                idx = (idx < 0) ? 0 : (idx > 255) ? 255 : idx;
                wresBuffer->setPixel(px, py, (uint8_t)idx);
            }
        }
    }
}

ST_API void st_wres_gradient_corners(int x, int y, int width, int height, 
                                      uint8_t tlIndex, uint8_t trIndex, 
                                      uint8_t blIndex, uint8_t brIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    int bufferID = display->getActiveWResBuffer();
    auto wresBuffer = display->getWResBuffer(bufferID);

    if (!wresBuffer) {
        ST_SET_ERROR("WResBuffer not available");
        return;
    }

    if (width <= 0 || height <= 0) return;

    // Clip to buffer bounds
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > 432) width = 432 - x;
    if (y + height > 240) height = 240 - y;
    if (width <= 0 || height <= 0) return;

    // Draw four-corner gradient (bilinear interpolation)
    for (int dy = 0; dy < height; dy++) {
        float ty = (height > 1) ? (float)dy / (float)(height - 1) : 0.0f;
        for (int dx = 0; dx < width; dx++) {
            float tx = (width > 1) ? (float)dx / (float)(width - 1) : 0.0f;

            // Bilinear interpolation
            float top = tlIndex + (trIndex - tlIndex) * tx;
            float bottom = blIndex + (brIndex - blIndex) * tx;
            float value = top + (bottom - top) * ty;

            int idx = (int)value;
            idx = (idx < 0) ? 0 : (idx > 255) ? 255 : idx;
            wresBuffer->setPixel(x + dx, y + dy, (uint8_t)idx);
        }
    }
}

// --- PRES Palette Animation ---

ST_API void st_pres_palette_rotate_row(int row, int startIndex, int endIndex, int direction) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.presPalette(), "PResPaletteManager");

    if (row < 0 || row >= 720) {
        ST_SET_ERROR("Row must be 0-719");
        return;
    }
    if (startIndex < 0 || startIndex >= 16 || endIndex < 0 || endIndex >= 16) {
        ST_SET_ERROR("Indices must be 0-15");
        return;
    }
    if (startIndex > endIndex) {
        ST_SET_ERROR("startIndex must be <= endIndex");
        return;
    }

    auto palette = ST_CONTEXT.presPalette();
    int count = endIndex - startIndex + 1;
    if (count <= 1) return;

    std::vector<uint32_t> colors(count);
    for (int i = 0; i < count; i++) {
        colors[i] = palette->getPerRowColor(row, startIndex + i);
    }

    if (direction > 0) {
        uint32_t temp = colors[count - 1];
        for (int i = count - 1; i > 0; i--) {
            colors[i] = colors[i - 1];
        }
        colors[0] = temp;
    } else if (direction < 0) {
        uint32_t temp = colors[0];
        for (int i = 0; i < count - 1; i++) {
            colors[i] = colors[i + 1];
        }
        colors[count - 1] = temp;
    }

    for (int i = 0; i < count; i++) {
        palette->setPerRowColor(row, startIndex + i, colors[i]);
    }
}

ST_API void st_pres_palette_rotate_global(int startIndex, int endIndex, int direction) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.presPalette(), "PResPaletteManager");

    if (startIndex < 16 || startIndex >= 256 || endIndex < 16 || endIndex >= 256) {
        ST_SET_ERROR("Indices must be 16-255");
        return;
    }
    if (startIndex > endIndex) {
        ST_SET_ERROR("startIndex must be <= endIndex");
        return;
    }

    auto palette = ST_CONTEXT.presPalette();
    int count = endIndex - startIndex + 1;
    if (count <= 1) return;

    std::vector<uint32_t> colors(count);
    for (int i = 0; i < count; i++) {
        colors[i] = palette->getGlobalColor(startIndex + i);
    }

    if (direction > 0) {
        uint32_t temp = colors[count - 1];
        for (int i = count - 1; i > 0; i--) {
            colors[i] = colors[i - 1];
        }
        colors[0] = temp;
    } else if (direction < 0) {
        uint32_t temp = colors[0];
        for (int i = 0; i < count - 1; i++) {
            colors[i] = colors[i + 1];
        }
        colors[count - 1] = temp;
    }

    for (int i = 0; i < count; i++) {
        palette->setGlobalColor(startIndex + i, colors[i]);
    }
}

ST_API void st_pres_palette_copy_row(int srcRow, int dstRow) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.presPalette(), "PResPaletteManager");

    if (srcRow < 0 || srcRow >= 720 || dstRow < 0 || dstRow >= 720) {
        ST_SET_ERROR("Row must be 0-719");
        return;
    }

    auto palette = ST_CONTEXT.presPalette();
    for (int i = 0; i < 16; i++) {
        uint32_t color = palette->getPerRowColor(srcRow, i);
        palette->setPerRowColor(dstRow, i, color);
    }
}

ST_API void st_pres_palette_lerp_row(int row, int index, int r1, int g1, int b1, int r2, int g2, int b2, float t) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.presPalette(), "PResPaletteManager");

    if (row < 0 || row >= 720) {
        ST_SET_ERROR("Row must be 0-719");
        return;
    }
    if (index < 0 || index >= 16) {
        ST_SET_ERROR("Index must be 0-15");
        return;
    }

    t = (t < 0.0f) ? 0.0f : (t > 1.0f) ? 1.0f : t;

    int r = (int)(r1 + (r2 - r1) * t);
    int g = (int)(g1 + (g2 - g1) * t);
    int b = (int)(b1 + (b2 - b1) * t);

    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;

    uint32_t rgba = 0xFF000000 | (r << 16) | (g << 8) | b;
    ST_CONTEXT.presPalette()->setPerRowColor(row, index, rgba);
}

ST_API void st_pres_palette_lerp_global(int index, int r1, int g1, int b1, int r2, int g2, int b2, float t) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.presPalette(), "PResPaletteManager");

    if (index < 16 || index >= 256) {
        ST_SET_ERROR("Index must be 16-255");
        return;
    }

    t = (t < 0.0f) ? 0.0f : (t > 1.0f) ? 1.0f : t;

    int r = (int)(r1 + (r2 - r1) * t);
    int g = (int)(g1 + (g2 - g1) * t);
    int b = (int)(b1 + (b2 - b1) * t);

    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;

    uint32_t rgba = 0xFF000000 | (r << 16) | (g << 8) | b;
    ST_CONTEXT.presPalette()->setGlobalColor(index, rgba);
}

// --- PRES Gradient Drawing Functions ---

ST_API void st_pres_palette_make_ramp(int row, int startIndex, int endIndex, 
                                       int r1, int g1, int b1, int r2, int g2, int b2) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.presPalette(), "PResPaletteManager");

    if (startIndex < 0 || startIndex > 255 || endIndex < 0 || endIndex > 255) {
        ST_SET_ERROR("Indices must be 0-255");
        return;
    }
    if (startIndex > endIndex) {
        ST_SET_ERROR("startIndex must be <= endIndex");
        return;
    }

    auto palette = ST_CONTEXT.presPalette();
    int count = endIndex - startIndex + 1;

    // Generate gradient ramp
    for (int i = 0; i < count; i++) {
        float t = (count > 1) ? (float)i / (float)(count - 1) : 0.0f;
        int r = (int)(r1 + (r2 - r1) * t);
        int g = (int)(g1 + (g2 - g1) * t);
        int b = (int)(b1 + (b2 - b1) * t);

        // Clamp
        r = (r < 0) ? 0 : (r > 255) ? 255 : r;
        g = (g < 0) ? 0 : (g > 255) ? 255 : g;
        b = (b < 0) ? 0 : (b > 255) ? 255 : b;

        uint32_t rgba = 0xFF000000 | (r << 16) | (g << 8) | b;
        int index = startIndex + i;

        if (row == -1) {
            // Global palette
            if (index >= 16 && index < 256) {
                palette->setGlobalColor(index, rgba);
            }
        } else {
            // Per-row palette
            if (row >= 0 && row < 720 && index >= 0 && index < 16) {
                palette->setPerRowColor(row, index, rgba);
            }
        }
    }
}

ST_API void st_pres_gradient_h(int bufferID, int x, int y, int width, int height, 
                                uint8_t startIndex, uint8_t endIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto presBuffer = display->getPResBuffer(bufferID);

    if (!presBuffer) {
        ST_SET_ERROR("PResBuffer not available");
        return;
    }

    if (width <= 0 || height <= 0) return;

    // Clip to buffer bounds
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > 320) width = 320 - x;
    if (y + height > 240) height = 240 - y;
    if (width <= 0 || height <= 0) return;

    // Draw horizontal gradient
    for (int dy = 0; dy < height; dy++) {
        for (int dx = 0; dx < width; dx++) {
            float t = (width > 1) ? (float)dx / (float)(width - 1) : 0.0f;
            int idx = (int)(startIndex + (endIndex - startIndex) * t);
            idx = (idx < 0) ? 0 : (idx > 255) ? 255 : idx;
            presBuffer->setPixel(x + dx, y + dy, (uint8_t)idx);
        }
    }
}

ST_API void st_pres_gradient_v(int bufferID, int x, int y, int width, int height, 
                                uint8_t startIndex, uint8_t endIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto presBuffer = display->getPResBuffer(bufferID);

    if (!presBuffer) {
        ST_SET_ERROR("PResBuffer not available");
        return;
    }

    if (width <= 0 || height <= 0) return;

    // Clip to buffer bounds
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > 320) width = 320 - x;
    if (y + height > 240) height = 240 - y;
    if (width <= 0 || height <= 0) return;

    // Draw vertical gradient
    for (int dy = 0; dy < height; dy++) {
        float t = (height > 1) ? (float)dy / (float)(height - 1) : 0.0f;
        int idx = (int)(startIndex + (endIndex - startIndex) * t);
        idx = (idx < 0) ? 0 : (idx > 255) ? 255 : idx;
        presBuffer->hline(x, y + dy, width, (uint8_t)idx);
    }
}

ST_API void st_pres_gradient_radial(int bufferID, int cx, int cy, int radius, 
                                     uint8_t centerIndex, uint8_t edgeIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto presBuffer = display->getPResBuffer(bufferID);

    if (!presBuffer) {
        ST_SET_ERROR("PResBuffer not available");
        return;
    }

    if (radius <= 0) return;

    float radiusSq = (float)(radius * radius);

    // Bounding box
    int minX = cx - radius;
    int maxX = cx + radius;
    int minY = cy - radius;
    int maxY = cy + radius;

    // Clip to buffer bounds
    if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;
    if (maxX >= 320) maxX = 319;
    if (maxY >= 240) maxY = 239;

    // Draw radial gradient
    for (int py = minY; py <= maxY; py++) {
        for (int px = minX; px <= maxX; px++) {
            int dx = px - cx;
            int dy = py - cy;
            float distSq = (float)(dx * dx + dy * dy);

            if (distSq <= radiusSq) {
                float t = (radius > 0) ? sqrtf(distSq) / (float)radius : 0.0f;
                t = (t > 1.0f) ? 1.0f : t;
                int idx = (int)(centerIndex + (edgeIndex - centerIndex) * t);
                idx = (idx < 0) ? 0 : (idx > 255) ? 255 : idx;
                presBuffer->setPixel(px, py, (uint8_t)idx);
            }
        }
    }
}

ST_API void st_pres_gradient_corners(int bufferID, int x, int y, int width, int height, 
                                      uint8_t tlIndex, uint8_t trIndex, 
                                      uint8_t blIndex, uint8_t brIndex) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.display(), "DisplayManager");

    auto display = ST_CONTEXT.display();
    auto presBuffer = display->getPResBuffer(bufferID);

    if (!presBuffer) {
        ST_SET_ERROR("PResBuffer not available");
        return;
    }

    if (width <= 0 || height <= 0) return;

    // Clip to buffer bounds
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > 320) width = 320 - x;
    if (y + height > 240) height = 240 - y;
    if (width <= 0 || height <= 0) return;

    // Draw four-corner gradient (bilinear interpolation)
    for (int dy = 0; dy < height; dy++) {
        float ty = (height > 1) ? (float)dy / (float)(height - 1) : 0.0f;
        for (int dx = 0; dx < width; dx++) {
            float tx = (width > 1) ? (float)dx / (float)(width - 1) : 0.0f;

            // Bilinear interpolation
            float top = tlIndex + (trIndex - tlIndex) * tx;
            float bottom = blIndex + (brIndex - blIndex) * tx;
            float value = top + (bottom - top) * ty;

            int idx = (int)value;
            idx = (idx < 0) ? 0 : (idx > 255) ? 255 : idx;
            presBuffer->setPixel(x + dx, y + dy, (uint8_t)idx);
        }
    }
}
