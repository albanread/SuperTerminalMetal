//
//  SimpleLineEditor.cpp
//  SuperTerminal Framework - Simple Line Editor Implementation
//
//  Reusable line editor for INPUT_AT and similar input operations
//  Based on FBRunner3's interactive mode editing logic
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "SimpleLineEditor.h"
#include "../Display/TextGrid.h"
#include "../Input/InputManager.h"
#include "InputManager.h"

namespace SuperTerminal {

// =============================================================================
// Constructor / Destructor
// =============================================================================

SimpleLineEditor::SimpleLineEditor(int x, int y, const std::string& prompt)
    : _currentInput("")
    , _cursorPos(0)
    , _displayX(x)
    , _displayY(y)
    , _prompt(prompt)
    , _isComplete(false)
    , _maxLength(0)  // unlimited
    , _passwordMode(false)
    , _showCursor(true)
    , _blinkCounter(0)
    , _lastInputTime(std::chrono::steady_clock::now())
{
    printf("DEBUG: SimpleLineEditor created at (%d, %d) with prompt='%s'\n", x, y, prompt.c_str());
}

SimpleLineEditor::~SimpleLineEditor() {
}

// =============================================================================
// Main Update Loop
// =============================================================================

bool SimpleLineEditor::update(InputManager* inputManager) {
    if (!inputManager || _isComplete) {
        return _isComplete;
    }
    
    static int updateCount = 0;
    if (updateCount % 60 == 0) {
        printf("DEBUG: SimpleLineEditor::update() called (count=%d), input='%s', cursorPos=%d\n", 
               updateCount, _currentInput.c_str(), _cursorPos);
    }
    updateCount++;
    
    // Update cursor blink (blink every 90 frames = ~1.5 seconds at 60fps)
    _blinkCounter++;
    if (_blinkCounter >= 90) {
        _showCursor = !_showCursor;
        _blinkCounter = 0;
    }
    
    bool canProcess = canProcessInput();
    
    // Handle character input
    while (inputManager->hasCharacters()) {
        uint32_t ch = inputManager->getNextCharacter();
        printf("DEBUG: SimpleLineEditor got character: %c (0x%x)\n", (ch >= 32 && ch < 127) ? (char)ch : '?', ch);
        if (ch >= 32 && ch < 127) {  // Printable ASCII
            if (_maxLength == 0 || (int)_currentInput.length() < _maxLength) {
                insertCharacter((char)ch);
                updateInputTiming();
            }
        }
    }
    
    // Handle special keys with timing control
    if (inputManager->isKeyJustPressed(KeyCode::Enter) || inputManager->isKeyJustPressed(KeyCode::Tab)) {
        printf("DEBUG: SimpleLineEditor detected Enter/Tab - completing with input='%s'\n", _currentInput.c_str());
        _isComplete = true;
        return true;
    }
    
    if (inputManager->isKeyJustPressed(KeyCode::Escape)) {
        // Cancel input - clear and complete
        _currentInput.clear();
        _cursorPos = 0;
        _isComplete = true;
        return true;
    }
    
    if (canProcess && inputManager->isKeyJustPressed(KeyCode::Backspace)) {
        deleteCharacterBefore();
        updateInputTiming();
    }
    
    if (canProcess && inputManager->isKeyJustPressed(KeyCode::Delete)) {
        deleteCharacterAt();
        updateInputTiming();
    }
    
    if (canProcess && inputManager->isKeyJustPressed(KeyCode::Home)) {
        moveCursorHome();
        updateInputTiming();
    }
    
    if (canProcess && inputManager->isKeyJustPressed(KeyCode::End)) {
        moveCursorEnd();
        updateInputTiming();
    }
    
    if (canProcess && inputManager->isKeyJustPressed(KeyCode::Left)) {
        if (inputManager->isCommandPressed()) {
            // Command+Left: Jump to start of line (macOS standard)
            moveCursorHome();
        } else if (inputManager->isControlPressed() || inputManager->isAltPressed()) {
            // Ctrl+Left or Option+Left: Jump to previous word
            moveCursorWordLeft();
        } else {
            moveCursorLeft();
        }
        updateInputTiming();
    }
    
    if (canProcess && inputManager->isKeyJustPressed(KeyCode::Right)) {
        if (inputManager->isCommandPressed()) {
            // Command+Right: Jump to end of line (macOS standard)
            moveCursorEnd();
        } else if (inputManager->isControlPressed() || inputManager->isAltPressed()) {
            // Ctrl+Right or Option+Right: Jump to next word
            moveCursorWordRight();
        } else {
            moveCursorRight();
        }
        updateInputTiming();
    }
    
    return false;
}

// =============================================================================
// Rendering
// =============================================================================

void SimpleLineEditor::render(TextGrid* textGrid) {
    if (!textGrid) {
        printf("DEBUG: SimpleLineEditor::render() called with NULL textGrid!\n");
        return;
    }
    
    static int renderCount = 0;
    if (renderCount % 60 == 0) {
        printf("DEBUG: SimpleLineEditor::render() called (count=%d), input='%s'\n", renderCount, _currentInput.c_str());
    }
    renderCount++;
    
    renderPrompt(textGrid);
    renderText(textGrid);
    renderCursor(textGrid);
}

void SimpleLineEditor::renderPrompt(TextGrid* textGrid) {
    if (_prompt.empty()) {
        return;
    }
    
    // Render prompt at the start position
    textGrid->putString(_displayX, _displayY, _prompt.c_str(), 
                       0xFFFFFFFF,  // White text
                       0xFF000000); // Black background
}

void SimpleLineEditor::renderText(TextGrid* textGrid) {
    std::string displayText = getDisplayText();
    int promptWidth = _prompt.length();
    
    // Render input text after the prompt
    if (!displayText.empty()) {
        textGrid->putString(_displayX + promptWidth, _displayY, displayText.c_str(),
                           0xFFFFFFFF,  // White text
                           0xFF000000); // Black background
    }
    
    // Clear any remaining characters on the line (in case text got shorter)
    int textEndX = _displayX + promptWidth + displayText.length();
    int gridWidth = textGrid->getWidth();
    for (int x = textEndX; x < gridWidth && x < _displayX + promptWidth + 80; x++) {
        textGrid->putChar(x, _displayY, ' ', 0xFFFFFFFF, 0xFF000000);
    }
}

void SimpleLineEditor::renderCursor(TextGrid* textGrid) {
    if (!_showCursor) {
        return;
    }
    
    int cursorX = _displayX + _prompt.length() + _cursorPos;
    int cursorY = _displayY;
    
    // Make sure cursor is within grid bounds
    if (cursorX < textGrid->getWidth() && cursorY < textGrid->getHeight()) {
        // Get the character at cursor position (or space if at end)
        char cursorChar = ' ';
        if (_cursorPos < (int)_currentInput.length()) {
            cursorChar = _passwordMode ? '*' : _currentInput[_cursorPos];
        }
        
        // Render cursor as inverted colors
        textGrid->putChar(cursorX, cursorY, cursorChar,
                         0xFF000000,  // Black text
                         0xFFFFFFFF); // White background (inverted)
    }
}

// =============================================================================
// Public Interface
// =============================================================================

std::string SimpleLineEditor::getResult() const {
    return _currentInput;
}

std::string SimpleLineEditor::getCurrentText() const {
    return _currentInput;
}

void SimpleLineEditor::reset() {
    _currentInput.clear();
    _cursorPos = 0;
    _isComplete = false;
    _showCursor = true;
    _blinkCounter = 0;
}

void SimpleLineEditor::setPrompt(const std::string& prompt) {
    _prompt = prompt;
}

void SimpleLineEditor::setPosition(int x, int y) {
    _displayX = x;
    _displayY = y;
}

void SimpleLineEditor::setMaxLength(int maxLength) {
    _maxLength = maxLength;
}

void SimpleLineEditor::setPasswordMode(bool enabled) {
    _passwordMode = enabled;
}

// =============================================================================
// Internal Character Manipulation
// =============================================================================

void SimpleLineEditor::insertCharacter(char ch) {
    _currentInput.insert(_cursorPos, 1, ch);
    _cursorPos++;
    
    // Reset cursor blink when typing
    _showCursor = true;
    _blinkCounter = 0;
}

void SimpleLineEditor::deleteCharacterBefore() {
    if (_cursorPos > 0) {
        _currentInput.erase(_cursorPos - 1, 1);
        _cursorPos--;
        
        // Reset cursor blink when editing
        _showCursor = true;
        _blinkCounter = 0;
    }
}

void SimpleLineEditor::deleteCharacterAt() {
    if (_cursorPos < (int)_currentInput.length()) {
        _currentInput.erase(_cursorPos, 1);
        
        // Reset cursor blink when editing
        _showCursor = true;
        _blinkCounter = 0;
    }
}

// =============================================================================
// Cursor Movement
// =============================================================================

void SimpleLineEditor::moveCursorLeft() {
    if (_cursorPos > 0) {
        _cursorPos--;
        _showCursor = true;
        _blinkCounter = 0;
    }
}

void SimpleLineEditor::moveCursorRight() {
    if (_cursorPos < (int)_currentInput.length()) {
        _cursorPos++;
        _showCursor = true;
        _blinkCounter = 0;
    }
}

void SimpleLineEditor::moveCursorWordLeft() {
    if (_cursorPos > 0) {
        // Skip any word boundaries at current position
        while (_cursorPos > 0 && isWordBoundary(_currentInput[_cursorPos - 1])) {
            _cursorPos--;
        }
        // Move to start of current word
        while (_cursorPos > 0 && !isWordBoundary(_currentInput[_cursorPos - 1])) {
            _cursorPos--;
        }
        
        _showCursor = true;
        _blinkCounter = 0;
    }
}

void SimpleLineEditor::moveCursorWordRight() {
    if (_cursorPos < (int)_currentInput.length()) {
        // Skip current word
        while (_cursorPos < (int)_currentInput.length() && 
               !isWordBoundary(_currentInput[_cursorPos])) {
            _cursorPos++;
        }
        // Skip any word boundaries
        while (_cursorPos < (int)_currentInput.length() && 
               isWordBoundary(_currentInput[_cursorPos])) {
            _cursorPos++;
        }
        
        _showCursor = true;
        _blinkCounter = 0;
    }
}

void SimpleLineEditor::moveCursorHome() {
    _cursorPos = 0;
    _showCursor = true;
    _blinkCounter = 0;
}

void SimpleLineEditor::moveCursorEnd() {
    _cursorPos = (int)_currentInput.length();
    _showCursor = true;
    _blinkCounter = 0;
}

// =============================================================================
// Helper Methods
// =============================================================================

bool SimpleLineEditor::isWordBoundary(char ch) const {
    return ch == ' ' || ch == ',' || ch == ';' || ch == ':' || 
           ch == '(' || ch == ')' || ch == '"' || ch == '\'' ||
           ch == '\t' || ch == '\n' || ch == '\r';
}

std::string SimpleLineEditor::getDisplayText() const {
    if (_passwordMode) {
        return std::string(_currentInput.length(), '*');
    }
    return _currentInput;
}

int SimpleLineEditor::getDisplayWidth() const {
    return (int)_prompt.length() + (int)_currentInput.length();
}

bool SimpleLineEditor::canProcessInput() const {
    auto currentTime = std::chrono::steady_clock::now();
    return (currentTime - _lastInputTime) >= INPUT_DELAY;
}

void SimpleLineEditor::updateInputTiming() {
    _lastInputTime = std::chrono::steady_clock::now();
}

} // namespace SuperTerminal