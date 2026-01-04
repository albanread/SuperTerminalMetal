//
// InputManager.h
// SuperTerminal v2
//
// Input system for keyboard and mouse events.
// Handles keyboard state, character input buffer, and mouse position/buttons.
//
// THREAD SAFETY:
// - All public methods are thread-safe via internal mutex
// - beginFrame() and endFrame() should be called from the render thread
// - Event handling (handleKeyDown, handleMouseMove, etc.) can be called from event thread
// - Query methods (isKeyPressed, getMousePosition, etc.) can be called from any thread
// - Character buffer operations are thread-safe
//

#ifndef SUPERTERMINAL_INPUT_MANAGER_H
#define SUPERTERMINAL_INPUT_MANAGER_H

#include <cstdint>
#include <array>
#include <vector>
#include <mutex>
#include <memory>

namespace SuperTerminal {

/// Key codes (matches USB HID codes used in API)
enum class KeyCode {
    Unknown = 0,
    
    // Letters
    A = 4, B, C, D, E, F, G, H, I, J, K, L, M, N,
    O, P, Q, R, S, T, U, V, W, X, Y, Z,
    
    // Numbers
    Num1 = 30, Num2, Num3, Num4, Num5,
    Num6, Num7, Num8, Num9, Num0,
    
    // Special keys
    Enter = 40,
    Escape = 41,
    Backspace = 42,
    Tab = 43,
    Space = 44,
    
    // Navigation keys
    Insert = 73,
    Home = 74,
    Delete = 76,
    End = 77,
    
    // Arrow keys
    Right = 79,
    Left = 80,
    Down = 81,
    Up = 82,
    
    // Page navigation
    PageUp = 75,
    PageDown = 78,
    
    // Function keys
    F1 = 58, F2, F3, F4, F5, F6,
    F7, F8, F9, F10, F11, F12,
    
    // Modifier keys
    LeftShift = 225,
    LeftControl = 224,
    LeftAlt = 226,
    LeftCommand = 227,
    RightShift = 229,
    RightControl = 228,
    RightAlt = 230,
    RightCommand = 231,
};

/// Mouse button enumeration
enum class MouseButton {
    Left = 0,
    Right = 1,
    Middle = 2,
};

/// Maximum number of keys to track
constexpr int MAX_KEYS = 256;

/// Maximum character buffer size
constexpr int MAX_CHAR_BUFFER = 128;

/// InputManager: Manages keyboard and mouse input state
///
/// Responsibilities:
/// - Track keyboard key states (pressed/just pressed/just released)
/// - Maintain character input buffer for text entry
/// - Track mouse position (pixels and grid coordinates)
/// - Track mouse button states
/// - Thread-safe input state updates
///
/// Usage:
/// - Call beginFrame() at start of frame (render thread)
/// - Process events with handleKeyDown(), handleKeyUp(), etc. (event thread)
/// - Query state with isKeyPressed(), getMousePosition(), etc. (any thread)
/// - Call endFrame() at end of frame to update "just pressed/released" states (render thread)
///
/// Thread Safety:
/// - All methods are protected by an internal mutex
/// - Frame boundary methods (beginFrame/endFrame) should be called from render thread
/// - Event handlers can be called from OS event thread
/// - State queries are safe from any thread (script thread, render thread, etc.)
class InputManager {
public:
    /// Constructor
    InputManager();
    
    /// Destructor
    ~InputManager();
    
    // Disable copy, allow move
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;
    InputManager(InputManager&&) noexcept;
    InputManager& operator=(InputManager&&) noexcept;
    
    // =========================================================================
    // Frame Management
    // =========================================================================
    
    /// Begin a new input frame
    /// Call this at the start of each frame, before processing events
    /// @note Thread Safety: Should be called from render thread
    void beginFrame();
    
    /// End the current input frame
    /// Call this at the end of each frame, after processing events
    /// Updates "just pressed" and "just released" states
    /// @note Thread Safety: Should be called from render thread
    void endFrame();
    
    // =========================================================================
    // Event Processing (called by DisplayManager from OS events)
    // =========================================================================
    
    /// Handle key down event
    /// @param keyCode The key code that was pressed
    /// @note Thread Safety: Safe to call from OS event thread
    void handleKeyDown(KeyCode keyCode);
    
    /// Handle key up event
    /// @param keyCode The key code that was released
    /// @note Thread Safety: Safe to call from OS event thread
    void handleKeyUp(KeyCode keyCode);
    
    /// Handle character input
    /// @param character Unicode character (UTF-32)
    /// @note Thread Safety: Safe to call from OS event thread
    void handleCharacterInput(uint32_t character);
    
    /// Handle character input with modifier keys
    /// Supports Alt+number for entering ASCII codes 128-255
    /// @param character Base character
    /// @param modifiers Modifier key flags (shift, ctrl, alt, cmd)
    /// @note Thread Safety: Safe to call from OS event thread
    void handleCharacterInputWithModifiers(uint32_t character, bool shift, bool ctrl, bool alt, bool cmd);
    
    /// Handle mouse move event
    /// @param x Mouse X position in pixels
    /// @param y Mouse Y position in pixels
    /// @note Thread Safety: Safe to call from OS event thread
    void handleMouseMove(int x, int y);
    
    /// Handle mouse button down event
    /// @param button Mouse button that was pressed
    /// @note Thread Safety: Safe to call from OS event thread
    void handleMouseButtonDown(MouseButton button);
    
    /// Handle mouse button up event
    /// @param button Mouse button that was released
    /// @note Thread Safety: Safe to call from OS event thread
    void handleMouseButtonUp(MouseButton button);
    
    /// Handle mouse button down with timestamp for double-click detection
    /// @param button Mouse button that was pressed
    /// @param timestamp Event timestamp in seconds
    /// @note Thread Safety: Safe to call from OS event thread
    void handleMouseButtonDownWithTime(MouseButton button, double timestamp);
    
    /// Handle mouse wheel event
    /// @param dx Horizontal scroll delta
    /// @param dy Vertical scroll delta
    /// @note Thread Safety: Safe to call from OS event thread
    void handleMouseWheel(float dx, float dy);
    
    // =========================================================================
    // Keyboard State Query
    // =========================================================================
    
    /// Check if a key is currently pressed
    /// @param keyCode Key to check
    /// @return true if key is pressed
    /// @note Thread Safety: Safe to call from any thread
    bool isKeyPressed(KeyCode keyCode) const;
    
    /// Check if a key was just pressed this frame
    /// @param keyCode Key to check
    /// @return true if key was pressed this frame
    /// @note Thread Safety: Safe to call from any thread
    bool isKeyJustPressed(KeyCode keyCode) const;
    
    /// Check if a key was just released this frame
    /// @param keyCode Key to check
    /// @return true if key was released this frame
    /// @note Thread Safety: Safe to call from any thread
    bool isKeyJustReleased(KeyCode keyCode) const;
    
    /// Check if any modifier key is pressed
    /// @return true if Shift, Control, Alt, or Command is pressed
    /// @note Thread Safety: Safe to call from any thread
    bool isAnyModifierPressed() const;
    
    /// Check if Shift is pressed
    /// @note Thread Safety: Safe to call from any thread
    bool isShiftPressed() const;
    
    /// Check if Control is pressed
    /// @note Thread Safety: Safe to call from any thread
    bool isControlPressed() const;
    
    /// Check if Alt/Option is pressed
    /// @note Thread Safety: Safe to call from any thread
    bool isAltPressed() const;
    
    /// Check if Command is pressed
    /// @note Thread Safety: Safe to call from any thread
    bool isCommandPressed() const;
    
    // =========================================================================
    // Character Input Buffer
    // =========================================================================
    
    /// Get next character from input buffer
    /// @return Next character (UTF-32), or 0 if buffer is empty
    uint32_t getNextCharacter();
    
    /// Peek at next character without removing it
    /// @return Next character (UTF-32), or 0 if buffer is empty
    uint32_t peekNextCharacter() const;
    
    /// Check if character buffer has data
    /// @return true if buffer is not empty
    bool hasCharacters() const;
    
    /// Clear character input buffer
    void clearCharacterBuffer();
    
    /// Get number of characters in buffer
    /// @return Character count
    size_t getCharacterBufferSize() const;
    
    // =========================================================================
    // Box Drawing Character Input (Compose Key / Digraph System)
    // =========================================================================
    
    /// Enable/disable compose key mode for box drawing characters
    /// When enabled, pressing a compose key (e.g., Right Alt) followed by
    /// two keys will produce box drawing characters
    /// @param enabled true to enable compose key mode
    void setComposeKeyEnabled(bool enabled);
    
    /// Check if compose key mode is enabled
    /// @return true if compose key mode is enabled
    bool isComposeKeyEnabled() const;
    
    /// Check if currently in compose sequence (waiting for second key)
    /// @return true if in compose sequence
    bool isInComposeSequence() const;
    
    /// Get the current compose key combination as a string (for display)
    /// @return String like "Compose + h" or empty if not in sequence
    std::string getComposeSequenceDisplay() const;
    
    /// Handle compose key sequence (called internally from handleCharacterInputWithModifiers)
    /// @param key1 First key of sequence
    /// @param key2 Second key of sequence
    /// @return ASCII code (128-255) if valid sequence, or 0 if invalid
    uint8_t handleComposeSequence(char key1, char key2);
    
    /// Manually enter compose mode (for key-based triggering)
    /// User can then type 2 letters (digraph) or 3 digits (ASCII code)
    void enterComposeMode();
    
    // =========================================================================
    // Mouse State Query
    // =========================================================================
    
    /// Get mouse position in pixels
    /// @param outX Output X coordinate
    /// @param outY Output Y coordinate
    void getMousePosition(int* outX, int* outY) const;
    
    /// Get mouse position in grid coordinates
    /// @param outGridX Output grid X coordinate
    /// @param outGridY Output grid Y coordinate
    void getMouseGridPosition(int* outGridX, int* outGridY) const;
    
    /// Set cell size for mouse-to-grid coordinate conversion
    /// @param cellWidth Width of a character cell in points (view coordinate space)
    /// @param cellHeight Height of a character cell in points (view coordinate space)
    void setCellSize(float cellWidth, float cellHeight);
    
    /// Check if mouse button is pressed
    /// @param button Button to check
    /// @return true if button is pressed
    bool isMouseButtonPressed(MouseButton button) const;
    
    /// Check if mouse button was just pressed this frame
    /// @param button Button to check
    /// @return true if button was pressed this frame
    bool isMouseButtonJustPressed(MouseButton button) const;
    
    /// Check if mouse button was just released this frame
    /// @param button Button to check
    /// @return true if button was released this frame
    bool isMouseButtonJustReleased(MouseButton button) const;
    
    /// Check if a double-click occurred this frame
    /// @return true if a double-click was detected
    bool isDoubleClick() const;
    
    /// Clear double-click state (called after processing)
    void clearDoubleClick();
    
    /// Get mouse wheel delta
    /// @param outDx Output horizontal scroll delta
    /// @param outDy Output vertical scroll delta
    void getMouseWheel(float* outDx, float* outDy) const;
    
    // =========================================================================
    // Utility
    // =========================================================================
    
    /// Clear all input state
    void clearAll();
    
    /// Convert macOS key code to our KeyCode
    /// @param macKeyCode macOS virtual key code
    /// @return Our KeyCode enum
    static KeyCode convertMacKeyCode(unsigned short macKeyCode);
    
    /// Convert KeyCode to string for debugging
    /// @param keyCode Key to convert
    /// @return String representation
    static const char* keyCodeToString(KeyCode keyCode);
    
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    
    // Internal helpers for box drawing input
    void processAltNumberInput(uint32_t character);
    void processComposeKey(char key);
};

} // namespace SuperTerminal

#endif // SUPERTERMINAL_INPUT_MANAGER_H