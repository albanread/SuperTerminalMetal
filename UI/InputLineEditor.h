//
// InputLineEditor.h
// SuperTerminal Framework - Interactive Line Input Editor
//
// Provides single-line text input with editing capabilities in a TextGrid.
// Handles keyboard input, cursor movement, history navigation, and line completion.
// Designed for interactive BASIC shell mode.
//

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>

namespace SuperTerminal {

// Forward declarations
class TextGrid;
class InputManager;

/// Single-line text editor for TextGrid input
/// Supports editing, history, cursor movement, and visual feedback
class InputLineEditor {
public:
    /// Constructor
    /// @param textGrid Shared pointer to TextGrid for rendering
    /// @param inputManager Shared pointer to InputManager for keyboard input
    /// @param row Row in TextGrid where input line appears (0-based)
    InputLineEditor(std::shared_ptr<TextGrid> textGrid,
                   std::shared_ptr<InputManager> inputManager,
                   int row);
    
    /// Destructor
    ~InputLineEditor();
    
    /// Update editor state and process keyboard input
    /// Called once per frame
    /// @return true if line is complete (Enter pressed), false otherwise
    bool update();
    
    /// Render the current input line with prompt
    /// @param prompt Prompt string to display before input (e.g., "Ready. ")
    void render(const std::string& prompt);
    
    /// Get the completed line
    /// @return The input line (without prompt)
    std::string getLine() const;
    
    /// Clear the current input buffer
    void clear();
    
    /// Set initial content for the input line (for editing)
    /// @param content Initial text content
    void setContent(const std::string& content);
    
    /// Add a line to command history
    /// @param line Line to add to history
    void addToHistory(const std::string& line);
    
    /// Move to previous history entry
    void historyPrev();
    
    /// Move to next history entry
    void historyNext();
    
    /// Get current cursor position in buffer
    /// @return Cursor position (0 = start of line)
    size_t getCursorPosition() const { return cursorPos_; }
    
    /// Get current buffer content
    /// @return Current input buffer
    std::string getBuffer() const { return buffer_; }
    
    /// Set colors for input display
    /// @param foreground Foreground color (RGBA)
    /// @param background Background color (RGBA)
    void setColors(uint32_t foreground, uint32_t background);
    
    /// Set maximum line length
    /// @param maxLength Maximum number of characters allowed
    void setMaxLength(size_t maxLength) { maxLength_ = maxLength; }
    
    /// Check if line is complete (Enter was pressed)
    /// @return true if line is ready to be retrieved
    bool isComplete() const { return lineComplete_; }
    
    /// Reset completion state (after retrieving line)
    void resetComplete() { lineComplete_ = false; }

private:
    // Components
    std::shared_ptr<TextGrid> textGrid_;
    std::shared_ptr<InputManager> inputManager_;
    
    // Display state
    int row_;                           // Row in TextGrid for this editor
    uint32_t foregroundColor_;          // Text color
    uint32_t backgroundColor_;          // Background color
    
    // Input state
    std::string buffer_;                // Current input buffer
    size_t cursorPos_;                  // Cursor position in buffer
    bool lineComplete_;                 // True when Enter pressed
    size_t maxLength_;                  // Maximum line length
    
    // History state
    std::vector<std::string> history_;  // Command history
    int historyIndex_;                  // Current position in history (-1 = not browsing)
    std::string savedBuffer_;           // Buffer saved when browsing history
    
    // Key state tracking (for repeat prevention)
    bool backspacePressed_;
    bool deletePressed_;
    bool leftPressed_;
    bool rightPressed_;
    bool upPressed_;
    bool downPressed_;
    bool enterPressed_;
    
    // Frame counter for cursor blink
    int frameCounter_;
    bool cursorVisible_;
    
    // Private methods
    void handleCharacterInput();
    void handleBackspace();
    void handleDelete();
    void handleCursorLeft();
    void handleCursorRight();
    void handleHome();
    void handleEnd();
    void handleEnter();
    void updateKeyStates();
    void insertChar(char32_t ch);
    void deleteCharAt(size_t pos);
    std::string getDisplayString(const std::string& prompt) const;
};

} // namespace SuperTerminal