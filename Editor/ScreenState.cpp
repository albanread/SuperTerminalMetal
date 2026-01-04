//
//  ScreenState.cpp
//  SuperTerminal Framework - Screen State Manager Implementation
//
//  Save and restore display state when switching between editor and runtime modes
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "ScreenState.h"
#include "Display/TextGrid.h"
#include "Display/GraphicsLayer.h"
#include "Display/SpriteManager.h"
#include <chrono>

namespace SuperTerminal {

// =============================================================================
// ScreenStateManager Implementation
// =============================================================================

ScreenStateManager::ScreenStateManager(std::shared_ptr<TextGrid> textGrid,
                                     std::shared_ptr<GraphicsLayer> graphicsLayer,
                                     std::shared_ptr<SpriteManager> spriteManager)
    : m_textGrid(textGrid)
    , m_graphicsLayer(graphicsLayer)
    , m_spriteManager(spriteManager)
    , m_editorBgColor(0x1E1E1EFF)  // Dark gray
    , m_saveGraphicsInEditor(false)  // Don't save graphics in editor mode (too large)
    , m_saveSpritesInEditor(false)   // Don't save sprite data (textures too large)
{
}

ScreenStateManager::~ScreenStateManager() {
    // Nothing to clean up
}

// =============================================================================
// State Capture
// =============================================================================

ScreenState ScreenStateManager::capture() {
    ScreenState state;
    
    state.textGrid = captureTextGrid();
    state.graphics = captureGraphics();
    state.sprites = captureSprites();
    
    state.valid = true;
    state.timestamp = getCurrentTimestamp();
    
    return state;
}

ScreenState::TextGridState ScreenStateManager::captureTextGrid() {
    ScreenState::TextGridState state;
    
    if (!m_textGrid) {
        return state;
    }
    
    state.width = m_textGrid->getWidth();
    state.height = m_textGrid->getHeight();
    
    size_t totalCells = state.size();
    state.characters.resize(totalCells);
    state.foregroundColors.resize(totalCells);
    state.backgroundColors.resize(totalCells);
    
    // Copy all cells
    for (int y = 0; y < state.height; ++y) {
        for (int x = 0; x < state.width; ++x) {
            size_t index = y * state.width + x;
            auto cell = m_textGrid->getCell(x, y);
            
            state.characters[index] = cell.character;
            state.foregroundColors[index] = cell.foreground;
            state.backgroundColors[index] = cell.background;
        }
    }
    
    return state;
}

ScreenState::GraphicsState ScreenStateManager::captureGraphics() {
    ScreenState::GraphicsState state;
    
    if (!m_graphicsLayer) {
        return state;
    }
    
    // Get graphics layer dimensions and pixels
    // Note: GraphicsLayer doesn't expose dimensions or pixel data yet
    // For editor mode, we typically don't save this anyway (too large)
    
    state.width = 800;  // Assume default window size
    state.height = 600;
    state.visible = true;  // Assume visible
    
    // Note: We don't capture pixel data - would need GraphicsLayer API enhancement
    // Graphics layer uses immediate-mode commands, not a pixel buffer
    
    return state;
}

ScreenState::SpriteState ScreenStateManager::captureSprites() {
    ScreenState::SpriteState state;
    
    if (!m_spriteManager) {
        return state;
    }
    
    state.spritesVisible = true;  // Assume sprites are visible
    
    // Note: We don't capture sprite texture data (too large)
    // Just capture positions and visibility
    // This would need SpriteManager API to enumerate sprites
    
    // For now, just track global visibility
    
    return state;
}

// =============================================================================
// State Restoration
// =============================================================================

void ScreenStateManager::restore(const ScreenState& state) {
    if (!state.valid) {
        return;
    }
    
    restoreTextGrid(state.textGrid);
    restoreGraphics(state.graphics);
    restoreSprites(state.sprites);
}

void ScreenStateManager::restoreTextGrid(const ScreenState::TextGridState& state) {
    if (!m_textGrid || state.width == 0 || state.height == 0) {
        return;
    }
    
    // Resize if needed
    if (m_textGrid->getWidth() != state.width || m_textGrid->getHeight() != state.height) {
        m_textGrid->resize(state.width, state.height);
    }
    
    // Restore all cells
    for (int y = 0; y < state.height; ++y) {
        for (int x = 0; x < state.width; ++x) {
            size_t index = y * state.width + x;
            
            if (index < state.characters.size()) {
                m_textGrid->putChar(x, y,
                                   state.characters[index],
                                   state.foregroundColors[index],
                                   state.backgroundColors[index]);
            }
        }
    }
}

void ScreenStateManager::restoreGraphics(const ScreenState::GraphicsState& state) {
    if (!m_graphicsLayer || state.width == 0 || state.height == 0) {
        return;
    }
    
    // Restore graphics layer
    // This would need GraphicsLayer API to set pixel data
    // For now, skip (graphics restore is optional)
}

void ScreenStateManager::restoreSprites(const ScreenState::SpriteState& state) {
    if (!m_spriteManager) {
        return;
    }
    
    // Restore sprite visibility
    if (state.spritesVisible) {
        showAllSprites();
    } else {
        hideAllSprites();
    }
    
    // Note: We don't restore individual sprite positions/data
    // That would require SpriteManager API enhancements
}

// =============================================================================
// Quick State Slots
// =============================================================================

void ScreenStateManager::saveEditorState() {
    m_editorState = capture();
}

void ScreenStateManager::saveRuntimeState() {
    m_runtimeState = capture();
}

void ScreenStateManager::restoreEditorState() {
    if (m_editorState.valid) {
        restore(m_editorState);
    }
}

void ScreenStateManager::restoreRuntimeState() {
    if (m_runtimeState.valid) {
        restore(m_runtimeState);
    }
}

void ScreenStateManager::clearSavedStates() {
    m_editorState = ScreenState();
    m_runtimeState = ScreenState();
}

// =============================================================================
// Editor Mode Helpers
// =============================================================================

void ScreenStateManager::switchToEditorMode() {
    if (!validateComponents()) {
        return;
    }
    
    // Save current runtime state
    saveRuntimeState();
    
    // DISABLED: Don't restore editor state - always use current document
    // Editor content is managed by SourceDocument/TextBuffer, not screen state
    // if (m_editorState.valid) {
    //     restoreEditorState();
    // } else {
        prepareEditorScreen(m_editorBgColor);
    // }
    
    // Hide sprites in editor mode
    hideAllSprites();
}

void ScreenStateManager::switchToRuntimeMode() {
    if (!validateComponents()) {
        return;
    }
    
    // Save current editor state
    saveEditorState();
    
    // Restore runtime state (or prepare blank runtime screen)
    if (m_runtimeState.valid) {
        restoreRuntimeState();
    } else {
        prepareRuntimeScreen();
    }
    
    // Show sprites in runtime mode
    showAllSprites();
}

void ScreenStateManager::prepareEditorScreen(uint32_t backgroundColor) {
    if (!m_textGrid) {
        return;
    }
    
    // Clear text grid with solid background
    int width = m_textGrid->getWidth();
    int height = m_textGrid->getHeight();
    
    m_textGrid->fillRegion(0, 0, width, height,
                          U' ',  // Space character
                          0xE0E0E0FF,  // Light gray text
                          backgroundColor);  // Solid background
    
    // Hide sprites
    hideAllSprites();
    
    // Optionally clear graphics layer
    if (m_graphicsLayer) {
        m_graphicsLayer->clear();
    }
}

void ScreenStateManager::prepareRuntimeScreen() {
    if (!m_textGrid) {
        return;
    }
    
    // Clear text grid with transparent background
    int width = m_textGrid->getWidth();
    int height = m_textGrid->getHeight();
    
    m_textGrid->fillRegion(0, 0, width, height,
                          U' ',  // Space character
                          0xFFFFFFFF,  // White text
                          0x00000000);  // Transparent background
    
    // Show sprites
    showAllSprites();
    
    // Don't clear graphics layer - let RECTF and other graphics commands persist
    // Graphics will be cleared explicitly by CLS, CLG, or gfx_clear() commands
    // if (m_graphicsLayer) {
    //     m_graphicsLayer->clear();
    // }
}

void ScreenStateManager::invalidateEditorState() {
    m_editorState.valid = false;
    // Note: Editor state invalidated
}

void ScreenStateManager::invalidateRuntimeState() {
    m_runtimeState.valid = false;
    // Note: Runtime state invalidated
}

// =============================================================================
// Utility
// =============================================================================

size_t ScreenStateManager::getMemoryUsage() const {
    size_t usage = 0;
    
    // Text grid state
    usage += m_editorState.textGrid.characters.size() * sizeof(char32_t);
    usage += m_editorState.textGrid.foregroundColors.size() * sizeof(uint32_t);
    usage += m_editorState.textGrid.backgroundColors.size() * sizeof(uint32_t);
    
    usage += m_runtimeState.textGrid.characters.size() * sizeof(char32_t);
    usage += m_runtimeState.textGrid.foregroundColors.size() * sizeof(uint32_t);
    usage += m_runtimeState.textGrid.backgroundColors.size() * sizeof(uint32_t);
    
    // Graphics state (pixels are large!)
    usage += m_editorState.graphics.pixels.size() * sizeof(uint32_t);
    usage += m_runtimeState.graphics.pixels.size() * sizeof(uint32_t);
    
    return usage;
}

uint64_t ScreenStateManager::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

// =============================================================================
// Internal Helpers
// =============================================================================

bool ScreenStateManager::validateComponents() const {
    return m_textGrid != nullptr;
    // Graphics and sprites are optional
}

void ScreenStateManager::hideAllSprites() {
    if (!m_spriteManager) {
        return;
    }
    
    // This would need SpriteManager API to hide all sprites
    // For now, this is a placeholder
    // SpriteManager would need: setAllVisible(false)
}

void ScreenStateManager::showAllSprites() {
    if (!m_spriteManager) {
        return;
    }
    
    // This would need SpriteManager API to show all sprites
    // For now, this is a placeholder
    // SpriteManager would need: setAllVisible(true)
}

void ScreenStateManager::setSolidBackground(uint32_t color) {
    if (!m_textGrid) {
        return;
    }
    
    int width = m_textGrid->getWidth();
    int height = m_textGrid->getHeight();
    
    // Set all cells to solid background color
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            auto cell = m_textGrid->getCell(x, y);
            m_textGrid->putChar(x, y, cell.character, cell.foreground, color);
        }
    }
}

void ScreenStateManager::clearBackground() {
    if (!m_textGrid) {
        return;
    }
    
    int width = m_textGrid->getWidth();
    int height = m_textGrid->getHeight();
    
    // Set all cells to transparent background
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            auto cell = m_textGrid->getCell(x, y);
            m_textGrid->putChar(x, y, cell.character, cell.foreground, 0x00000000);
        }
    }
}

} // namespace SuperTerminal