//
//  SimpleLineEditor.h
//  SuperTerminal Framework - Simple Line Editor
//
//  Reusable line editor for INPUT_AT and similar input operations
//  Based on FBRunner3's interactive mode editing logic
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef SIMPLE_LINE_EDITOR_H
#define SIMPLE_LINE_EDITOR_H

#include <string>
#include <memory>
#include <chrono>

namespace SuperTerminal {

// Forward declarations
class InputManager;
class TextGrid;

// =============================================================================
// SimpleLineEditor - Basic line editing functionality
// =============================================================================
//
// Provides:
// - Character input with cursor positioning
// - Backspace/delete support
// - Left/right arrow key navigation
// - Word-boundary navigation (Ctrl+Left/Right)
// - Visual cursor display
// - Coordinate-based positioning for INPUT_AT
//
// Does NOT include:
// - Command history (simplified for single input)
// - Multi-line editing
// - Auto-completion
// - Syntax highlighting
//

class SimpleLineEditor {
public:
    // Constructor - sets up editor at specified grid coordinates
    SimpleLineEditor(int x, int y, const std::string& prompt = "");
    
    // Destructor
    ~SimpleLineEditor();
    
    // Update editor state with input - returns true when input is complete (Enter pressed)
    bool update(InputManager* inputManager);
    
    // Render current state to text grid
    void render(TextGrid* textGrid);
    
    // Get the final input result (call after update() returns true)
    std::string getResult() const;
    
    // Get current input text (for live display)
    std::string getCurrentText() const;
    
    // Reset editor state for reuse
    void reset();
    
    // Configuration
    void setPrompt(const std::string& prompt);
    void setPosition(int x, int y);
    void setMaxLength(int maxLength);  // 0 = unlimited
    void setPasswordMode(bool enabled); // Hide characters with *
    
    // State queries
    bool isComplete() const { return _isComplete; }
    int getCursorPosition() const { return _cursorPos; }
    
private:
    // Core state
    std::string _currentInput;
    int _cursorPos;
    int _displayX, _displayY;
    std::string _prompt;
    bool _isComplete;
    
    // Configuration options
    int _maxLength;
    bool _passwordMode;
    
    // Display state
    bool _showCursor;
    int _blinkCounter;  // For cursor blinking
    
    // Timing for input processing (real time, not frame dependent)
    std::chrono::steady_clock::time_point _lastInputTime;
    static constexpr std::chrono::milliseconds INPUT_DELAY{50}; // 50ms between key repeats
    
    // Internal methods
    void insertCharacter(char ch);
    void deleteCharacterBefore();
    void deleteCharacterAt();
    void moveCursorLeft();
    void moveCursorRight();
    void moveCursorWordLeft();
    void moveCursorWordRight();
    void moveCursorHome();
    void moveCursorEnd();
    
    // Helper methods
    bool canProcessInput() const;
    void updateInputTiming();
    bool isWordBoundary(char ch) const;
    std::string getDisplayText() const;  // Handles password mode
    int getDisplayWidth() const;
    
    // Rendering helpers
    void renderPrompt(TextGrid* textGrid);
    void renderText(TextGrid* textGrid);
    void renderCursor(TextGrid* textGrid);
};

} // namespace SuperTerminal

#endif // SIMPLE_LINE_EDITOR_H