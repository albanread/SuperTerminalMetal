//
//  EditorAPI.mm
//  SuperTerminal Framework - Editor C API
//
//  C API wrapper for TextEditor C++ class
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#import <Foundation/Foundation.h>
#include "TextEditor.h"
#include <string>

// Global editor instance pointer (set by the application/runner)
static SuperTerminal::TextEditor* g_editorInstance = nullptr;
static bool g_editorActive = false;

extern "C" {

// =============================================================================
// Editor Instance Management
// =============================================================================

void editor_set_instance(void* editor) {
    g_editorInstance = static_cast<SuperTerminal::TextEditor*>(editor);
    NSLog(@"[EditorAPI] editor_set_instance: %p", g_editorInstance);
}

void* editor_get_instance() {
    return g_editorInstance;
}

void editor_set_active(bool active) {
    NSLog(@"[EditorAPI] editor_set_active: %d (was %d)", active, g_editorActive);
    g_editorActive = active;
}

bool editor_is_active() {
    return g_editorActive;
}

// =============================================================================
// Editor Lifecycle
// =============================================================================

bool editor_toggle_impl() {
    // Toggle active state
    g_editorActive = !g_editorActive;
    return g_editorActive;
}

void editor_activate() {
    g_editorActive = true;
}

void editor_deactivate() {
    g_editorActive = false;
}

// =============================================================================
// Editor Mouse Input
// =============================================================================

void editor_mouse_click(float x, float y) {
    if (g_editorInstance && g_editorActive) {
        g_editorInstance->handleMouseClick((int)x, (int)y, 0);
    }
}

void editor_mouse_drag(float x, float y) {
    if (g_editorInstance && g_editorActive) {
        g_editorInstance->handleMouseDrag((int)x, (int)y);
    }
}

void editor_mouse_up(float x, float y) {
    if (g_editorInstance && g_editorActive) {
        g_editorInstance->handleMouseRelease();
    }
}

void editor_scroll_vertical(int lines) {
    NSLog(@"[EditorAPI] editor_scroll_vertical called: lines=%d, instance=%p, active=%d",
          lines, g_editorInstance, g_editorActive);

    if (g_editorInstance && g_editorActive) {
        NSLog(@"[EditorAPI] Calling TextEditor::scrollBy(%d)", lines);
        g_editorInstance->scrollBy(lines);
        NSLog(@"[EditorAPI] After scrollBy, scrollLine=%d", g_editorInstance->getScrollLine());
    } else {
        NSLog(@"[EditorAPI] Scroll SKIPPED - instance or active check failed");
    }
}

void editor_scroll_horizontal(int columns) {
    // Not implemented yet - TextEditor doesn't support horizontal scroll
    // Future: implement horizontal scrolling for long lines
}

void editor_mouse_wheel(float deltaX, float deltaY) {
    NSLog(@"[EditorAPI] editor_mouse_wheel called: deltaX=%.2f, deltaY=%.2f, instance=%p, active=%d",
          deltaX, deltaY, g_editorInstance, g_editorActive);

    if (g_editorInstance && g_editorActive) {
        NSLog(@"[EditorAPI] Calling TextEditor::handleMouseWheel(%.2f, %.2f)", deltaX, deltaY);
        g_editorInstance->handleMouseWheel(deltaX, deltaY);
    } else {
        NSLog(@"[EditorAPI] Mouse wheel SKIPPED - instance or active check failed");
    }
}

// =============================================================================
// Editor Content Management
// =============================================================================

void editor_clear() {
    if (g_editorInstance) {
        g_editorInstance->clear();
    }
}

void editor_new_file() {
    if (g_editorInstance) {
        g_editorInstance->clear();
        g_editorInstance->setFilename("untitled.bas");
        g_editorInstance->markClean();
    }
}

char* editor_get_content() {
    if (!g_editorInstance) {
        return nullptr;
    }

    std::string content = g_editorInstance->getText();
    char* result = (char*)malloc(content.length() + 1);
    if (result) {
        strcpy(result, content.c_str());
    }
    return result;
}

void editor_load_content(const char* content) {
    if (g_editorInstance && content) {
        g_editorInstance->loadText(content);
        g_editorInstance->markClean();
    }
}

// =============================================================================
// Editor File Operations
// =============================================================================

bool editor_save(const char* filename) {
    if (!g_editorInstance || !filename) {
        return false;
    }

    // Get content from editor
    std::string content = g_editorInstance->getText();

    // Save to file
    NSString* path = [NSString stringWithUTF8String:filename];
    NSString* contentStr = [NSString stringWithUTF8String:content.c_str()];
    NSError* error = nil;

    BOOL success = [contentStr writeToFile:path
                                atomically:YES
                                  encoding:NSUTF8StringEncoding
                                     error:&error];

    if (success) {
        g_editorInstance->setFilename(filename);
        g_editorInstance->markClean();
        return true;
    } else {
        if (error) {
            NSLog(@"[EditorAPI] Failed to save file: %@", error.localizedDescription);
        }
        return false;
    }
}

bool editor_load(const char* filename) {
    if (!g_editorInstance || !filename) {
        return false;
    }

    NSString* path = [NSString stringWithUTF8String:filename];
    NSError* error = nil;
    NSString* content = [NSString stringWithContentsOfFile:path
                                                   encoding:NSUTF8StringEncoding
                                                      error:&error];

    if (content) {
        g_editorInstance->loadText([content UTF8String]);
        g_editorInstance->setFilename(filename);
        g_editorInstance->markClean();
        return true;
    } else {
        if (error) {
            NSLog(@"[EditorAPI] Failed to load file: %@", error.localizedDescription);
        }
        return false;
    }
}

bool editor_load_file(const char* filepath) {
    return editor_load(filepath);
}

const char* editor_get_current_filename() {
    if (!g_editorInstance) {
        return "";
    }

    static std::string filename;
    filename = g_editorInstance->getFilename();
    return filename.c_str();
}

// =============================================================================
// Editor State
// =============================================================================

bool editor_is_modified() {
    if (!g_editorInstance) {
        return false;
    }
    return g_editorInstance->isDirty();
}

bool editor_is_empty() {
    if (!g_editorInstance) {
        return true;
    }
    return g_editorInstance->isEmpty();
}

// =============================================================================
// Editor Cursor
// =============================================================================

void editor_get_cursor_position(int* line, int* column) {
    if (!g_editorInstance || !line || !column) {
        return;
    }

    *line = (int)g_editorInstance->getCursorLine();
    *column = (int)g_editorInstance->getCursorColumn();
}

void editor_set_cursor_position(int line, int column) {
    if (g_editorInstance) {
        g_editorInstance->setCursorPosition(line, column);
    }
}

// =============================================================================
// Editor Editing Operations
// =============================================================================

void editor_insert_text(const char* text) {
    if (g_editorInstance && text) {
        g_editorInstance->insertText(text);
    }
}

void editor_delete_selection() {
    if (g_editorInstance) {
        g_editorInstance->deleteSelection();
    }
}

bool editor_undo() {
    if (!g_editorInstance) {
        return false;
    }
    return g_editorInstance->undo();
}

bool editor_redo() {
    if (!g_editorInstance) {
        return false;
    }
    return g_editorInstance->redo();
}

bool editor_can_undo() {
    if (!g_editorInstance) {
        return false;
    }
    return g_editorInstance->canUndo();
}

bool editor_can_redo() {
    if (!g_editorInstance) {
        return false;
    }
    return g_editorInstance->canRedo();
}

// =============================================================================
// Editor Clipboard
// =============================================================================

void editor_cut() {
    if (g_editorInstance) {
        g_editorInstance->cut();
    }
}

void editor_copy() {
    if (g_editorInstance) {
        g_editorInstance->copy();
    }
}

void editor_paste() {
    if (g_editorInstance) {
        g_editorInstance->paste();
    }
}

// =============================================================================
// Editor Status
// =============================================================================

void editor_set_status(const char* message, int duration) {
    // Note: Status bar is handled by DisplayManager
    // This is a placeholder for future implementation
    if (message) {
        NSLog(@"[EditorAPI] Status: %s (duration: %d)", message, duration);
    }
}

// =============================================================================
// Editor Configuration
// =============================================================================

void editor_set_tab_width(int width) {
    if (g_editorInstance) {
        g_editorInstance->setTabWidth(width);
    }
}

int editor_get_tab_width() {
    if (!g_editorInstance) {
        return 4;
    }
    return g_editorInstance->getTabWidth();
}

void editor_set_language(const char* language) {
    if (g_editorInstance && language) {
        g_editorInstance->setLanguage(language);
    }
}

const char* editor_get_language() {
    if (!g_editorInstance) {
        return "";
    }

    static std::string language;
    language = g_editorInstance->getLanguage();
    return language.c_str();
}

// =============================================================================
// Editor Viewport
// =============================================================================

int editor_get_scroll_line() {
    if (!g_editorInstance) {
        return 0;
    }
    return g_editorInstance->getScrollLine();
}

void editor_set_scroll_line(int line) {
    if (g_editorInstance) {
        g_editorInstance->setScrollLine(line);
    }
}

void editor_scroll_to_cursor() {
    if (g_editorInstance) {
        g_editorInstance->scrollToCursor();
    }
}

int editor_get_viewport_height() {
    if (!g_editorInstance) {
        return 0;
    }
    return g_editorInstance->getViewportHeight();
}

// =============================================================================
// Editor Statistics
// =============================================================================

int editor_get_line_count() {
    if (!g_editorInstance) {
        return 0;
    }
    return (int)g_editorInstance->getLineCount();
}

int editor_get_character_count() {
    if (!g_editorInstance) {
        return 0;
    }
    return (int)g_editorInstance->getCharacterCount();
}

// =============================================================================
// Editor Shutdown
// =============================================================================

void editor_shutdown() {
    // Note: Actual shutdown is handled by the owning BaseRunner
    // This just clears our reference
    g_editorInstance = nullptr;
    g_editorActive = false;
}

} // extern "C"
