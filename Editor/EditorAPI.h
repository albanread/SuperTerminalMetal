//
//  EditorAPI.h
//  SuperTerminal Framework - Editor C API
//
//  C API wrapper for TextEditor C++ class
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef EDITOR_API_H
#define EDITOR_API_H

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Editor Instance Management
// =============================================================================

/// Set the global editor instance (called by BaseRunner or application)
/// @param editor Pointer to TextEditor instance
void editor_set_instance(void* editor);

/// Get the global editor instance
/// @return Pointer to TextEditor instance or NULL
void* editor_get_instance(void);

/// Set editor active state
/// @param active True to activate, false to deactivate
void editor_set_active(bool active);

/// Check if editor is active
/// @return True if editor is active
bool editor_is_active(void);

// =============================================================================
// Editor Lifecycle
// =============================================================================

/// Toggle editor on/off
/// @return New active state (true = active, false = inactive)
bool editor_toggle_impl(void);

/// Activate editor
void editor_activate(void);

/// Deactivate editor
void editor_deactivate(void);

// =============================================================================
// Editor Mouse Input
// =============================================================================

/// Handle mouse click at editor position
/// @param x X coordinate (grid column)
/// @param y Y coordinate (grid row)
void editor_mouse_click(float x, float y);

/// Handle mouse drag
/// @param x X coordinate (grid column)
/// @param y Y coordinate (grid row)
void editor_mouse_drag(float x, float y);

/// Handle mouse button release
/// @param x X coordinate (grid column)
/// @param y Y coordinate (grid row)
void editor_mouse_up(float x, float y);

/// Scroll editor vertically by number of lines
/// @param lines Number of lines to scroll (positive = down, negative = up)
void editor_scroll_vertical(int lines);

/// Scroll editor horizontally by number of columns
/// @param columns Number of columns to scroll (positive = right, negative = left)
void editor_scroll_horizontal(int columns);

/// Handle mouse wheel event
/// @param deltaX Horizontal scroll delta
/// @param deltaY Vertical scroll delta
void editor_mouse_wheel(float deltaX, float deltaY);

// =============================================================================
// Editor Content Management
// =============================================================================

/// Clear editor content
void editor_clear(void);

/// Create new file (clear content and set filename to "untitled.bas")
void editor_new_file(void);

/// Get editor content as string
/// @return Content string (caller must free with free())
char* editor_get_content(void);

/// Load content into editor
/// @param content Text content to load
void editor_load_content(const char* content);

// =============================================================================
// Editor File Operations
// =============================================================================

/// Save editor content to file
/// @param filename Path to file
/// @return True if successful
bool editor_save(const char* filename);

/// Load file into editor
/// @param filename Path to file
/// @return True if successful
bool editor_load(const char* filename);

/// Load file into editor (alias for editor_load)
/// @param filepath Path to file
/// @return True if successful
bool editor_load_file(const char* filepath);

/// Get current filename
/// @return Filename (static string, do not free)
const char* editor_get_current_filename(void);

// =============================================================================
// Editor State
// =============================================================================

/// Check if editor content has been modified
/// @return True if modified
bool editor_is_modified(void);

/// Check if editor is empty
/// @return True if empty
bool editor_is_empty(void);

// =============================================================================
// Editor Cursor
// =============================================================================

/// Get cursor position
/// @param line Output: cursor line (0-based)
/// @param column Output: cursor column (0-based)
void editor_get_cursor_position(int* line, int* column);

/// Set cursor position
/// @param line Line number (0-based)
/// @param column Column number (0-based)
void editor_set_cursor_position(int line, int column);

// =============================================================================
// Editor Editing Operations
// =============================================================================

/// Insert text at cursor
/// @param text Text to insert
void editor_insert_text(const char* text);

/// Delete selected text
void editor_delete_selection(void);

/// Undo last change
/// @return True if successful
bool editor_undo(void);

/// Redo last undone change
/// @return True if successful
bool editor_redo(void);

/// Check if undo is available
/// @return True if undo is available
bool editor_can_undo(void);

/// Check if redo is available
/// @return True if redo is available
bool editor_can_redo(void);

// =============================================================================
// Editor Clipboard
// =============================================================================

/// Cut selected text to clipboard
void editor_cut(void);

/// Copy selected text to clipboard
void editor_copy(void);

/// Paste text from clipboard
void editor_paste(void);

// =============================================================================
// Editor Status
// =============================================================================

/// Set status message
/// @param message Status message to display
/// @param duration Duration in milliseconds (0 = indefinite)
void editor_set_status(const char* message, int duration);

// =============================================================================
// Editor Configuration
// =============================================================================

/// Set tab width (number of spaces)
/// @param width Tab width
void editor_set_tab_width(int width);

/// Get tab width
/// @return Tab width
int editor_get_tab_width(void);

/// Set editor language (for syntax highlighting)
/// @param language Language name (e.g., "basic", "lua", "javascript")
void editor_set_language(const char* language);

/// Get editor language
/// @return Language name (static string, do not free)
const char* editor_get_language(void);

// =============================================================================
// Editor Viewport
// =============================================================================

/// Get current scroll position (top visible line)
/// @return Line number at top of viewport
int editor_get_scroll_line(void);

/// Set scroll position
/// @param line Line number to show at top of viewport
void editor_set_scroll_line(int line);

/// Scroll to make cursor visible
void editor_scroll_to_cursor(void);

/// Get viewport height (number of visible lines)
/// @return Viewport height in lines
int editor_get_viewport_height(void);

// =============================================================================
// Editor Statistics
// =============================================================================

/// Get total line count
/// @return Number of lines
int editor_get_line_count(void);

/// Get total character count
/// @return Number of characters
int editor_get_character_count(void);

// =============================================================================
// Editor Shutdown
// =============================================================================

/// Shutdown editor (cleanup global references)
void editor_shutdown(void);

// =============================================================================
// Editor Execution (external dependency)
// =============================================================================

/// Execute current editor content (implemented elsewhere)
void editor_execute_current(void);

#ifdef __cplusplus
}
#endif

#endif // EDITOR_API_H
