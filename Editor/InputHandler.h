//
//  InputHandler.h
//  SuperTerminal Framework - Input Handler
//
//  Map keyboard and mouse input to editor actions with configurable bindings
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <cstdint>

namespace SuperTerminal {

// Forward declarations
class TextBuffer;
class Cursor;
class InputManager;
enum class KeyCode;

// =============================================================================
// EditorAction - Actions the editor can perform
// =============================================================================

enum class EditorAction {
    // Movement
    MOVE_UP,
    MOVE_DOWN,
    MOVE_LEFT,
    MOVE_RIGHT,
    MOVE_LINE_START,
    MOVE_LINE_END,
    MOVE_DOCUMENT_START,
    MOVE_DOCUMENT_END,
    MOVE_WORD_LEFT,
    MOVE_WORD_RIGHT,
    MOVE_PAGE_UP,
    MOVE_PAGE_DOWN,
    MOVE_TO_SPACE_LEFT,      // Move to previous space (Option+Left alternative)
    MOVE_TO_SPACE_RIGHT,     // Move to next space (Option+Right alternative)
    MOVE_SMART_HOME,         // Ctrl+Left: cycle between line start and after line number
    MOVE_SMART_END,          // Ctrl+Right: move to line end
    MOVE_UP_FAST,            // Option+Up: move up 2 lines
    MOVE_DOWN_FAST,          // Option+Down: move down 2 lines
    
    // Movement with selection
    SELECT_UP,
    SELECT_DOWN,
    SELECT_LEFT,
    SELECT_RIGHT,
    SELECT_LINE_START,
    SELECT_LINE_END,
    SELECT_DOCUMENT_START,
    SELECT_DOCUMENT_END,
    SELECT_WORD_LEFT,
    SELECT_WORD_RIGHT,
    SELECT_PAGE_UP,
    SELECT_PAGE_DOWN,
    
    // Selection
    SELECT_ALL,
    SELECT_LINE,
    SELECT_WORD,
    CLEAR_SELECTION,
    
    // Editing
    INSERT_NEWLINE,
    INSERT_TAB,
    DELETE_CHAR_BEFORE,  // Backspace
    DELETE_CHAR_AFTER,   // Delete
    DELETE_WORD_BEFORE,
    DELETE_WORD_AFTER,
    DELETE_LINE,
    DELETE_SELECTION,
    DUPLICATE_LINE,
    
    // Clipboard
    CUT,
    COPY,
    PASTE,
    
    // Undo/Redo
    UNDO,
    REDO,
    
    // File operations (handled by higher level)
    NEW_FILE,
    OPEN_FILE,
    SAVE_FILE,
    SAVE_FILE_AS,
    CLOSE_FILE,
    
    // Script operations
    RUN_SCRIPT,
    STOP_SCRIPT,
    CLEAR_OUTPUT,
    
    // View
    TOGGLE_LINE_NUMBERS,
    INCREASE_FONT_SIZE,
    DECREASE_FONT_SIZE,
    RESET_FONT_SIZE,
    
    // Search and Navigation
    FIND,
    FIND_NEXT,
    FIND_PREVIOUS,
    FIND_REPLACE,
    GOTO_LINE,
    
    // Mode switching
    EDIT_MODE,
    RUN_MODE,
    SPLIT_MODE,
    
    // Other
    NONE
};

// =============================================================================
// KeyBinding - Keyboard shortcut definition
// =============================================================================

struct KeyBinding {
    KeyCode key;
    bool shift;
    bool ctrl;
    bool alt;
    bool cmd;
    EditorAction action;
    
    KeyBinding()
        : key(static_cast<KeyCode>(0))
        , shift(false)
        , ctrl(false)
        , alt(false)
        , cmd(false)
        , action(EditorAction::NONE)
    {}
    
    KeyBinding(KeyCode k, bool s, bool c, bool a, bool m, EditorAction act)
        : key(k)
        , shift(s)
        , ctrl(c)
        , alt(a)
        , cmd(m)
        , action(act)
    {}
    
    // Comparison for map key
    bool operator<(const KeyBinding& other) const;
    bool operator==(const KeyBinding& other) const;
};

// =============================================================================
// InputHandler - Handle keyboard and mouse input for editor
// =============================================================================

class InputHandler {
public:
    // Constructor
    InputHandler(std::shared_ptr<InputManager> inputManager);
    
    // Destructor
    ~InputHandler();
    
    // =========================================================================
    // Input Processing
    // =========================================================================
    
    /// Process input and return action to perform
    /// @param buffer Text buffer (for context)
    /// @param cursor Cursor state (for context)
    /// @return Action to perform (NONE if no action)
    EditorAction processInput(TextBuffer& buffer, Cursor& cursor);
    
    /// Get last character typed (for text insertion)
    /// @return UTF-32 character, or 0 if none
    char32_t getLastCharacter();
    
    /// Check if there are characters in the input buffer
    bool hasCharacters() const;
    
    // =========================================================================
    // Key Bindings
    // =========================================================================
    
    /// Register a key binding
    /// @param binding Key binding to register
    void registerBinding(const KeyBinding& binding);
    
    /// Register a key binding (convenience)
    void registerBinding(KeyCode key, bool shift, bool ctrl, bool alt, bool cmd, EditorAction action);
    
    /// Remove a key binding
    /// @param key Key code
    /// @param shift Shift modifier
    /// @param ctrl Control modifier
    /// @param alt Alt/Option modifier
    /// @param cmd Command modifier
    void removeBinding(KeyCode key, bool shift, bool ctrl, bool alt, bool cmd);
    
    /// Clear all key bindings
    void clearBindings();
    
    /// Load default key bindings (macOS standard)
    void loadDefaultBindings();
    
    /// Load Emacs-style key bindings
    void loadEmacsBindings();
    
    /// Load Vi-style key bindings
    void loadViBindings();
    
    /// Get all registered key bindings
    std::map<KeyBinding, EditorAction> getBindings() const { return m_bindings; }
    
    // =========================================================================
    // Mouse Input
    // =========================================================================
    
    /// Process mouse click
    /// @param gridX Grid column
    /// @param gridY Grid row
    /// @param button Mouse button (0=left, 1=right, 2=middle)
    /// @return true if click was handled
    bool handleMouseClick(int gridX, int gridY, int button);
    
    /// Process mouse drag (for selection)
    /// @param gridX Grid column
    /// @param gridY Grid row
    /// @return true if drag was handled
    bool handleMouseDrag(int gridX, int gridY);
    
    /// Process mouse release
    /// @return true if release was handled
    bool handleMouseRelease();
    
    /// Process mouse wheel (for scrolling)
    /// @param deltaX Horizontal scroll delta
    /// @param deltaY Vertical scroll delta
    /// @return Scroll amount (negative = up, positive = down)
    int handleMouseWheel(float deltaX, float deltaY);
    
    /// Check if mouse is currently selecting
    bool isMouseSelecting() const { return m_mouseSelecting; }
    
    // =========================================================================
    // Configuration
    // =========================================================================
    
    /// Set tab width (number of spaces)
    void setTabWidth(int width) { m_tabWidth = width; }
    
    /// Get tab width
    int getTabWidth() const { return m_tabWidth; }
    
    /// Set whether to use spaces for tab
    void setUseSpacesForTab(bool useSpaces) { m_useSpacesForTab = useSpaces; }
    
    /// Get whether to use spaces for tab
    bool getUseSpacesForTab() const { return m_useSpacesForTab; }
    
    /// Set auto-indent
    void setAutoIndent(bool autoIndent) { m_autoIndent = autoIndent; }
    
    /// Get auto-indent
    bool getAutoIndent() const { return m_autoIndent; }
    
    /// Set page scroll lines
    void setPageScrollLines(int lines) { m_pageScrollLines = lines; }
    
    /// Get page scroll lines
    int getPageScrollLines() const { return m_pageScrollLines; }
    
    // =========================================================================
    // Action Callbacks
    // =========================================================================
    
    /// Action callback: called when an action needs to be handled at higher level
    using ActionCallback = std::function<void(EditorAction action)>;
    
    /// Set action callback for actions that need higher-level handling
    /// (e.g., file operations, mode switching)
    void setActionCallback(ActionCallback callback) { m_actionCallback = callback; }
    
    /// Post-newline callback: called after a newline is inserted and auto-indent is applied
    using PostNewlineCallback = std::function<void(size_t lineNumber, TextBuffer* buffer, Cursor* cursor)>;
    
    /// Set post-newline callback (for auto line numbering, etc.)
    void setPostNewlineCallback(PostNewlineCallback callback) { m_postNewlineCallback = callback; }
    
    // =========================================================================
    // State
    // =========================================================================
    
    /// Reset input state (clear buffers, selection state, etc.)
    void reset();
    
    /// Get last action performed
    EditorAction getLastAction() const { return m_lastAction; }

private:
    // =========================================================================
    // Member Variables
    // =========================================================================
    
    std::shared_ptr<InputManager> m_inputManager;
    
    // Key bindings
    std::map<KeyBinding, EditorAction> m_bindings;
    
    // Mouse state
    bool m_mouseSelecting;
    int m_mouseStartX;
    int m_mouseStartY;
    
    // Configuration
    int m_tabWidth;
    bool m_useSpacesForTab;
    bool m_autoIndent;
    int m_pageScrollLines;
    
    // State
    EditorAction m_lastAction;
    
    // Key repeat timing (kept for potential future use)
    double m_keyRepeatInitialDelay;  // Seconds before repeat starts
    double m_keyRepeatInterval;      // Seconds between repeats
    KeyCode m_lastKeyCode;           // Last key that was pressed
    double m_keyPressTime;           // Time when key was first pressed
    double m_lastRepeatTime;         // Time of last repeat trigger
    
    // Key edge detection (for responsive input without repeat)
    std::array<bool, 256> m_processedKeys;  // Track which keys have been processed
    
    // Callbacks
    ActionCallback m_actionCallback;
    PostNewlineCallback m_postNewlineCallback;
    
    // =========================================================================
    // Internal Helpers
    // =========================================================================
    
    /// Check if a key is pressed with modifiers
    bool isKeyPressed(KeyCode key, bool shift, bool ctrl, bool alt, bool cmd) const;
    
    /// Find action for current key state
    EditorAction findAction();
    
    /// Handle character insertion
    void handleCharacterInsertion(TextBuffer& buffer, Cursor& cursor, char32_t ch);
    
    /// Get indent level of line
    int getIndentLevel(const std::string& line) const;
    
    /// Create indent string
    std::string createIndent(int level) const;
};

// =============================================================================
// Helper Functions
// =============================================================================

/// Convert EditorAction to string (for debugging/display)
const char* editorActionToString(EditorAction action);

/// Convert string to EditorAction
EditorAction stringToEditorAction(const char* str);

} // namespace SuperTerminal

#endif // INPUT_HANDLER_H