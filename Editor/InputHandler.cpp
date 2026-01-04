//
//  InputHandler.cpp
//  SuperTerminal Framework - Input Handler Implementation
//
//  Map keyboard and mouse input to editor actions with configurable bindings
//  Copyright © 2024 SuperTerminal. All rights reserved.
//
#include "InputHandler.h"
#include "TextBuffer.h"
#include "Cursor.h"
#include "Input/InputManager.h"
#include <algorithm>

namespace SuperTerminal {

// =============================================================================
// KeyBinding Comparison
// =============================================================================

bool KeyBinding::operator<(const KeyBinding& other) const {
    if (key != other.key) return key < other.key;
    if (shift != other.shift) return shift < other.shift;
    if (ctrl != other.ctrl) return ctrl < other.ctrl;
    if (alt != other.alt) return alt < other.alt;
    return cmd < other.cmd;
}

bool KeyBinding::operator==(const KeyBinding& other) const {
    return key == other.key &&
           shift == other.shift &&
           ctrl == other.ctrl &&
           alt == other.alt &&
           cmd == other.cmd;
}

// =============================================================================
// EditorAction String Conversion
// =============================================================================

const char* editorActionToString(EditorAction action) {
    switch (action) {
        case EditorAction::MOVE_UP: return "move_up";
        case EditorAction::MOVE_DOWN: return "move_down";
        case EditorAction::MOVE_LEFT: return "move_left";
        case EditorAction::MOVE_RIGHT: return "move_right";
        case EditorAction::MOVE_LINE_START: return "move_line_start";
        case EditorAction::MOVE_LINE_END: return "move_line_end";
        case EditorAction::MOVE_DOCUMENT_START: return "move_document_start";
        case EditorAction::MOVE_DOCUMENT_END: return "move_document_end";
        case EditorAction::MOVE_WORD_LEFT: return "move_word_left";
        case EditorAction::MOVE_WORD_RIGHT: return "move_word_right";
        case EditorAction::MOVE_PAGE_UP: return "move_page_up";
        case EditorAction::MOVE_PAGE_DOWN: return "move_page_down";
        case EditorAction::MOVE_TO_SPACE_LEFT: return "move_to_space_left";
        case EditorAction::MOVE_TO_SPACE_RIGHT: return "move_to_space_right";
        case EditorAction::MOVE_SMART_HOME: return "move_smart_home";
        case EditorAction::MOVE_SMART_END: return "move_smart_end";
        case EditorAction::MOVE_UP_FAST: return "move_up_fast";
        case EditorAction::MOVE_DOWN_FAST: return "move_down_fast";
        case EditorAction::INSERT_NEWLINE: return "insert_newline";
        case EditorAction::INSERT_TAB: return "insert_tab";
        case EditorAction::DELETE_CHAR_BEFORE: return "delete_char_before";
        case EditorAction::DELETE_CHAR_AFTER: return "delete_char_after";
        case EditorAction::DELETE_LINE: return "delete_line";
        case EditorAction::DUPLICATE_LINE: return "duplicate_line";
        case EditorAction::CUT: return "cut";
        case EditorAction::COPY: return "copy";
        case EditorAction::PASTE: return "paste";
        case EditorAction::UNDO: return "undo";
        case EditorAction::REDO: return "redo";
        case EditorAction::SELECT_ALL: return "select_all";
        case EditorAction::SAVE_FILE: return "save_file";
        case EditorAction::RUN_SCRIPT: return "run_script";
        default: return "none";
    }
}

EditorAction stringToEditorAction(const char* str) {
    if (!str) return EditorAction::NONE;

    // Simple string comparison (could use a map for efficiency)
    std::string s = str;
    if (s == "move_up") return EditorAction::MOVE_UP;
    if (s == "move_down") return EditorAction::MOVE_DOWN;
    if (s == "save_file") return EditorAction::SAVE_FILE;
    // ... add more as needed

    return EditorAction::NONE;
}

// =============================================================================
// InputHandler Implementation
// =============================================================================

InputHandler::InputHandler(std::shared_ptr<InputManager> inputManager)
    : m_inputManager(inputManager)
    , m_mouseSelecting(false)
    , m_mouseStartX(0)
    , m_mouseStartY(0)
    , m_tabWidth(4)
    , m_useSpacesForTab(true)
    , m_autoIndent(true)
    , m_pageScrollLines(20)
    , m_lastAction(EditorAction::NONE)
    , m_keyRepeatInitialDelay(0.5)
    , m_keyRepeatInterval(0.05)
    , m_lastKeyCode(KeyCode::Unknown)
    , m_keyPressTime(0.0)
    , m_lastRepeatTime(0.0)
    , m_actionCallback(nullptr)
    , m_postNewlineCallback(nullptr)
{
    loadDefaultBindings();
    // Initialize processed keys array
    m_processedKeys.fill(false);
}

InputHandler::~InputHandler() {
    // Nothing to clean up
}

// =============================================================================
// Input Processing
// =============================================================================

EditorAction InputHandler::processInput(TextBuffer& buffer, Cursor& cursor) {
    if (!m_inputManager) {
        return EditorAction::NONE;
    }

    // Check for bound actions first
    EditorAction action = findAction();

    if (action != EditorAction::NONE) {
        m_lastAction = action;

        // If this was a command/control key action, clear the character buffer
        // to prevent the character from being inserted (e.g., Cmd+A shouldn't insert 'a')
        if (m_inputManager->isCommandPressed() || m_inputManager->isControlPressed()) {
            // Consume any pending characters that were part of this keyboard shortcut
            while (m_inputManager->hasCharacters()) {
                m_inputManager->getNextCharacter();
            }
        }

        // Handle actions that need buffer/cursor manipulation
        switch (action) {
            case EditorAction::MOVE_UP:
                cursor.moveUp(buffer);
                cursor.clearSelection();
                break;

            case EditorAction::MOVE_DOWN:
                cursor.moveDown(buffer);
                cursor.clearSelection();
                break;

            case EditorAction::MOVE_LEFT:
                cursor.moveLeft(buffer);
                cursor.clearSelection();
                break;

            case EditorAction::MOVE_RIGHT:
                cursor.moveRight(buffer);
                cursor.clearSelection();
                break;

            case EditorAction::MOVE_LINE_START:
                cursor.moveToLineStart();
                cursor.clearSelection();
                break;

            case EditorAction::MOVE_LINE_END:
                cursor.moveToLineEnd(buffer);
                cursor.clearSelection();
                break;

            case EditorAction::MOVE_DOCUMENT_START:
                cursor.moveToDocumentStart();
                cursor.clearSelection();
                break;

            case EditorAction::MOVE_DOCUMENT_END:
                cursor.moveToDocumentEnd(buffer);
                cursor.clearSelection();
                break;

            case EditorAction::MOVE_WORD_LEFT:
                cursor.moveWordLeft(buffer);
                cursor.clearSelection();
                break;

            case EditorAction::MOVE_WORD_RIGHT:
                cursor.moveWordRight(buffer);
                cursor.clearSelection();
                break;

            case EditorAction::MOVE_TO_SPACE_LEFT:
                // Move to previous space
                {
                    std::string line = buffer.getLine(cursor.getLine());
                    size_t col = cursor.getColumn();

                    if (col > 0) {
                        // Move back to find previous space or start of line
                        size_t newCol = col - 1;

                        // Skip current character if it's a space
                        while (newCol > 0 && line[newCol] == ' ') {
                            newCol--;
                        }

                        // Find the previous space
                        while (newCol > 0 && line[newCol] != ' ') {
                            newCol--;
                        }

                        // If we found a space, position after it, otherwise go to start
                        if (newCol > 0 || line[0] == ' ') {
                            cursor.setPosition(cursor.getLine(), newCol, buffer);
                        } else {
                            cursor.setPosition(cursor.getLine(), 0, buffer);
                        }
                    }
                }
                cursor.clearSelection();
                break;

            case EditorAction::MOVE_TO_SPACE_RIGHT:
                // Move to next space
                {
                    std::string line = buffer.getLine(cursor.getLine());
                    size_t col = cursor.getColumn();

                    if (col < line.length()) {
                        // Skip current spaces
                        size_t newCol = col;
                        while (newCol < line.length() && line[newCol] == ' ') {
                            newCol++;
                        }

                        // Find the next space
                        while (newCol < line.length() && line[newCol] != ' ') {
                            newCol++;
                        }

                        cursor.setPosition(cursor.getLine(), newCol, buffer);
                    }
                }
                cursor.clearSelection();
                break;

            case EditorAction::MOVE_SMART_HOME:
                // Ctrl+Left: Toggle between line start and position after line number
                {
                    std::string line = buffer.getLine(cursor.getLine());
                    size_t col = cursor.getColumn();

                    // Find first non-whitespace character
                    size_t firstNonSpace = line.find_first_not_of(" \t");

                    if (firstNonSpace == std::string::npos) {
                        // Line is empty or all whitespace - go to start
                        cursor.setPosition(cursor.getLine(), 0, buffer);
                    } else {
                        // Check if line starts with a number (line number)
                        size_t afterLineNumber = firstNonSpace;
                        if (std::isdigit(line[firstNonSpace])) {
                            // Find end of line number
                            while (afterLineNumber < line.length() && std::isdigit(line[afterLineNumber])) {
                                afterLineNumber++;
                            }
                            // Skip space after line number
                            if (afterLineNumber < line.length() && line[afterLineNumber] == ' ') {
                                afterLineNumber++;
                            }
                        }

                        // Toggle between positions
                        if (col == 0) {
                            // Currently at start - move to after line number
                            cursor.setPosition(cursor.getLine(), afterLineNumber, buffer);
                        } else if (col == afterLineNumber) {
                            // Currently after line number - move to start
                            cursor.setPosition(cursor.getLine(), 0, buffer);
                        } else {
                            // Somewhere else - move to after line number
                            cursor.setPosition(cursor.getLine(), afterLineNumber, buffer);
                        }
                    }
                }
                cursor.clearSelection();
                break;

            case EditorAction::MOVE_SMART_END:
                // Ctrl+Right: Move to end of line
                cursor.moveToLineEnd(buffer);
                cursor.clearSelection();
                break;

            case EditorAction::MOVE_PAGE_UP:
                cursor.movePageUp(buffer, m_pageScrollLines);
                cursor.clearSelection();
                break;

            case EditorAction::MOVE_PAGE_DOWN:
                cursor.movePageDown(buffer, m_pageScrollLines);
                cursor.clearSelection();
                break;

            case EditorAction::MOVE_UP_FAST:
                // Move up 2 lines, stopping at top
                {
                    size_t currentLine = cursor.getLine();
                    if (currentLine >= 2) {
                        cursor.setPosition(currentLine - 2, cursor.getColumn(), buffer);
                    } else {
                        cursor.setPosition(0, cursor.getColumn(), buffer);
                    }
                }
                cursor.clearSelection();
                break;

            case EditorAction::MOVE_DOWN_FAST:
                // Move down 2 lines, stopping at bottom
                {
                    size_t currentLine = cursor.getLine();
                    size_t lastLine = buffer.getLineCount() - 1;
                    if (currentLine + 2 <= lastLine) {
                        cursor.setPosition(currentLine + 2, cursor.getColumn(), buffer);
                    } else {
                        cursor.setPosition(lastLine, cursor.getColumn(), buffer);
                    }
                }
                cursor.clearSelection();
                break;

            // Selection variants
            case EditorAction::SELECT_UP:
                if (!cursor.hasSelection()) cursor.startSelection();
                cursor.moveUp(buffer);
                cursor.extendSelection(cursor.getLine(), cursor.getColumn());
                break;

            case EditorAction::SELECT_DOWN:
                if (!cursor.hasSelection()) cursor.startSelection();
                cursor.moveDown(buffer);
                cursor.extendSelection(cursor.getLine(), cursor.getColumn());
                break;

            case EditorAction::SELECT_LEFT:
                if (!cursor.hasSelection()) cursor.startSelection();
                cursor.moveLeft(buffer);
                cursor.extendSelection(cursor.getLine(), cursor.getColumn());
                break;

            case EditorAction::SELECT_RIGHT:
                if (!cursor.hasSelection()) cursor.startSelection();
                cursor.moveRight(buffer);
                cursor.extendSelection(cursor.getLine(), cursor.getColumn());
                break;

            case EditorAction::SELECT_ALL:
                cursor.selectAll(buffer);
                break;

            case EditorAction::SELECT_LINE:
                cursor.selectLine(buffer);
                break;

            case EditorAction::CLEAR_SELECTION:
                cursor.clearSelection();
                break;

            case EditorAction::INSERT_NEWLINE:
                buffer.pushUndoState();
                if (cursor.hasSelection()) {
                    auto [start, end] = cursor.getSelection();
                    buffer.deleteRange(start.line, start.column, end.line, end.column);
                    cursor.setPosition(start.line, start.column, buffer);
                    cursor.clearSelection();
                }
                buffer.splitLine(cursor.getLine(), cursor.getColumn());
                cursor.setPosition(cursor.getLine() + 1, 0, buffer);

                // Auto-indent
                if (m_autoIndent && cursor.getLine() > 0) {
                    std::string prevLine = buffer.getLine(cursor.getLine() - 1);
                    int indent = getIndentLevel(prevLine);
                    if (indent > 0) {
                        std::string indentStr = createIndent(indent);
                        buffer.insertText(cursor.getLine(), 0, indentStr);
                        cursor.setPosition(cursor.getLine(), indentStr.length(), buffer);
                    }
                }

                // Call post-newline callback (for auto line numbering, etc.)
                if (m_postNewlineCallback) {
                    m_postNewlineCallback(cursor.getLine(), &buffer, &cursor);
                }
                break;

            case EditorAction::INSERT_TAB:
                buffer.pushUndoState();
                if (m_useSpacesForTab) {
                    std::string spaces(m_tabWidth, ' ');
                    buffer.insertText(cursor.getLine(), cursor.getColumn(), spaces);
                    cursor.setPosition(cursor.getLine(), cursor.getColumn() + m_tabWidth, buffer);
                } else {
                    buffer.insertChar(cursor.getLine(), cursor.getColumn(), U'\t');
                    cursor.setPosition(cursor.getLine(), cursor.getColumn() + 1, buffer);
                }
                break;

            case EditorAction::DELETE_CHAR_BEFORE:
                buffer.pushUndoState();
                if (cursor.hasSelection()) {
                    auto [start, end] = cursor.getSelection();
                    buffer.deleteRange(start.line, start.column, end.line, end.column);
                    cursor.setPosition(start.line, start.column, buffer);
                    cursor.clearSelection();
                } else if (cursor.getColumn() > 0) {
                    buffer.deleteChar(cursor.getLine(), cursor.getColumn() - 1);
                    cursor.setPosition(cursor.getLine(), cursor.getColumn() - 1, buffer);
                } else if (cursor.getLine() > 0) {
                    size_t prevLineLen = buffer.getLine(cursor.getLine() - 1).length();
                    buffer.joinLine(cursor.getLine() - 1);
                    cursor.setPosition(cursor.getLine() - 1, prevLineLen, buffer);
                }
                break;

            case EditorAction::DELETE_CHAR_AFTER:
                buffer.pushUndoState();
                if (cursor.hasSelection()) {
                    auto [start, end] = cursor.getSelection();
                    buffer.deleteRange(start.line, start.column, end.line, end.column);
                    cursor.setPosition(start.line, start.column, buffer);
                    cursor.clearSelection();
                } else {
                    std::string line = buffer.getLine(cursor.getLine());
                    if (cursor.getColumn() < line.length()) {
                        buffer.deleteChar(cursor.getLine(), cursor.getColumn());
                    } else if (cursor.getLine() < buffer.getLineCount() - 1) {
                        buffer.joinLine(cursor.getLine());
                    }
                }
                break;

            case EditorAction::UNDO:
                if (buffer.undo()) {
                    cursor.clampToBuffer(buffer);
                }
                break;

            case EditorAction::REDO:
                if (buffer.redo()) {
                    cursor.clampToBuffer(buffer);
                }
                break;

            case EditorAction::DELETE_LINE:
                buffer.pushUndoState();
                {
                    size_t currentLine = cursor.getLine();
                    buffer.deleteLine(currentLine);
                    // Clamp cursor to buffer after deletion
                    cursor.clampToBuffer(buffer);
                }
                break;

            case EditorAction::DUPLICATE_LINE:
                buffer.pushUndoState();
                {
                    size_t currentLine = cursor.getLine();
                    std::string lineContent = buffer.getLine(currentLine);
                    // Insert duplicate after current line
                    buffer.insertLine(currentLine + 1, lineContent);
                    // Move cursor to the duplicated line
                    cursor.setPosition(currentLine + 1, cursor.getColumn(), buffer);
                }
                break;

            // High-level actions passed to callback
            case EditorAction::CUT:
            case EditorAction::COPY:
            case EditorAction::PASTE:
            case EditorAction::NEW_FILE:
            case EditorAction::OPEN_FILE:
            case EditorAction::SAVE_FILE:
            case EditorAction::SAVE_FILE_AS:
            case EditorAction::CLOSE_FILE:
            case EditorAction::RUN_SCRIPT:
            case EditorAction::STOP_SCRIPT:
            case EditorAction::FIND:
            case EditorAction::EDIT_MODE:
            case EditorAction::RUN_MODE:
            case EditorAction::SPLIT_MODE:
                if (m_actionCallback) {
                    m_actionCallback(action);
                }
                break;

            default:
                break;
        }

        return action;
    }

    // Handle character input
    if (m_inputManager->hasCharacters()) {
        char32_t ch = m_inputManager->getNextCharacter();
        if (ch != 0) {
            // Map macOS special characters (Unicode private use area) to actions
            // These are sent for arrow keys, delete, etc. when the keycode isn't caught
            // Values from actual log: Up=63232, Down=63233, Left=63234, Right=63235
            switch (ch) {
                case 63232: // Up arrow - 0xF700
                    cursor.moveUp(buffer);
                    cursor.clearSelection();
                    return EditorAction::MOVE_UP;

                case 63233: // Down arrow - 0xF701
                    cursor.moveDown(buffer);
                    cursor.clearSelection();
                    return EditorAction::MOVE_DOWN;

                case 63234: // Left arrow - 0xF702
                    cursor.moveLeft(buffer);
                    cursor.clearSelection();
                    return EditorAction::MOVE_LEFT;

                case 63235: // Right arrow - 0xF703
                    cursor.moveRight(buffer);
                    cursor.clearSelection();
                    return EditorAction::MOVE_RIGHT;

                case 0xF728: // Delete key
                    {
                        std::string line = buffer.getLine(cursor.getLine());
                        if (cursor.getColumn() < line.length()) {
                            buffer.deleteChar(cursor.getLine(), cursor.getColumn());
                        } else if (cursor.getLine() < buffer.getLineCount() - 1) {
                            buffer.joinLine(cursor.getLine());
                        }
                    }
                    return EditorAction::DELETE_CHAR_AFTER;

                case 0xF729: // Home key
                    cursor.moveToLineStart();
                    cursor.clearSelection();
                    return EditorAction::MOVE_LINE_START;

                case 0xF72B: // End key
                    cursor.moveToLineEnd(buffer);
                    cursor.clearSelection();
                    return EditorAction::MOVE_LINE_END;

                case 0xF72C: // Page Up
                    cursor.movePageUp(buffer, m_pageScrollLines);
                    cursor.clearSelection();
                    return EditorAction::MOVE_PAGE_UP;

                case 0xF72D: // Page Down
                    cursor.movePageDown(buffer, m_pageScrollLines);
                    cursor.clearSelection();
                    return EditorAction::MOVE_PAGE_DOWN;

                // If not a special key, handle as normal character insertion
                default:
                    handleCharacterInsertion(buffer, cursor, ch);
                    return EditorAction::NONE;
            }
        }
    }

    return EditorAction::NONE;
}

char32_t InputHandler::getLastCharacter() {
    if (m_inputManager && m_inputManager->hasCharacters()) {
        return m_inputManager->peekNextCharacter();
    }
    return 0;
}

bool InputHandler::hasCharacters() const {
    return m_inputManager && m_inputManager->hasCharacters();
}

// =============================================================================
// Key Bindings
// =============================================================================

void InputHandler::registerBinding(const KeyBinding& binding) {
    m_bindings[binding] = binding.action;
}

void InputHandler::registerBinding(KeyCode key, bool shift, bool ctrl, bool alt, bool cmd, EditorAction action) {
    KeyBinding binding(key, shift, ctrl, alt, cmd, action);
    m_bindings[binding] = action;
}

void InputHandler::removeBinding(KeyCode key, bool shift, bool ctrl, bool alt, bool cmd) {
    KeyBinding binding(key, shift, ctrl, alt, cmd, EditorAction::NONE);
    m_bindings.erase(binding);
}

void InputHandler::clearBindings() {
    m_bindings.clear();
}

void InputHandler::loadDefaultBindings() {
    clearBindings();

    // Movement (arrows)
    registerBinding(KeyCode::Up,    false, false, false, false, EditorAction::MOVE_UP);
    registerBinding(KeyCode::Down,  false, false, false, false, EditorAction::MOVE_DOWN);
    registerBinding(KeyCode::Left,  false, false, false, false, EditorAction::MOVE_LEFT);
    registerBinding(KeyCode::Right, false, false, false, false, EditorAction::MOVE_RIGHT);

    // Movement with Cmd
    registerBinding(KeyCode::Left,  false, false, false, true,  EditorAction::MOVE_LINE_START);
    registerBinding(KeyCode::Right, false, false, false, true,  EditorAction::MOVE_LINE_END);
    registerBinding(KeyCode::Up,    false, false, false, true,  EditorAction::MOVE_DOCUMENT_START);
    registerBinding(KeyCode::Down,  false, false, false, true,  EditorAction::MOVE_DOCUMENT_END);

    // Movement with Alt (space-based for left/right, fast for up/down)
    registerBinding(KeyCode::Left,  false, false, true,  false, EditorAction::MOVE_TO_SPACE_LEFT);
    registerBinding(KeyCode::Right, false, false, true,  false, EditorAction::MOVE_TO_SPACE_RIGHT);
    registerBinding(KeyCode::Up,    false, false, true,  false, EditorAction::MOVE_UP_FAST);
    registerBinding(KeyCode::Down,  false, false, true,  false, EditorAction::MOVE_DOWN_FAST);

    // Movement with Ctrl (smart BASIC navigation)
    registerBinding(KeyCode::Left,  false, true,  false, false, EditorAction::MOVE_SMART_HOME);
    registerBinding(KeyCode::Right, false, true,  false, false, EditorAction::MOVE_SMART_END);

    // Page navigation
    registerBinding(KeyCode::PageUp,   false, false, false, false, EditorAction::MOVE_PAGE_UP);
    registerBinding(KeyCode::PageDown, false, false, false, false, EditorAction::MOVE_PAGE_DOWN);

    // Selection (Shift + movement)
    registerBinding(KeyCode::Up,    true,  false, false, false, EditorAction::SELECT_UP);
    registerBinding(KeyCode::Down,  true,  false, false, false, EditorAction::SELECT_DOWN);
    registerBinding(KeyCode::Left,  true,  false, false, false, EditorAction::SELECT_LEFT);
    registerBinding(KeyCode::Right, true,  false, false, false, EditorAction::SELECT_RIGHT);

    registerBinding(KeyCode::Left,  true,  false, false, true,  EditorAction::SELECT_LINE_START);
    registerBinding(KeyCode::Right, true,  false, false, true,  EditorAction::SELECT_LINE_END);
    registerBinding(KeyCode::Up,    true,  false, false, true,  EditorAction::SELECT_DOCUMENT_START);
    registerBinding(KeyCode::Down,  true,  false, false, true,  EditorAction::SELECT_DOCUMENT_END);

    // Page selection (Shift + Page Up/Down)
    registerBinding(KeyCode::PageUp,   true,  false, false, false, EditorAction::SELECT_PAGE_UP);
    registerBinding(KeyCode::PageDown, true,  false, false, false, EditorAction::SELECT_PAGE_DOWN);

    // Editing
    registerBinding(KeyCode::Enter,     false, false, false, false, EditorAction::INSERT_NEWLINE);
    registerBinding(KeyCode::Tab,       false, false, false, false, EditorAction::INSERT_TAB);
    registerBinding(KeyCode::Backspace, false, false, false, false, EditorAction::DELETE_CHAR_BEFORE);
    // Delete key would be here (need to add to KeyCode enum)

    // Line operations (Ctrl+K = delete line, Ctrl+D = duplicate line)
    registerBinding(KeyCode::K, false, true, false, false, EditorAction::DELETE_LINE);
    registerBinding(KeyCode::D, false, true, false, false, EditorAction::DUPLICATE_LINE);

    // Clipboard (Cmd+X/C/V)
    registerBinding(KeyCode::X, false, false, false, true, EditorAction::CUT);
    registerBinding(KeyCode::C, false, false, false, true, EditorAction::COPY);
    registerBinding(KeyCode::V, false, false, false, true, EditorAction::PASTE);

    // Undo/Redo
    registerBinding(KeyCode::Z, false, false, false, true,  EditorAction::UNDO);
    registerBinding(KeyCode::Z, true,  false, false, true,  EditorAction::REDO);

    // Select All
    registerBinding(KeyCode::A, false, false, false, true, EditorAction::SELECT_ALL);

    // File operations
    registerBinding(KeyCode::N, false, false, false, true, EditorAction::NEW_FILE);
    registerBinding(KeyCode::O, false, false, false, true, EditorAction::OPEN_FILE);
    registerBinding(KeyCode::S, false, false, false, true, EditorAction::SAVE_FILE);
    registerBinding(KeyCode::S, true,  false, false, true, EditorAction::SAVE_FILE_AS);
    registerBinding(KeyCode::W, false, false, false, true, EditorAction::CLOSE_FILE);

    // Script operations
    registerBinding(KeyCode::R, false, false, false, true, EditorAction::RUN_SCRIPT);
    // Cmd+. for stop (period key)

    // Find
    registerBinding(KeyCode::F, false, false, false, true, EditorAction::FIND);
    registerBinding(KeyCode::G, false, false, false, true, EditorAction::FIND_NEXT);

    // Mode switching
    registerBinding(KeyCode::Num1, false, false, false, true, EditorAction::EDIT_MODE);
    registerBinding(KeyCode::Num2, false, false, false, true, EditorAction::RUN_MODE);
    registerBinding(KeyCode::Num3, false, false, false, true, EditorAction::SPLIT_MODE);
}

void InputHandler::loadEmacsBindings() {
    // Future: Ctrl-based bindings like Emacs
    // Ctrl+F = forward, Ctrl+B = back, Ctrl+N = next, Ctrl+P = prev, etc.
}

void InputHandler::loadViBindings() {
    // Future: Modal editing like Vi/Vim
    // Would need to track mode (normal/insert/visual)
}

// =============================================================================
// Mouse Input
// =============================================================================

bool InputHandler::handleMouseClick(int gridX, int gridY, int button) {
    if (button == 0) {  // Left click
        m_mouseSelecting = false;
        return true;
    }
    return false;
}

bool InputHandler::handleMouseDrag(int gridX, int gridY) {
    m_mouseSelecting = true;
    return true;
}

bool InputHandler::handleMouseRelease() {
    m_mouseSelecting = false;
    return true;
}

int InputHandler::handleMouseWheel(float deltaX, float deltaY) {
    // Convert wheel delta to scroll lines
    // macOS typically uses small deltas, so scale appropriately
    int scrollLines = static_cast<int>(deltaY / 10.0f);
    return scrollLines;
}

// =============================================================================
// State
// =============================================================================

void InputHandler::reset() {
    if (m_inputManager) {
        m_inputManager->clearCharacterBuffer();
    }
    m_mouseSelecting = false;
    m_lastAction = EditorAction::NONE;
    m_processedKeys.fill(false);
    m_lastKeyCode = KeyCode::Unknown;
}

// =============================================================================
// Internal Helpers
// =============================================================================

bool InputHandler::isKeyPressed(KeyCode key, bool shift, bool ctrl, bool alt, bool cmd) const {
    if (!m_inputManager) {
        return false;
    }

    bool keyDown = m_inputManager->isKeyJustPressed(key);
    bool shiftDown = shift ? m_inputManager->isShiftPressed() : !m_inputManager->isShiftPressed();
    bool ctrlDown = ctrl ? m_inputManager->isControlPressed() : !m_inputManager->isControlPressed();
    bool altDown = alt ? m_inputManager->isAltPressed() : !m_inputManager->isAltPressed();
    bool cmdDown = cmd ? m_inputManager->isCommandPressed() : !m_inputManager->isCommandPressed();

    return keyDown && shiftDown && ctrlDown && altDown && cmdDown;
}

EditorAction InputHandler::findAction() {
    if (!m_inputManager) {
        return EditorAction::NONE;
    }

    // KEY REPEAT DISABLED: Only respond to fresh key presses
    // Use edge detection: key is pressed now but wasn't processed yet
    // This is more responsive than isKeyJustPressed() which can miss events

    // Check each registered binding
    for (const auto& [binding, action] : m_bindings) {
        int keyIdx = static_cast<int>(binding.key);

        // Check if key is currently pressed
        bool keyPressed = m_inputManager->isKeyPressed(binding.key);

        // Only trigger if pressed AND not yet processed (edge detection)
        if (keyPressed && !m_processedKeys[keyIdx]) {
            // Check modifiers match
            bool shiftMatch = binding.shift == m_inputManager->isShiftPressed();
            bool ctrlMatch = binding.ctrl == m_inputManager->isControlPressed();
            bool altMatch = binding.alt == m_inputManager->isAltPressed();
            bool cmdMatch = binding.cmd == m_inputManager->isCommandPressed();

            if (shiftMatch && ctrlMatch && altMatch && cmdMatch) {
                // Mark this key as processed
                m_processedKeys[keyIdx] = true;
                return action;
            }
        }
        // If key is released, clear its processed flag
        else if (!keyPressed && m_processedKeys[keyIdx]) {
            m_processedKeys[keyIdx] = false;
        }
    }

    return EditorAction::NONE;
}

void InputHandler::handleCharacterInsertion(TextBuffer& buffer, Cursor& cursor, char32_t ch) {
    // Skip control characters (except tab and newline, which are handled by actions)
    // Also skip DEL (127) which is the backspace character
    if ((ch < 32 && ch != 9 && ch != 10 && ch != 13) || ch == 127) {
        return;
    }

    buffer.pushUndoState();

    // Delete selection if active
    if (cursor.hasSelection()) {
        auto [start, end] = cursor.getSelection();
        buffer.deleteRange(start.line, start.column, end.line, end.column);
        cursor.setPosition(start.line, start.column, buffer);
        cursor.clearSelection();
    }

    // Insert character
    buffer.insertChar(cursor.getLine(), cursor.getColumn(), ch);
    cursor.setPosition(cursor.getLine(), cursor.getColumn() + 1, buffer);
}

int InputHandler::getIndentLevel(const std::string& line) const {
    int level = 0;
    for (char c : line) {
        if (c == ' ') {
            level++;
        } else if (c == '\t') {
            level += m_tabWidth;
        } else {
            break;
        }
    }
    return level;
}

std::string InputHandler::createIndent(int level) const {
    if (m_useSpacesForTab) {
        return std::string(level, ' ');
    } else {
        int tabs = level / m_tabWidth;
        int spaces = level % m_tabWidth;
        return std::string(tabs, '\t') + std::string(spaces, ' ');
    }
}

} // namespace SuperTerminal
