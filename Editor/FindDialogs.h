//
//  FindDialogs.h
//  SuperTerminal Framework - Find/Replace and Go to Line Dialogs
//
//  Native macOS dialogs for text searching and navigation
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#include <string>
#include <memory>

// Forward declarations
namespace SuperTerminal {
    class TextBuffer;
    class Cursor;
}

// =============================================================================
// GotoLineDialog - Jump to specific line number
// =============================================================================

@interface GotoLineDialog : NSObject

/// Show go to line dialog modally
/// @param currentLine Current line number (1-based, for display)
/// @param maxLine Maximum line number
/// @param outLine Output: target line number (1-based)
/// @return YES if user confirmed, NO if cancelled
+ (BOOL)showDialogWithCurrentLine:(NSInteger)currentLine
                          maxLine:(NSInteger)maxLine
                          outLine:(NSInteger*)outLine;

@end

// =============================================================================
// FindDialog - Search for text
// =============================================================================

@interface FindDialog : NSObject

typedef NS_OPTIONS(NSUInteger, FindOptions) {
    FindOptionNone          = 0,
    FindOptionCaseSensitive = 1 << 0,
    FindOptionWholeWord     = 1 << 1,
    FindOptionWrapAround    = 1 << 2,
    FindOptionRegex         = 1 << 3,
    FindOptionBackward      = 1 << 4
};

/// Show find dialog modally
/// @param defaultText Default search text
/// @param outText Output: search text
/// @param outOptions Output: search options
/// @return YES if user wants to search, NO if cancelled
+ (BOOL)showDialogWithDefaultText:(NSString*)defaultText
                          outText:(NSString**)outText
                       outOptions:(FindOptions*)outOptions;

/// Show find dialog as non-modal panel (stays on screen)
/// @param defaultText Default search text
/// @param target Target for find actions
+ (void)showPanelWithDefaultText:(NSString*)defaultText
                          target:(id)target;

@end

// =============================================================================
// FindReplaceDialog - Search and replace text
// =============================================================================

@interface FindReplaceDialog : NSObject

typedef NS_ENUM(NSInteger, FindReplaceAction) {
    FindReplaceActionCancel,        // User cancelled
    FindReplaceActionFindNext,      // Find next occurrence
    FindReplaceActionReplace,       // Replace current and find next
    FindReplaceActionReplaceAll     // Replace all occurrences
};

/// Show find/replace dialog modally
/// @param defaultFindText Default search text
/// @param defaultReplaceText Default replacement text
/// @param outFindText Output: search text
/// @param outReplaceText Output: replacement text
/// @param outOptions Output: search options
/// @param outAction Output: action to perform
/// @return YES if user wants to perform action, NO if cancelled
+ (BOOL)showDialogWithDefaultFindText:(NSString*)defaultFindText
                   defaultReplaceText:(NSString*)defaultReplaceText
                          outFindText:(NSString**)outFindText
                       outReplaceText:(NSString**)outReplaceText
                           outOptions:(FindOptions*)outOptions
                            outAction:(FindReplaceAction*)outAction;

/// Show find/replace dialog as non-modal panel (stays on screen)
/// @param defaultFindText Default search text
/// @param defaultReplaceText Default replacement text
/// @param target Target for find/replace actions
+ (void)showPanelWithDefaultFindText:(NSString*)defaultFindText
                  defaultReplaceText:(NSString*)defaultReplaceText
                              target:(id)target;

@end

// =============================================================================
// Find/Replace Result - Position of found text
// =============================================================================

struct FindResult {
    bool found;
    size_t line;
    size_t column;
    size_t length;
    
    FindResult() : found(false), line(0), column(0), length(0) {}
    FindResult(size_t l, size_t c, size_t len) 
        : found(true), line(l), column(c), length(len) {}
};

// =============================================================================
// TextSearcher - Helper for searching text buffer
// =============================================================================

namespace SuperTerminal {

class TextSearcher {
public:
    /// Find next occurrence of text
    /// @param buffer Text buffer to search
    /// @param cursor Current cursor position (start search from here)
    /// @param searchText Text to find
    /// @param caseSensitive Case-sensitive search
    /// @param wholeWord Match whole words only
    /// @param wrapAround Wrap to beginning if not found
    /// @param backward Search backward instead of forward
    /// @return Find result with position
    static FindResult findNext(const TextBuffer& buffer,
                              const Cursor& cursor,
                              const std::string& searchText,
                              bool caseSensitive,
                              bool wholeWord,
                              bool wrapAround,
                              bool backward);
    
    /// Replace current selection with text
    /// @param buffer Text buffer
    /// @param cursor Cursor with selection
    /// @param replaceText Replacement text
    /// @return true if replaced
    static bool replaceCurrent(TextBuffer& buffer,
                              Cursor& cursor,
                              const std::string& replaceText);
    
    /// Replace all occurrences of text
    /// @param buffer Text buffer
    /// @param cursor Cursor (will be positioned at last replacement)
    /// @param searchText Text to find
    /// @param replaceText Replacement text
    /// @param caseSensitive Case-sensitive search
    /// @param wholeWord Match whole words only
    /// @return Number of replacements made
    static int replaceAll(TextBuffer& buffer,
                         Cursor& cursor,
                         const std::string& searchText,
                         const std::string& replaceText,
                         bool caseSensitive,
                         bool wholeWord);
    
private:
    /// Check if character is word boundary
    static bool isWordBoundary(char ch);
    
    /// Check if position is at word boundary
    static bool isAtWordBoundary(const TextBuffer& buffer,
                                size_t line,
                                size_t column);
    
    /// Case-insensitive string comparison
    static bool equalsCaseInsensitive(const std::string& a,
                                     const std::string& b);
    
    /// Find text in line
    static size_t findInLine(const std::string& line,
                            const std::string& searchText,
                            size_t startColumn,
                            bool caseSensitive);
};

} // namespace SuperTerminal