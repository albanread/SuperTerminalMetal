//
//  TextEditor.cpp
//  SuperTerminal Framework - Text Editor Implementation
//
//  Main text editor class - integrates all editor components
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "TextEditor.h"
#include "Document.h"
#include "TextBuffer.h"
#include "Cursor.h"
#include "EditorRenderer.h"
#include "InputHandler.h"
#include "Display/TextGrid.h"
#include "Display/CharacterMapping.h"
#include "Input/InputManager.h"
#include "API/superterminal_api.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <cstdlib>

namespace SuperTerminal {

// =============================================================================
// Constructor / Destructor
// =============================================================================

TextEditor::TextEditor(std::shared_ptr<TextGrid> textGrid,
                       std::shared_ptr<InputManager> inputManager)
    : m_textGrid(textGrid)
    , m_inputManager(inputManager)
    , m_filename("untitled")
    , m_language("text")
    , m_scrollLine(0)
    , m_initialized(false)
    , m_autoSaveEnabled(false)
    , m_autoSaveInterval(30.0)
    , m_timeSinceLastSave(0.0)
{
}

TextEditor::~TextEditor() {
    shutdown();
}

// =============================================================================
// Lifecycle
// =============================================================================

bool TextEditor::initialize() {
    if (m_initialized) {
        return true;
    }

    // Create components
    m_buffer = std::make_unique<TextBuffer>();
    m_cursor = std::make_unique<Cursor>();
    m_renderer = std::make_unique<EditorRenderer>(m_textGrid);
    m_inputHandler = std::make_unique<InputHandler>(m_inputManager);

    // Load default key bindings (macOS style)
    m_inputHandler->loadDefaultBindings();

    // Set up action callback to route high-level actions back to us
    m_inputHandler->setActionCallback([this](EditorAction action) {
        handleAction(action);
    });

    m_initialized = true;
    return true;
}

void TextEditor::shutdown() {
    if (!m_initialized) {
        return;
    }

    // Clean up components
    m_inputHandler.reset();
    m_renderer.reset();
    m_cursor.reset();
    m_buffer.reset();

    m_initialized = false;
}

// =============================================================================
// Document Integration
// =============================================================================

void TextEditor::setDocument(std::shared_ptr<Document> document) {
    m_document = document;
    
    if (m_document) {
        // Sync TextBuffer with Document content (temporary bridge)
        // TODO Phase 2: Remove TextBuffer and read directly from Document
        std::string content = m_document->getText();
        if (m_buffer) {
            m_buffer->setText(content);
        }
        
        // Update filename
        m_filename = m_document->getName();
        
        // Reset cursor to start
        if (m_cursor && m_buffer) {
            m_cursor->setPosition(0, 0, *m_buffer);
        }
        
        // Reset scroll
        m_scrollLine = 0;
    }
}

// =============================================================================
// Update and Render
// =============================================================================

void TextEditor::update(double deltaTime) {
    if (!m_initialized) {
        return;
    }

    // Update cursor blink animation
    updateCursorBlink(deltaTime);

    // Update auto-save timer
    if (m_autoSaveEnabled) {
        updateAutoSave(deltaTime);
    }
}

void TextEditor::render() {
    if (!m_initialized) {
        return;
    }

    // Render buffer to text grid
    m_renderer->render(*m_buffer, *m_cursor, m_scrollLine);
}

void TextEditor::setTextGrid(std::shared_ptr<TextGrid> textGrid) {
    if (!textGrid) {
        return;
    }
    
    // Update text grid reference
    m_textGrid = textGrid;
    
    // Update renderer with new grid
    if (m_renderer) {
        m_renderer->setTextGrid(textGrid);
    }
    
    // Clamp cursor position to new grid dimensions if needed
    if (m_cursor && m_textGrid) {
        int maxCols = m_textGrid->getWidth();
        int maxRows = m_textGrid->getHeight();
        
        // Get current cursor position
        size_t line = m_cursor->getLine();
        size_t col = m_cursor->getColumn();
        
        // Adjust scroll if needed
        if (m_scrollLine > 0 && line < m_scrollLine + maxRows) {
            // Try to keep cursor visible
            if (line >= maxRows) {
                m_scrollLine = line - maxRows + 1;
            }
        }
    }
    
    // Re-render to new grid
    render();
}

void TextEditor::processInput() {
    if (!m_initialized) {
        return;
    }

    // Update page scroll lines to half text grid height
    int gridHeight = m_textGrid->getHeight();
    m_inputHandler->setPageScrollLines(gridHeight / 2);

    // Track cursor position before input processing
    size_t cursorLineBefore = m_cursor->getLine();
    size_t cursorColumnBefore = m_cursor->getColumn();
    
    // Track if we were in compose mode before processing input
    bool wasInComposeMode = m_inputManager && m_inputManager->isInComposeSequence();

    // Check for Ctrl+P to enter compose mode (without selection)
    // Using Ctrl+P instead of Alt to avoid macOS Alt character transformation
    if (m_inputManager && 
        m_inputManager->isKeyJustPressed(KeyCode::P) && 
        m_inputManager->isControlPressed() && 
        !hasSelection()) {
        m_inputManager->enterComposeMode();
        m_inputManager->clearCharacterBuffer(); // Clear any Ctrl+P characters
        return; // Skip normal input processing
    }

    // Check for Alt key + selection for compose functionality
    if (m_inputManager && m_inputManager->isAltPressed() && hasSelection()) {
        // First, consume any pending characters that might be Alt-modified chars
        // This prevents them from being inserted after composition
        if (m_inputManager->hasCharacters()) {
            // Drain the character buffer
            while (m_inputManager->hasCharacters()) {
                m_inputManager->getNextCharacter();
            }
        }
        
        std::string selected = getSelectedText();
        uint32_t composedAscii = tryComposeFromSelection(selected);
        if (composedAscii > 0) {
            // Delete selection and insert composed character as single ASCII byte
            deleteSelection();
            
            // Insert as single byte (ASCII value 128-255)
            char asciiChar = static_cast<char>(composedAscii);
            std::string asciiStr(1, asciiChar);
            
            insertText(asciiStr);
            
            // Move cursor right by 1 to position after the inserted character
            m_cursor->setPosition(m_cursor->getLine(), m_cursor->getColumn() + 1, *m_buffer);
            
            // Clear buffer again in case any events arrived during insertion
            m_inputManager->clearCharacterBuffer();
            
            return; // Skip normal input processing
        }
    }

    // Process keyboard input through input handler
    m_inputHandler->processInput(*m_buffer, *m_cursor);
    
    // Check if cursor moved from keyboard input
    bool cursorMoved = (m_cursor->getLine() != cursorLineBefore || 
                        m_cursor->getColumn() != cursorColumnBefore);

    // Process mouse input
    if (m_inputManager && m_textGrid) {
        // Get mouse grid position directly from InputManager
        // This uses the correct cell size in points (view coordinate space)
        // that matches the mouse event coordinate space, accounting for retina scaling
        int gridX = 0, gridY = 0;
        m_inputManager->getMouseGridPosition(&gridX, &gridY);

        // Check mouse button states
        bool leftPressed = m_inputManager->isMouseButtonPressed(MouseButton::Left);
        bool leftJustPressed = m_inputManager->isMouseButtonJustPressed(MouseButton::Left);
        bool leftJustReleased = m_inputManager->isMouseButtonJustReleased(MouseButton::Left);
        bool doubleClick = m_inputManager->isDoubleClick();

        // Track mouse state between frames
        static bool wasPressed = false;
        static int clickGridX = -1, clickGridY = -1;

        // Handle double-click (select word)
        if (doubleClick && leftJustPressed) {
            handleMouseClick(gridX, gridY, 0);
            m_cursor->selectWord(*m_buffer);
            m_inputManager->clearDoubleClick();  // Clear flag after processing
            cursorMoved = true;
        }
        // Handle single click (just pressed)
        else if (leftJustPressed) {
            handleMouseClick(gridX, gridY, 0);
            clickGridX = gridX;
            clickGridY = gridY;
            wasPressed = true;
            cursorMoved = true;  // Mouse click moves cursor
        }
        // Handle mouse drag (button held and position changed)
        else if (leftPressed && wasPressed) {
            // Only start drag if mouse moved from click position
            if (gridX != clickGridX || gridY != clickGridY) {
                handleMouseDrag(gridX, gridY);
            }
        }
        // Handle mouse release
        else if (leftJustReleased) {
            handleMouseRelease();
            wasPressed = false;
        }

        // Update pressed state
        if (!leftPressed) {
            wasPressed = false;
        }
    }

    // Handle scroll wheel
    if (m_inputManager) {
        float wheelDx = 0.0f, wheelDy = 0.0f;
        m_inputManager->getMouseWheel(&wheelDx, &wheelDy);
        
        if (wheelDy != 0.0f) {
            handleMouseWheel(wheelDx, wheelDy);
        }
    }

    // Only scroll to cursor if cursor actually moved (keyboard/mouse navigation)
    // This allows free scrolling with mouse wheel without viewport jumping back
    if (cursorMoved) {
        scrollToCursor();
    }
}

// =============================================================================
// Content Management
// =============================================================================

void TextEditor::loadText(const std::string& text) {
    if (!m_initialized) {
        return;
    }

    m_buffer->setText(text);
    m_cursor->setPosition(0, 0, *m_buffer);
    m_scrollLine = 0;
    syncToDocument();
}

std::string TextEditor::getText() const {
    if (!m_initialized) {
        return "";
    }

    return m_buffer->getText();
}

void TextEditor::clear() {
    if (!m_initialized) {
        return;
    }

    m_buffer->clear();
    m_cursor->setPosition(0, 0, *m_buffer);
    m_scrollLine = 0;
    syncToDocument();
}

bool TextEditor::isEmpty() const {
    if (!m_initialized) {
        return true;
    }

    return m_buffer->isEmpty();
}

// =============================================================================
// File State
// =============================================================================

bool TextEditor::isDirty() const {
    if (!m_initialized) {
        return false;
    }

    return m_buffer->isDirty();
}

void TextEditor::markClean() {
    if (!m_initialized) {
        return;
    }

    m_buffer->markClean();
    m_timeSinceLastSave = 0.0;
}

void TextEditor::markDirty() {
    if (!m_initialized) {
        return;
    }

    m_buffer->markDirty();
}

// =============================================================================
// Cursor and Selection
// =============================================================================

size_t TextEditor::getCursorLine() const {
    if (!m_initialized) {
        return 0;
    }

    return m_cursor->getLine();
}

size_t TextEditor::getCursorColumn() const {
    if (!m_initialized) {
        return 0;
    }

    return m_cursor->getColumn();
}

void TextEditor::setCursorPosition(size_t line, size_t column) {
    if (!m_initialized) {
        return;
    }

    m_cursor->setPosition(line, column, *m_buffer);
    scrollToCursor();
}

bool TextEditor::hasSelection() const {
    if (!m_initialized) {
        return false;
    }

    return m_cursor->hasSelection();
}

std::string TextEditor::getSelectedText() const {
    if (!m_initialized || !m_cursor->hasSelection()) {
        return "";
    }

    auto selection = m_cursor->getSelection();
    Position start = selection.first;
    Position end = selection.second;

    // Single line selection
    if (start.line == end.line) {
        std::string line = m_buffer->getLine(start.line);
        if (end.column > start.column && start.column < line.length()) {
            size_t length = std::min(end.column - start.column, line.length() - start.column);
            return line.substr(start.column, length);
        }
        return "";
    }

    // Multi-line selection
    std::string result;
    for (size_t line = start.line; line <= end.line && line < m_buffer->getLineCount(); ++line) {
        std::string lineText = m_buffer->getLine(line);

        if (line == start.line) {
            // First line: from start column to end
            if (start.column < lineText.length()) {
                result += lineText.substr(start.column);
            }
            result += "\n";
        } else if (line == end.line) {
            // Last line: from start to end column
            size_t endCol = std::min(end.column, lineText.length());
            result += lineText.substr(0, endCol);
        } else {
            // Middle lines: entire line
            result += lineText + "\n";
        }
    }

    return result;
}

void TextEditor::clearSelection() {
    if (!m_initialized) {
        return;
    }

    m_cursor->clearSelection();
}

// =============================================================================
// Editing Operations
// =============================================================================

void TextEditor::insertText(const std::string& text) {
    if (!m_initialized) {
        return;
    }

    // Delete selection first if any
    if (m_cursor->hasSelection()) {
        deleteSelection();
    }

    // Insert text at cursor position
    size_t line = m_cursor->getLine();
    size_t column = m_cursor->getColumn();

    // Handle multi-line text
    size_t newlinePos = text.find('\n');
    if (newlinePos != std::string::npos) {
        // Split on newlines and insert
        std::vector<std::string> lines;
        size_t start = 0;
        while (start < text.length()) {
            size_t end = text.find('\n', start);
            if (end == std::string::npos) {
                lines.push_back(text.substr(start));
                break;
            }
            lines.push_back(text.substr(start, end - start));
            start = end + 1;
        }

        if (!lines.empty()) {
            // Insert first line at cursor
            m_buffer->insertText(line, column, lines[0]);
            column += lines[0].length();

            // Insert remaining lines
            for (size_t i = 1; i < lines.size(); ++i) {
                m_buffer->splitLine(line, column);
                line++;
                column = 0;
                if (!lines[i].empty()) {
                    m_buffer->insertText(line, column, lines[i]);
                    column = lines[i].length();
                }
            }

            m_cursor->setPosition(line, column, *m_buffer);
        }
    } else {
        // Single line insert
        m_buffer->insertText(line, column, text);
        m_cursor->setPosition(line, column + text.length(), *m_buffer);
    }

    m_buffer->markDirty();
    syncToDocument();
}

void TextEditor::deleteSelection() {
    if (!m_initialized || !m_cursor->hasSelection()) {
        return;
    }

    auto selection = m_cursor->getSelection();
    Position start = selection.first;
    Position end = selection.second;

    // Use deleteRange for efficient multi-line deletion
    m_buffer->deleteRange(start.line, start.column, end.line, end.column);

    // Move cursor to start of deleted region
    m_cursor->setPosition(start.line, start.column, *m_buffer);
    m_cursor->clearSelection();
    m_buffer->markDirty();
    syncToDocument();
}

bool TextEditor::undo() {
    if (!m_initialized) {
        return false;
    }

    bool result = m_buffer->undo();
    if (result) {
        // Cursor position should be managed by the buffer's undo state
        // For now, just clear selection and scroll
        m_cursor->clearSelection();
        scrollToCursor();
        syncToDocument();
    }
    return result;
}

void TextEditor::syncToDocument() {
    // Sync TextBuffer changes to Document (Phase 2 bridge)
    // TODO: Remove when TextEditor writes directly to Document
    if (m_document && m_buffer) {
        std::string content = m_buffer->getText();
        m_document->setText(content);
        if (m_buffer->isDirty()) {
            m_document->markDirty();
        }
    }
}

bool TextEditor::redo() {
    if (!m_initialized) {
        return false;
    }

    bool result = m_buffer->redo();
    if (result) {
        // Cursor position should be managed by the buffer's redo state
        // For now, just clear selection and scroll
        m_cursor->clearSelection();
        scrollToCursor();
    }
    return result;
}

bool TextEditor::canUndo() const {
    if (!m_initialized) {
        return false;
    }

    return m_buffer->canUndo();
}

bool TextEditor::canRedo() const {
    if (!m_initialized) {
        return false;
    }

    return m_buffer->canRedo();
}

// =============================================================================
// Clipboard Operations
// =============================================================================

void TextEditor::cut() {
    if (!m_initialized || !m_clipboardCallback) {
        return;
    }

    performClipboardOperation("cut");
}

void TextEditor::copy() {
    if (!m_initialized || !m_clipboardCallback) {
        return;
    }

    performClipboardOperation("copy");
}

void TextEditor::paste() {
    if (!m_initialized || !m_clipboardCallback) {
        return;
    }

    performClipboardOperation("paste");
}



void TextEditor::performClipboardOperation(const std::string& operation) {
    if (operation == "cut" || operation == "copy") {
        if (!m_cursor->hasSelection()) {
            return;
        }

        std::string text = getSelectedText();
        m_clipboardCallback(operation, text);

        if (operation == "cut") {
            deleteSelection();
        }
    } else if (operation == "paste") {
        std::string text = m_clipboardCallback(operation, "");
        if (!text.empty()) {
            insertText(text);
        }
    }
}

// =============================================================================
// Syntax Highlighting
// =============================================================================

void TextEditor::setSyntaxHighlighter(SyntaxHighlighter highlighter) {
    if (!m_initialized) {
        return;
    }

    m_renderer->setSyntaxHighlighter(highlighter);
}

void TextEditor::clearSyntaxHighlighter() {
    if (!m_initialized) {
        return;
    }

    m_renderer->clearSyntaxHighlighter();
}

// =============================================================================
// Post-Newline Callback
// =============================================================================

void TextEditor::setPostNewlineCallback(PostNewlineCallback callback) {
    if (!m_initialized) {
        return;
    }
    
    m_postNewlineCallback = callback;
    
    // Forward to input handler
    if (m_inputHandler) {
        m_inputHandler->setPostNewlineCallback(callback);
    }
}

void TextEditor::clearPostNewlineCallback() {
    if (!m_initialized) {
        return;
    }
    
    m_postNewlineCallback = nullptr;
    
    // Clear in input handler
    if (m_inputHandler) {
        m_inputHandler->setPostNewlineCallback(nullptr);
    }
}

// =============================================================================
// Action Callbacks
// =============================================================================

void TextEditor::setActionCallback(ActionCallback callback) {
    m_actionCallback = callback;
}

void TextEditor::handleAction(EditorAction action) {
    // Handle actions that the editor can handle directly
    switch (action) {
        case EditorAction::CUT:
            cut();
            break;

        case EditorAction::COPY:
            copy();
            break;

        case EditorAction::PASTE:
            paste();
            break;

        case EditorAction::UNDO:
            undo();
            break;

        case EditorAction::REDO:
            redo();
            break;

        case EditorAction::SELECT_ALL:
            if (m_buffer->getLineCount() > 0) {
                m_cursor->setPosition(0, 0, *m_buffer);
                m_cursor->startSelection();
                size_t lastLine = m_buffer->getLineCount() - 1;
                size_t lastCol = m_buffer->getLine(lastLine).length();
                m_cursor->setPosition(lastLine, lastCol, *m_buffer);
            }
            break;

        case EditorAction::CLEAR_SELECTION:
            clearSelection();
            break;

        case EditorAction::TOGGLE_LINE_NUMBERS:
            m_renderer->setLineNumbersVisible(!m_renderer->areLineNumbersVisible());
            break;

        default:
            // Forward high-level actions to callback
            if (m_actionCallback) {
                m_actionCallback(action);
            }
            break;
    }
}

// =============================================================================
// Configuration
// =============================================================================

void TextEditor::setTabWidth(int width) {
    if (!m_initialized) {
        return;
    }

    m_inputHandler->setTabWidth(width);
}

int TextEditor::getTabWidth() const {
    if (!m_initialized) {
        return 4;
    }

    return m_inputHandler->getTabWidth();
}

void TextEditor::setUseSpacesForTab(bool useSpaces) {
    if (!m_initialized) {
        return;
    }

    m_inputHandler->setUseSpacesForTab(useSpaces);
}

bool TextEditor::getUseSpacesForTab() const {
    if (!m_initialized) {
        return true;
    }

    return m_inputHandler->getUseSpacesForTab();
}

void TextEditor::setAutoIndent(bool autoIndent) {
    if (!m_initialized) {
        return;
    }

    m_inputHandler->setAutoIndent(autoIndent);
}

bool TextEditor::getAutoIndent() const {
    if (!m_initialized) {
        return true;
    }

    return m_inputHandler->getAutoIndent();
}

void TextEditor::setLineNumbersVisible(bool visible) {
    if (!m_initialized) {
        return;
    }

    m_renderer->setLineNumbersVisible(visible);
}

bool TextEditor::areLineNumbersVisible() const {
    if (!m_initialized) {
        return true;
    }

    return m_renderer->areLineNumbersVisible();
}

void TextEditor::setCursorBlinkRate(double rate) {
    if (!m_initialized) {
        return;
    }

    m_cursor->setBlinkRate(rate);
}

double TextEditor::getCursorBlinkRate() const {
    if (!m_initialized) {
        return 0.5;
    }

    return m_cursor->getBlinkRate();
}

// =============================================================================
// Viewport and Scrolling
// =============================================================================

void TextEditor::setScrollLine(int line) {
    if (!m_initialized) {
        return;
    }

    size_t lineCount = m_buffer->getLineCount();
    m_scrollLine = std::max(0, std::min(line, static_cast<int>(lineCount) - 1));
}

void TextEditor::scrollToCursor() {
    if (!m_initialized) {
        return;
    }

    int viewportHeight = getViewportHeight();
    size_t cursorLine = m_cursor->getLine();

    // Scroll up if cursor is above viewport
    if (cursorLine < static_cast<size_t>(m_scrollLine)) {
        m_scrollLine = static_cast<int>(cursorLine);
    }

    // Scroll down if cursor is below viewport
    if (cursorLine >= static_cast<size_t>(m_scrollLine + viewportHeight)) {
        m_scrollLine = static_cast<int>(cursorLine) - viewportHeight + 1;
    }

    // Clamp scroll position
    setScrollLine(m_scrollLine);
}

void TextEditor::scrollBy(int lines) {
    setScrollLine(m_scrollLine + lines);
}

int TextEditor::getViewportHeight() const {
    if (!m_initialized) {
        return 0;
    }

    // Get text grid height and subtract space for status bars (top + bottom = 2)
    int gridHeight = m_textGrid->getHeight();
    int statusBarLines = 2;  // Top status bar + bottom status bar
    return std::max(1, gridHeight - statusBarLines);
}

// =============================================================================
// Mouse Input
// =============================================================================

void TextEditor::handleMouseClick(int gridX, int gridY, int button) {
    if (!m_initialized) {
        return;
    }

    // Convert grid coordinates to buffer position
    int gutterWidth = m_renderer->areLineNumbersVisible() ? m_renderer->getLineNumberGutterWidth() : 0;
    int textX = gridX - gutterWidth;
    int textY = gridY;

    if (textX < 0 || textY < 0) {
        return;
    }

    // Move cursor to clicked position (without starting selection)
    size_t newLine = static_cast<size_t>(std::max(0, m_scrollLine + textY));
    size_t newColumn = static_cast<size_t>(std::max(0, textX));

    // Clamp to buffer bounds
    if (newLine >= m_buffer->getLineCount()) {
        newLine = m_buffer->getLineCount() > 0 ? m_buffer->getLineCount() - 1 : 0;
    }
    if (newLine < m_buffer->getLineCount()) {
        size_t lineLength = m_buffer->getLine(newLine).length();
        if (newColumn > lineLength) {
            newColumn = lineLength;
        }
    }

    // Clear any existing selection and move cursor
    m_cursor->clearSelection();
    m_cursor->setPosition(newLine, newColumn, *m_buffer);
}

void TextEditor::handleMouseDrag(int gridX, int gridY) {
    if (!m_initialized) {
        return;
    }

    // Convert grid coordinates to buffer position
    int gutterWidth = m_renderer->areLineNumbersVisible() ? m_renderer->getLineNumberGutterWidth() : 0;
    int textX = gridX - gutterWidth;
    int textY = gridY;

    if (textY < 0) {
        return;
    }

    // If not already selecting, start selection from current cursor position
    if (!m_cursor->isMouseSelecting()) {
        m_cursor->startMouseSelection(m_cursor->getColumn(), m_cursor->getLine(), *m_buffer);
    }

    // Extend selection to drag position
    m_cursor->extendMouseSelection(std::max(0, textX), m_scrollLine + std::max(0, textY), *m_buffer);
    scrollToCursor();
}

void TextEditor::handleMouseRelease() {
    if (!m_initialized) {
        return;
    }

    m_cursor->endMouseSelection();
}

void TextEditor::handleMouseWheel(float deltaX, float deltaY) {
    if (!m_initialized) {
        return;
    }

    // Scroll by lines (negative deltaY = scroll up)
    // Apply sensitivity divisor to slow down scrolling (3.0 = slower, feels more natural)
    constexpr float scrollSensitivity = 3.0f;
    int lines = static_cast<int>(-deltaY / scrollSensitivity);
    
    // Only scroll if we have at least 1 line worth of movement
    if (lines != 0) {
        scrollBy(lines);
    }
}

// =============================================================================
// Statistics
// =============================================================================

size_t TextEditor::getLineCount() const {
    if (!m_initialized) {
        return 0;
    }

    return m_buffer->getLineCount();
}

size_t TextEditor::getCharacterCount() const {
    if (!m_initialized) {
        return 0;
    }

    size_t count = 0;
    for (size_t i = 0; i < m_buffer->getLineCount(); ++i) {
        count += m_buffer->getLine(i).length();
        if (i < m_buffer->getLineCount() - 1) {
            count++; // Count newline
        }
    }
    return count;
}

size_t TextEditor::getMemoryUsage() const {
    if (!m_initialized) {
        return 0;
    }

    // Estimate memory usage
    size_t usage = 0;
    for (size_t i = 0; i < m_buffer->getLineCount(); ++i) {
        usage += m_buffer->getLine(i).capacity();
    }
    return usage;
}

// =============================================================================
// Status Information
// =============================================================================

std::string TextEditor::getStatusString() const {
    if (!m_initialized) {
        return "";
    }

    // Format: "filename [modified] - Line X, Col Y - N lines - language"
    std::string status = m_filename;
    if (isDirty()) {
        status += " [modified]";
    }

    status += " - Line " + std::to_string(m_cursor->getLine() + 1);
    status += ", Col " + std::to_string(m_cursor->getColumn() + 1);
    status += " - " + std::to_string(m_buffer->getLineCount()) + " lines";
    status += " - " + m_language;

    return status;
}

std::string TextEditor::getCommandString() const {
    if (!m_initialized) {
        return "";
    }

    // Format: "^X Exit  ^O Save  ^R Run  ^K Cut  ^U Paste"
    return "^X Exit  ^O Save  ^R Run  ^K Cut  ^U Paste";
}

// =============================================================================
// Auto-save
// =============================================================================

void TextEditor::enableAutoSave(double intervalSeconds) {
    m_autoSaveEnabled = true;
    m_autoSaveInterval = intervalSeconds;
    m_timeSinceLastSave = 0.0;
}

void TextEditor::disableAutoSave() {
    m_autoSaveEnabled = false;
}

void TextEditor::triggerAutoSave() {
    if (!m_initialized || !isDirty() || !m_actionCallback) {
        return;
    }

    m_actionCallback(EditorAction::SAVE_FILE);
    m_timeSinceLastSave = 0.0;
}

// =============================================================================
// Internal Helpers
// =============================================================================

void TextEditor::updateCursorBlink(double deltaTime) {
    m_cursor->updateBlink(deltaTime);
}

void TextEditor::updateAutoSave(double deltaTime) {
    if (!isDirty()) {
        m_timeSinceLastSave = 0.0;
        return;
    }

    m_timeSinceLastSave += deltaTime;

    if (m_timeSinceLastSave >= m_autoSaveInterval) {
        triggerAutoSave();
    }
}

// =============================================================================
// Compose Character from Selection
// =============================================================================

uint32_t TextEditor::tryComposeFromSelection(const std::string& selected) {
    // Try 2-character compose sequence
    if (selected.length() == 2) {
        uint8_t ascii = m_inputManager->handleComposeSequence(selected[0], selected[1]);
        if (ascii >= 128) {
            // Return the ASCII value (128-255), not the Unicode codepoint
            return ascii;
        }
    }
    // Try 3-digit ASCII code
    else if (selected.length() == 3) {
        bool allDigits = true;
        for (char c : selected) {
            if (c < '0' || c > '9') {
                allDigits = false;
                break;
            }
        }
        if (allDigits) {
            int code = std::atoi(selected.c_str());
            if (code >= 128 && code <= 255) {
                // Return the ASCII value (128-255), not the Unicode codepoint
                return static_cast<uint32_t>(code);
            }
        }
    }
    
    return 0; // Invalid
}

} // namespace SuperTerminal
