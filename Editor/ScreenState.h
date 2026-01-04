//
//  ScreenState.h
//  SuperTerminal Framework - Screen State Manager
//
//  Save and restore display state when switching between editor and runtime modes
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef SCREEN_STATE_H
#define SCREEN_STATE_H

#include <memory>
#include <vector>
#include <cstdint>

namespace SuperTerminal {

// Forward declarations
class TextGrid;
class GraphicsLayer;
class SpriteManager;

// =============================================================================
// ScreenState - Snapshot of display state
// =============================================================================

struct ScreenState {
    // Text grid snapshot
    struct TextGridState {
        int width;
        int height;
        std::vector<char32_t> characters;
        std::vector<uint32_t> foregroundColors;
        std::vector<uint32_t> backgroundColors;
        
        TextGridState() : width(0), height(0) {}
        
        size_t size() const {
            return static_cast<size_t>(width) * static_cast<size_t>(height);
        }
    } textGrid;
    
    // Graphics layer snapshot
    struct GraphicsState {
        int width;
        int height;
        std::vector<uint32_t> pixels;  // RGBA
        bool visible;
        
        GraphicsState() : width(0), height(0), visible(false) {}
        
        size_t size() const {
            return static_cast<size_t>(width) * static_cast<size_t>(height);
        }
    } graphics;
    
    // Sprite state snapshot
    struct SpriteState {
        struct Sprite {
            uint16_t id;
            int16_t x;
            int16_t y;
            uint16_t width;
            uint16_t height;
            bool visible;
            uint8_t layer;
            // Texture data not saved (too large), just hide sprites in editor mode
        };
        
        std::vector<Sprite> sprites;
        bool spritesVisible;  // Global sprite visibility toggle
        
        SpriteState() : spritesVisible(true) {}
    } sprites;
    
    // Metadata
    bool valid;           // Is this state valid?
    uint64_t timestamp;   // When was this captured?
    
    ScreenState() : valid(false), timestamp(0) {}
};

// =============================================================================
// ScreenStateManager - Manage screen state transitions
// =============================================================================

class ScreenStateManager {
public:
    // Constructor
    ScreenStateManager(std::shared_ptr<TextGrid> textGrid,
                      std::shared_ptr<GraphicsLayer> graphicsLayer,
                      std::shared_ptr<SpriteManager> spriteManager);
    
    // Destructor
    ~ScreenStateManager();
    
    // =========================================================================
    // State Capture
    // =========================================================================
    
    /// Capture current screen state
    /// @return ScreenState snapshot
    ScreenState capture();
    
    /// Capture text grid only (fast)
    ScreenState::TextGridState captureTextGrid();
    
    /// Capture graphics layer only
    ScreenState::GraphicsState captureGraphics();
    
    /// Capture sprite state only
    ScreenState::SpriteState captureSprites();
    
    // =========================================================================
    // State Restoration
    // =========================================================================
    
    /// Restore screen state
    /// @param state State to restore
    void restore(const ScreenState& state);
    
    /// Restore text grid only
    void restoreTextGrid(const ScreenState::TextGridState& state);
    
    /// Restore graphics layer only
    void restoreGraphics(const ScreenState::GraphicsState& state);
    
    /// Restore sprite state only
    void restoreSprites(const ScreenState::SpriteState& state);
    
    // =========================================================================
    // Quick State Slots (for mode switching)
    // =========================================================================
    
    /// Save current state to editor slot
    void saveEditorState();
    
    /// Save current state to runtime slot
    void saveRuntimeState();
    
    /// Restore editor state
    void restoreEditorState();
    
    /// Restore runtime state
    void restoreRuntimeState();
    
    /// Check if editor state exists
    bool hasEditorState() const { return m_editorState.valid; }
    
    /// Check if runtime state exists
    bool hasRuntimeState() const { return m_runtimeState.valid; }
    
    /// Clear saved states
    void clearSavedStates();
    
    // =========================================================================
    // Editor Mode Helpers
    // =========================================================================
    
    /// Switch to editor mode
    /// - Saves current runtime state
    /// - Restores editor state (or sets up blank editor screen)
    /// - Sets solid background
    /// - Hides all sprites
    void switchToEditorMode();
    
    /// Switch to runtime mode
    /// - Saves current editor state
    /// - Restores runtime state (or clears for fresh run)
    /// - Restores transparent background
    /// - Shows sprites
    void switchToRuntimeMode();
    
    /// Prepare blank editor screen
    /// - Clear text grid
    /// - Set solid background color
    /// - Hide all sprites
    /// - Clear graphics layer (optional)
    void prepareEditorScreen(uint32_t backgroundColor = 0x1E1E1EFF);  // Dark gray
    
    /// Prepare blank runtime screen
    /// - Clear text grid
    /// - Set transparent background
    /// - Clear graphics layer
    /// - Clear sprites
    void prepareRuntimeScreen();
    
    /// Invalidate saved editor state
    /// - Marks editor state as invalid so it won't be restored
    /// - Useful when editor content has been modified externally
    void invalidateEditorState();
    
    /// Invalidate saved runtime state
    /// - Marks runtime state as invalid so it won't be restored
    void invalidateRuntimeState();
    
    // =========================================================================
    // Configuration
    // =========================================================================
    
    /// Set editor background color
    void setEditorBackgroundColor(uint32_t color) { m_editorBgColor = color; }
    
    /// Get editor background color
    uint32_t getEditorBackgroundColor() const { return m_editorBgColor; }
    
    /// Set whether to save/restore graphics in editor mode
    void setSaveGraphicsInEditor(bool save) { m_saveGraphicsInEditor = save; }
    
    /// Set whether to save/restore sprites in editor mode
    void setSaveSpritesInEditor(bool save) { m_saveSpritesInEditor = save; }
    
    // =========================================================================
    // Utility
    // =========================================================================
    
    /// Get memory usage of saved states (bytes)
    size_t getMemoryUsage() const;
    
    /// Get current timestamp
    static uint64_t getCurrentTimestamp();

private:
    // =========================================================================
    // Member Variables
    // =========================================================================
    
    std::shared_ptr<TextGrid> m_textGrid;
    std::shared_ptr<GraphicsLayer> m_graphicsLayer;
    std::shared_ptr<SpriteManager> m_spriteManager;
    
    // Saved states
    ScreenState m_editorState;      // Editor screen state
    ScreenState m_runtimeState;     // Runtime screen state
    
    // Configuration
    uint32_t m_editorBgColor;       // Background color for editor mode
    bool m_saveGraphicsInEditor;    // Save graphics layer in editor state?
    bool m_saveSpritesInEditor;     // Save sprite state in editor mode?
    
    // =========================================================================
    // Internal Helpers
    // =========================================================================
    
    /// Validate components are available
    bool validateComponents() const;
    
    /// Hide all sprites
    void hideAllSprites();
    
    /// Show all sprites
    void showAllSprites();
    
    /// Set all background cells to solid color
    void setSolidBackground(uint32_t color);
    
    /// Clear all background cells (transparent)
    void clearBackground();
};

} // namespace SuperTerminal

#endif // SCREEN_STATE_H