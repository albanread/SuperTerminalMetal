//
// InputLineEditor.cpp
// SuperTerminal Framework - Interactive Line Input Editor
//
// Implementation of single-line text input with editing capabilities.
//

#include "InputLineEditor.h"
#include "../Display/TextGrid.h"
#include "../Input/InputManager.h"
#include <algorithm>

namespace SuperTerminal {

// Default colors (white on black)
static const uint32_t DEFAULT_FG_COLOR = 0xFFFFFFFF;
static const uint32_t DEFAULT_BG_COLOR = 0xFF000000;
static const uint32_t CURSOR_COLOR = 0xFF00FF00;  // Green cursor

// Cursor blink rate (frames)
static const int CURSOR_BLINK_RATE = 30;

InputLineEditor::InputLineEditor(std::shared_ptr<TextGrid> textGrid,
                                 std::shared_ptr<InputManager> inputManager,
                                 int row)
    : textGrid_(textGrid)
    , inputManager_(inputManager)
    , row_(row)
    , foregroundColor_(DEFAULT_FG_COLOR)
    , backgroundColor_(DEFAULT_BG_COLOR)
    , buffer_("")
    , cursorPos_(0)
    , lineComplete_(false)
    , maxLength_(80)
    , historyIndex_(-1)
    , savedBuffer_("")
    , backspacePressed_(false)
    , deletePressed_(false)
    , leftPressed_(false)
    , rightPressed_(false)
    , upPressed_(false)
    , downPressed_(false)
    , enterPressed_(false)
    , frameCounter_(0)
    , cursorVisible_(true)
{
}

InputLineEditor::~InputLineEditor() {
}

bool InputLineEditor::update() {
    if (!inputManager_) {
        return false;
    }
    
    // Update key states for edge detection
    updateKeyStates();
    
    // Handle special keys
    if (inputManager_->isKeyPressed(KeyCode::Backspace) && !backspacePressed_) {
        handleBackspace();
        backspacePressed_ = true;
    }
    
    // Note: Delete key not available in KeyCode enum
    
    if (inputManager_->isKeyPressed(KeyCode::Left) && !leftPressed_) {
        handleCursorLeft();
        leftPressed_ = true;
    }
    
    if (inputManager_->isKeyPressed(KeyCode::Right) && !rightPressed_) {
        handleCursorRight();
        rightPressed_ = true;
    }
    
    if (inputManager_->isKeyPressed(KeyCode::Up) && !upPressed_) {
        historyPrev();
        upPressed_ = true;
    }
    
    if (inputManager_->isKeyPressed(KeyCode::Down) && !downPressed_) {
        historyNext();
        downPressed_ = true;
    }
    
    // Note: Home and End keys not available in KeyCode enum
    
    if (inputManager_->isKeyPressed(KeyCode::Enter) && !enterPressed_) {
        handleEnter();
        enterPressed_ = true;
        return true;  // Line is complete
    }
    
    // Handle character input
    handleCharacterInput();
    
    // Update cursor blink
    frameCounter_++;
    if (frameCounter_ >= CURSOR_BLINK_RATE) {
        frameCounter_ = 0;
        cursorVisible_ = !cursorVisible_;
    }
    
    return lineComplete_;
}

void InputLineEditor::render(const std::string& prompt) {
    if (!textGrid_) {
        return;
    }
    
    // Get display string with prompt
    std::string displayString = getDisplayString(prompt);
    
    // Clear the row first
    int width = textGrid_->getWidth();
    for (int x = 0; x < width; x++) {
        textGrid_->putChar(x, row_, ' ', foregroundColor_, backgroundColor_);
    }
    
    // Render the text
    int x = 0;
    for (size_t i = 0; i < displayString.length() && x < width; i++) {
        char ch = displayString[i];
        
        // Check if this is the cursor position
        bool isCursor = (i == (prompt.length() + cursorPos_));
        
        if (isCursor && cursorVisible_) {
            // Render cursor (inverted colors)
            textGrid_->putChar(x, row_, ch, backgroundColor_, CURSOR_COLOR);
        } else {
            // Normal character
            textGrid_->putChar(x, row_, ch, foregroundColor_, backgroundColor_);
        }
        x++;
    }
    
    // If cursor is at end of line and visible, show it
    if (cursorPos_ == buffer_.length() && cursorVisible_) {
        int cursorX = static_cast<int>(prompt.length() + cursorPos_);
        if (cursorX < width) {
            textGrid_->putChar(cursorX, row_, ' ', backgroundColor_, CURSOR_COLOR);
        }
    }
}

std::string InputLineEditor::getLine() const {
    return buffer_;
}

void InputLineEditor::clear() {
    buffer_.clear();
    cursorPos_ = 0;
    lineComplete_ = false;
    historyIndex_ = -1;
    savedBuffer_.clear();
}

void InputLineEditor::setContent(const std::string& content) {
    buffer_ = content;
    cursorPos_ = buffer_.length();
    lineComplete_ = false;
}

void InputLineEditor::addToHistory(const std::string& line) {
    if (line.empty()) {
        return;
    }
    
    // Don't add duplicates of the last entry
    if (!history_.empty() && history_.back() == line) {
        return;
    }
    
    history_.push_back(line);
    
    // Limit history size to 1000 entries
    if (history_.size() > 1000) {
        history_.erase(history_.begin());
    }
    
    historyIndex_ = -1;
}

void InputLineEditor::historyPrev() {
    if (history_.empty()) {
        return;
    }
    
    // Save current buffer if starting history browsing
    if (historyIndex_ == -1) {
        savedBuffer_ = buffer_;
        historyIndex_ = static_cast<int>(history_.size());
    }
    
    // Move to previous entry
    historyIndex_--;
    if (historyIndex_ < 0) {
        historyIndex_ = 0;
    }
    
    // Load history entry
    buffer_ = history_[historyIndex_];
    cursorPos_ = buffer_.length();
}

void InputLineEditor::historyNext() {
    if (historyIndex_ == -1) {
        return;  // Not browsing history
    }
    
    // Move to next entry
    historyIndex_++;
    
    if (historyIndex_ >= static_cast<int>(history_.size())) {
        // Back to current buffer
        buffer_ = savedBuffer_;
        historyIndex_ = -1;
    } else {
        // Load history entry
        buffer_ = history_[historyIndex_];
    }
    
    cursorPos_ = buffer_.length();
}

void InputLineEditor::setColors(uint32_t foreground, uint32_t background) {
    foregroundColor_ = foreground;
    backgroundColor_ = background;
}

void InputLineEditor::handleCharacterInput() {
    if (!inputManager_) {
        return;
    }
    
    // Get typed characters from buffer
    while (inputManager_->hasCharacters()) {
        uint32_t ch = inputManager_->getNextCharacter();
        
        // Filter out control characters
        if (ch >= 32 && ch < 127) {
            insertChar(static_cast<char32_t>(ch));
        }
    }
}

void InputLineEditor::handleBackspace() {
    if (cursorPos_ > 0) {
        deleteCharAt(cursorPos_ - 1);
        cursorPos_--;
        
        // Reset history browsing
        historyIndex_ = -1;
    }
}

void InputLineEditor::handleDelete() {
    if (cursorPos_ < buffer_.length()) {
        deleteCharAt(cursorPos_);
        
        // Reset history browsing
        historyIndex_ = -1;
    }
}

void InputLineEditor::handleCursorLeft() {
    if (cursorPos_ > 0) {
        cursorPos_--;
    }
}

void InputLineEditor::handleCursorRight() {
    if (cursorPos_ < buffer_.length()) {
        cursorPos_++;
    }
}

void InputLineEditor::handleHome() {
    cursorPos_ = 0;
}

void InputLineEditor::handleEnd() {
    cursorPos_ = buffer_.length();
}

void InputLineEditor::handleEnter() {
    lineComplete_ = true;
    historyIndex_ = -1;
}

void InputLineEditor::updateKeyStates() {
    if (!inputManager_) {
        return;
    }
    
    // Update key press states for edge detection
    if (!inputManager_->isKeyPressed(KeyCode::Backspace)) {
        backspacePressed_ = false;
    }
    
    // Delete key not tracked
    
    if (!inputManager_->isKeyPressed(KeyCode::Left)) {
        leftPressed_ = false;
    }
    
    if (!inputManager_->isKeyPressed(KeyCode::Right)) {
        rightPressed_ = false;
    }
    
    if (!inputManager_->isKeyPressed(KeyCode::Up)) {
        upPressed_ = false;
    }
    
    if (!inputManager_->isKeyPressed(KeyCode::Down)) {
        downPressed_ = false;
    }
    
    if (!inputManager_->isKeyPressed(KeyCode::Enter)) {
        enterPressed_ = false;
    }
}

void InputLineEditor::insertChar(char32_t ch) {
    // Check length limit
    if (buffer_.length() >= maxLength_) {
        return;
    }
    
    // Insert character at cursor position
    buffer_.insert(cursorPos_, 1, static_cast<char>(ch));
    cursorPos_++;
    
    // Reset history browsing
    historyIndex_ = -1;
}

void InputLineEditor::deleteCharAt(size_t pos) {
    if (pos < buffer_.length()) {
        buffer_.erase(pos, 1);
    }
}

std::string InputLineEditor::getDisplayString(const std::string& prompt) const {
    return prompt + buffer_;
}

} // namespace SuperTerminal