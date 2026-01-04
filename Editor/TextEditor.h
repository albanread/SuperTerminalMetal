//
//  TextEditor.h
//  SuperTerminal Framework - Text Editor
//
//  Main text editor class - integrates all editor components
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef TEXT_EDITOR_H
#define TEXT_EDITOR_H

#include <memory>
#include <string>
#include <functional>
#include <cstdint>

namespace SuperTerminal {

// Forward declarations
class TextBuffer;
class Cursor;
class EditorRenderer;
class InputHandler;
class TextGrid;
class InputManager;
class Document;
enum class EditorAction;
enum class KeyCode;

// =============================================================================
// TextEditor - Complete text editor with all features
// =============================================================================
//
// Integrates:
// - TextBuffer (text storage, undo/redo)
// - Cursor (position, selection, mouse)
// - EditorRenderer (render to TextGrid with yellow cursor)
// - InputHandler (keyboard/mouse → actions)
//
// Provides:
// - Simple API for embedding in applications
// - Update/render loop integration
// - Clipboard operations (via callback)
// - Syntax highlighting (via callback)
// - Dirty state tracking
// - Auto-save support
//

class TextEditor {
public:
    // Constructor
    TextEditor(std::shared_ptr<TextGrid> textGrid,
              std::shared_ptr<InputManager> inputManager);
    
    // Destructor
    ~TextEditor();
    
    // No copy (has unique state)
    TextEditor(const TextEditor&) = delete;
    TextEditor& operator=(const TextEditor&) = delete;
    
    // =========================================================================
    // Lifecycle
    // =========================================================================
    
    /// Initialize editor (call once after construction)
    bool initialize();
    
    /// Shutdown editor (call before destruction)
    void shutdown();
    
    // =========================================================================
    // Document Integration
    // =========================================================================
    
    /// Set the document to edit (makes TextEditor a view of Document)
    /// @param document Document to edit (can be nullptr to detach)
    void setDocument(std::shared_ptr<Document> document);
    
    /// Get current document
    /// @return Current document or nullptr if none set
    std::shared_ptr<Document> getDocument() const { return m_document; }
    
    /// Check if editor has a document attached
    bool hasDocument() const { return m_document != nullptr; }
    
    // =========================================================================
    // Update and Render
    // =========================================================================
    
    /// Update editor state (cursor blink, auto-save, etc.)
    /// @param deltaTime Time since last update (seconds)
    void update(double deltaTime);
    
    /// Render editor to TextGrid
    void render();
    
    /// Update TextGrid reference (for window resize)
    /// @param textGrid New text grid to render to
    void setTextGrid(std::shared_ptr<TextGrid> textGrid);
    
    /// Process input (keyboard and mouse)
    /// Call this each frame to handle user input
    void processInput();
    
    // =========================================================================
    // Content Management
    // =========================================================================
    
    /// Load text into editor
    /// @param text Text content (UTF-8)
    void loadText(const std::string& text);
    
    /// Get text from editor
    /// @return Text content (UTF-8)
    std::string getText() const;
    
    /// Clear editor
    void clear();
    
    /// Check if editor is empty
    bool isEmpty() const;
    
    // =========================================================================
    // File State
    // =========================================================================
    
    /// Set filename (for display)
    void setFilename(const std::string& filename) { m_filename = filename; }
    
    /// Get filename
    std::string getFilename() const { return m_filename; }
    
    /// Set language (for display and syntax highlighting)
    void setLanguage(const std::string& language) { m_language = language; }
    
    /// Get language
    std::string getLanguage() const { return m_language; }
    
    /// Check if content has been modified
    bool isDirty() const;
    
    /// Mark content as clean (e.g., after save)
    void markClean();
    
    /// Mark content as dirty (e.g., after edit)
    void markDirty();
    
    // =========================================================================
    // Cursor and Selection
    // =========================================================================
    
    /// Get cursor line (0-based)
    size_t getCursorLine() const;
    
    /// Get cursor column (0-based)
    size_t getCursorColumn() const;
    
    /// Set cursor position
    void setCursorPosition(size_t line, size_t column);
    
    /// Check if there is a selection
    bool hasSelection() const;
    
    /// Get selected text
    std::string getSelectedText() const;
    
    /// Clear selection
    void clearSelection();
    
    // =========================================================================
    // Editing Operations
    // =========================================================================
    
    /// Insert text at cursor
    void insertText(const std::string& text);
    
    /// Delete selection (if any)
    void deleteSelection();
    
    /// Undo last change
    bool undo();
    
    /// Redo last undone change
    bool redo();
    
    /// Check if undo is available
    bool canUndo() const;
    
    /// Check if redo is available
    bool canRedo() const;
    
    // =========================================================================
    // Clipboard Operations (via callbacks)
    // =========================================================================
    
    /// Clipboard callback: called when clipboard operation is needed
    /// @param operation "cut", "copy", or "paste"
    /// @param text For "cut"/"copy": text to put on clipboard
    ///             For "paste": text from clipboard (set by callback)
    /// @return For "paste": text to insert (empty if cancelled)
    using ClipboardCallback = std::function<std::string(const std::string& operation, const std::string& text)>;
    
    /// Set clipboard callback
    void setClipboardCallback(ClipboardCallback callback) { m_clipboardCallback = callback; }
    
    /// Cut selected text to clipboard
    void cut();
    
    /// Copy selected text to clipboard
    void copy();
    
    /// Paste text from clipboard
    void paste();
    
    // =========================================================================
    // Syntax Highlighting (via callback)
    // =========================================================================
    
    /// Syntax highlighter callback: takes line text, returns color per character
    using SyntaxHighlighter = std::function<std::vector<uint32_t>(const std::string& line, size_t lineNumber)>;
    
    /// Set syntax highlighter callback
    void setSyntaxHighlighter(SyntaxHighlighter highlighter);
    
    /// Clear syntax highlighter
    void clearSyntaxHighlighter();
    
    // =========================================================================
    // Post-Newline Callback (for auto line numbering, etc.)
    // =========================================================================
    
    /// Post-newline callback: called after a newline is inserted and auto-indent is applied
    /// @param lineNumber The line number where the cursor is now (the new line)
    /// @param buffer Pointer to the text buffer (for inserting line numbers, etc.)
    /// @param cursor Pointer to the cursor (for adjusting position after insertion)
    using PostNewlineCallback = std::function<void(size_t lineNumber, TextBuffer* buffer, Cursor* cursor)>;
    
    /// Set post-newline callback
    void setPostNewlineCallback(PostNewlineCallback callback);
    
    /// Clear post-newline callback
    void clearPostNewlineCallback();
    
    // =========================================================================
    // Action Callbacks (for high-level operations)
    // =========================================================================
    
    /// Action callback: called when high-level action is triggered
    /// (e.g., save file, run script, open file browser)
    using ActionCallback = std::function<void(EditorAction action)>;
    
    /// Set action callback
    void setActionCallback(ActionCallback callback);
    
    // =========================================================================
    // Configuration
    // =========================================================================
    
    /// Set tab width (number of spaces)
    void setTabWidth(int width);
    
    /// Get tab width
    int getTabWidth() const;
    
    /// Set whether to use spaces for tab
    void setUseSpacesForTab(bool useSpaces);
    
    /// Get whether to use spaces for tab
    bool getUseSpacesForTab() const;
    
    /// Set auto-indent
    void setAutoIndent(bool autoIndent);
    
    /// Get auto-indent
    bool getAutoIndent() const;
    
    /// Set line numbers visible
    void setLineNumbersVisible(bool visible);
    
    /// Get line numbers visible
    bool areLineNumbersVisible() const;
    
    /// Set cursor blink rate (seconds)
    void setCursorBlinkRate(double rate);
    
    /// Get cursor blink rate
    double getCursorBlinkRate() const;
    
    // =========================================================================
    // Viewport and Scrolling
    // =========================================================================
    
    /// Get current scroll position (top visible line)
    int getScrollLine() const { return m_scrollLine; }
    
    /// Set scroll position
    void setScrollLine(int line);
    
    /// Scroll to make cursor visible
    void scrollToCursor();
    
    /// Scroll by lines (negative = up, positive = down)
    void scrollBy(int lines);
    
    /// Get viewport height (visible lines)
    int getViewportHeight() const;
    
    // =========================================================================
    // Mouse Input
    // =========================================================================
    
    /// Handle mouse click at grid coordinates
    /// @param gridX Grid column
    /// @param gridY Grid row
    /// @param button Mouse button (0=left, 1=right, 2=middle)
    void handleMouseClick(int gridX, int gridY, int button);
    
    /// Handle mouse drag
    /// @param gridX Grid column
    /// @param gridY Grid row
    void handleMouseDrag(int gridX, int gridY);
    
    /// Handle mouse release
    void handleMouseRelease();
    
    /// Handle mouse wheel
    /// @param deltaX Horizontal scroll delta
    /// @param deltaY Vertical scroll delta
    void handleMouseWheel(float deltaX, float deltaY);
    
    // =========================================================================
    // Statistics
    // =========================================================================
    
    /// Get total line count
    size_t getLineCount() const;
    
    /// Get total character count
    size_t getCharacterCount() const;
    
    /// Get memory usage (bytes)
    size_t getMemoryUsage() const;
    
    // =========================================================================
    // Status Information (for status bars)
    // =========================================================================
    
    /// Get status string (for top status bar)
    std::string getStatusString() const;
    
    /// Get command string (for bottom status bar)
    std::string getCommandString() const;
    
    // =========================================================================
    // Internal Component Access (for advanced operations)
    // =========================================================================
    
    /// Get text buffer (for search operations)
    TextBuffer* getTextBuffer() { return m_buffer.get(); }
    const TextBuffer* getTextBuffer() const { return m_buffer.get(); }
    
    /// Get cursor (for search operations)
    Cursor* getCursor() { return m_cursor.get(); }
    const Cursor* getCursor() const { return m_cursor.get(); }
    
    // =========================================================================
    // Auto-save
    // =========================================================================
    
    /// Enable auto-save
    /// @param intervalSeconds Seconds between auto-saves
    void enableAutoSave(double intervalSeconds);
    
    /// Disable auto-save
    void disableAutoSave();
    
    /// Check if auto-save is enabled
    bool isAutoSaveEnabled() const { return m_autoSaveEnabled; }
    
    /// Trigger auto-save manually (calls action callback with SAVE_FILE)
    void triggerAutoSave();
    
    /// Try to compose a box drawing character from selected text
    /// @param selected Selected text (2 chars or 3 digits)
    /// @return Composed character code (128-255) or 0 if invalid
    uint32_t tryComposeFromSelection(const std::string& selected);

private:
    // =========================================================================
    // Member Variables
    // =========================================================================
    
    // Components
    std::unique_ptr<TextBuffer> m_buffer;
    std::unique_ptr<Cursor> m_cursor;
    std::unique_ptr<EditorRenderer> m_renderer;
    std::unique_ptr<InputHandler> m_inputHandler;
    
    std::shared_ptr<TextGrid> m_textGrid;
    std::shared_ptr<InputManager> m_inputManager;
    
    // Document (Phase 2: Single Source of Truth)
    std::shared_ptr<Document> m_document;
    
    // State
    std::string m_filename;
    std::string m_language;
    int m_scrollLine;
    bool m_initialized;
    
    // Auto-save
    bool m_autoSaveEnabled;
    double m_autoSaveInterval;
    double m_timeSinceLastSave;
    
    // Callbacks
    ClipboardCallback m_clipboardCallback;
    ActionCallback m_actionCallback;
    PostNewlineCallback m_postNewlineCallback;
    
    // =========================================================================
    // Internal Helpers
    // =========================================================================
    
    /// Update cursor blink animation
    void updateCursorBlink(double deltaTime);
    
    /// Update auto-save timer
    void updateAutoSave(double deltaTime);
    
    /// Handle action from input handler
    void handleAction(EditorAction action);
    
    /// Perform clipboard operation
    void performClipboardOperation(const std::string& operation);
    
    /// Sync TextBuffer changes to Document (Phase 2 bridge)
    void syncToDocument();
};

} // namespace SuperTerminal

#endif // TEXT_EDITOR_H