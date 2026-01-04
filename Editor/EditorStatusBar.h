//
//  EditorStatusBar.h
//  SuperTerminal Framework - Editor Status Bar
//
//  Native Cocoa status bar view for bottom of editor window
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef EDITOR_STATUS_BAR_H
#define EDITOR_STATUS_BAR_H

#import <Cocoa/Cocoa.h>

// =============================================================================
// EditorStatusBar - Native macOS status bar at bottom of window
// =============================================================================
//
// A native Cocoa view that displays editor status information at the bottom
// of the window (like Xcode, VS Code, etc.)
//
// Layout:
// ┌────────────────────────────────────────────────────────────────┐
// │ script.lua · Lua · Line 45, Col 12 · 1234 lines · UTF-8 · LF  │
// └────────────────────────────────────────────────────────────────┘
//
// Left side: File info
// Right side: Encoding, line ending, etc.
//

@interface EditorStatusBar : NSView

// =============================================================================
// Properties
// =============================================================================

/// Status bar height (default 22 pixels - standard macOS height)
@property (nonatomic, assign) CGFloat height;

/// Status bar background color
@property (nonatomic, strong) NSColor *backgroundColor;

/// Status bar text color
@property (nonatomic, strong) NSColor *textColor;

// =============================================================================
// Status Information
// =============================================================================

/// Set filename being edited
@property (nonatomic, copy) NSString *filename;

/// Set programming language
@property (nonatomic, copy) NSString *language;

/// Set current line number (1-based)
@property (nonatomic, assign) NSInteger currentLine;

/// Set current column number (1-based)
@property (nonatomic, assign) NSInteger currentColumn;

/// Set total line count
@property (nonatomic, assign) NSInteger totalLines;

/// Set modified flag
@property (nonatomic, assign) BOOL modified;

/// Set encoding (e.g., "UTF-8")
@property (nonatomic, copy) NSString *encoding;

/// Set line ending type (e.g., "LF", "CRLF", "CR")
@property (nonatomic, copy) NSString *lineEnding;

/// Set screen mode (e.g., "80×25", "40×25")
@property (nonatomic, copy) NSString *screenMode;

// =============================================================================
// Initialization
// =============================================================================

/// Initialize with frame
- (instancetype)initWithFrame:(NSRect)frame;

// =============================================================================
// Update
// =============================================================================

/// Update all status information at once
- (void)updateWithFilename:(NSString *)filename
                  language:(NSString *)language
                      line:(NSInteger)line
                    column:(NSInteger)column
                totalLines:(NSInteger)totalLines
                  modified:(BOOL)modified;

/// Force redraw
- (void)refresh;

// =============================================================================
// Appearance
// =============================================================================

/// Use light appearance
- (void)useLightAppearance;

/// Use dark appearance
- (void)useDarkAppearance;

/// Use system appearance (automatic light/dark)
- (void)useSystemAppearance;

@end

#endif // EDITOR_STATUS_BAR_H