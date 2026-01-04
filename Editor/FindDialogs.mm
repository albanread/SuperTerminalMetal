//
//  FindDialogs.mm
//  SuperTerminal Framework - Find/Replace and Go to Line Dialogs Implementation
//
//  Native macOS dialogs for text searching and navigation
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#import "FindDialogs.h"
#include "Editor/TextBuffer.h"
#include "Editor/Cursor.h"
#include <algorithm>
#include <cctype>

using namespace SuperTerminal;

// =============================================================================
// GotoLineDialog Implementation
// =============================================================================

@interface GotoLineDialog () <NSWindowDelegate>
@property (strong) NSWindow* window;
@property (strong) NSTextField* lineField;
@property (strong) NSTextField* infoLabel;
@property (assign) BOOL userConfirmed;
@property (assign) NSInteger targetLine;
@end

@implementation GotoLineDialog

// Singleton instance
+ (instancetype)sharedDialog {
    static GotoLineDialog* sharedInstance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sharedInstance = [[GotoLineDialog alloc] init];
        [sharedInstance createWindow];
    });
    return sharedInstance;
}

+ (BOOL)showDialogWithCurrentLine:(NSInteger)currentLine
                          maxLine:(NSInteger)maxLine
                          outLine:(NSInteger*)outLine {
    GotoLineDialog* dialog = [GotoLineDialog sharedDialog];

    // Reset dialog state
    [dialog.lineField setStringValue:[NSString stringWithFormat:@"%ld", (long)currentLine]];
    [dialog.infoLabel setStringValue:[NSString stringWithFormat:@"Enter line number (1-%ld):", (long)maxLine]];
    [dialog.lineField selectText:nil];
    dialog.userConfirmed = NO;
    dialog.targetLine = 0;

    if ([dialog runModal]) {
        *outLine = dialog.targetLine;
        return YES;
    }
    return NO;
}

- (void)createWindow {
    // Create window
    NSRect frame = NSMakeRect(0, 0, 300, 140);
    NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable;

    self.window = [[NSWindow alloc] initWithContentRect:frame
                                              styleMask:style
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    [self.window setTitle:@"Go to Line"];
    [self.window setDelegate:self];
    [self.window setReleasedWhenClosed:NO]; // Keep window alive
    [self.window center];

    NSView* contentView = self.window.contentView;

    // Info label
    self.infoLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 90, 260, 20)];
    [self.infoLabel setBezeled:NO];
    [self.infoLabel setDrawsBackground:NO];
    [self.infoLabel setEditable:NO];
    [self.infoLabel setSelectable:NO];
    [contentView addSubview:self.infoLabel];

    // Line number field
    self.lineField = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 60, 260, 24)];
    [contentView addSubview:self.lineField];

    // Buttons
    NSButton* cancelButton = [[NSButton alloc] initWithFrame:NSMakeRect(110, 20, 80, 28)];
    [cancelButton setTitle:@"Cancel"];
    [cancelButton setBezelStyle:NSBezelStyleRounded];
    [cancelButton setKeyEquivalent:@"\e"];
    [cancelButton setTarget:self];
    [cancelButton setAction:@selector(cancelClicked:)];
    [contentView addSubview:cancelButton];

    NSButton* goButton = [[NSButton alloc] initWithFrame:NSMakeRect(200, 20, 80, 28)];
    [goButton setTitle:@"Go"];
    [goButton setBezelStyle:NSBezelStyleRounded];
    [goButton setKeyEquivalent:@"\r"];
    [goButton setTarget:self];
    [goButton setAction:@selector(goClicked:)];
    [contentView addSubview:goButton];
}

- (BOOL)runModal {
    [self.window makeFirstResponder:self.lineField];
    [self.window makeKeyAndOrderFront:nil];

    // Use runModalForWindow instead of manual session loop
    NSModalResponse response = [NSApp runModalForWindow:self.window];

    // Hide window instead of closing it
    [self.window orderOut:nil];

    return self.userConfirmed;
}

- (BOOL)windowShouldClose:(NSWindow*)sender {
    // Stop modal when user clicks close button
    [NSApp stopModal];
    return YES;
}

- (void)goClicked:(id)sender {
    NSString* text = [self.lineField stringValue];
    NSInteger line = [text integerValue];

    if (line <= 0) {
        NSBeep();
        return;
    }

    // Copy values before stopping modal and closing window
    self.targetLine = line;
    self.userConfirmed = YES;
    [NSApp stopModal];
}

- (void)cancelClicked:(id)sender {
    self.userConfirmed = NO;
    [NSApp stopModal];
}

@end

// =============================================================================
// FindDialog Implementation
// =============================================================================

@interface FindDialog () <NSWindowDelegate>
@property (strong) NSWindow* window;
@property (strong) NSTextField* findField;
@property (strong) NSButton* caseSensitiveCheck;
@property (strong) NSButton* wholeWordCheck;
@property (strong) NSButton* wrapAroundCheck;
@property (assign) BOOL userConfirmed;
@property (copy) NSString* resultText;
@property (assign) FindOptions resultOptions;
@end

@implementation FindDialog

// Singleton instance
+ (instancetype)sharedDialog {
    static FindDialog* sharedInstance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sharedInstance = [[FindDialog alloc] init];
        [sharedInstance createWindow];
    });
    return sharedInstance;
}

+ (BOOL)showDialogWithDefaultText:(NSString*)defaultText
                          outText:(NSString**)outText
                       outOptions:(FindOptions*)outOptions {
    FindDialog* dialog = [FindDialog sharedDialog];

    // Reset dialog state
    if (defaultText) {
        [dialog.findField setStringValue:defaultText];
    } else {
        [dialog.findField setStringValue:@""];
    }
    [dialog.findField selectText:nil];

    dialog.userConfirmed = NO;
    dialog.resultText = nil;
    dialog.resultOptions = FindOptionNone;

    if ([dialog runModal]) {
        *outText = dialog.resultText;
        *outOptions = dialog.resultOptions;
        return YES;
    }
    return NO;
}

+ (void)showPanelWithDefaultText:(NSString*)defaultText
                          target:(id)target {
    // TODO: Implement non-modal panel
    NSLog(@"Non-modal find panel not yet implemented");
}

- (void)createWindow {
    // Create window
    NSRect frame = NSMakeRect(0, 0, 400, 200);
    NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable;

    self.window = [[NSWindow alloc] initWithContentRect:frame
                                              styleMask:style
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    [self.window setTitle:@"Find"];
    [self.window setDelegate:self];
    [self.window setReleasedWhenClosed:NO]; // Keep window alive
    [self.window center];

    NSView* contentView = self.window.contentView;

    // Find label
    NSTextField* label = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 160, 60, 20)];
    [label setStringValue:@"Find:"];
    [label setBezeled:NO];
    [label setDrawsBackground:NO];
    [label setEditable:NO];
    [label setSelectable:NO];
    [contentView addSubview:label];

    // Find field
    self.findField = [[NSTextField alloc] initWithFrame:NSMakeRect(80, 158, 300, 24)];
    [contentView addSubview:self.findField];

    // Options checkboxes
    self.caseSensitiveCheck = [[NSButton alloc] initWithFrame:NSMakeRect(20, 120, 200, 20)];
    [self.caseSensitiveCheck setButtonType:NSButtonTypeSwitch];
    [self.caseSensitiveCheck setTitle:@"Case Sensitive"];
    [contentView addSubview:self.caseSensitiveCheck];

    self.wholeWordCheck = [[NSButton alloc] initWithFrame:NSMakeRect(20, 95, 200, 20)];
    [self.wholeWordCheck setButtonType:NSButtonTypeSwitch];
    [self.wholeWordCheck setTitle:@"Whole Word"];
    [contentView addSubview:self.wholeWordCheck];

    self.wrapAroundCheck = [[NSButton alloc] initWithFrame:NSMakeRect(20, 70, 200, 20)];
    [self.wrapAroundCheck setButtonType:NSButtonTypeSwitch];
    [self.wrapAroundCheck setTitle:@"Wrap Around"];
    [self.wrapAroundCheck setState:NSControlStateValueOn];
    [contentView addSubview:self.wrapAroundCheck];

    // Buttons
    NSButton* cancelButton = [[NSButton alloc] initWithFrame:NSMakeRect(210, 20, 80, 28)];
    [cancelButton setTitle:@"Cancel"];
    [cancelButton setBezelStyle:NSBezelStyleRounded];
    [cancelButton setKeyEquivalent:@"\e"];
    [cancelButton setTarget:self];
    [cancelButton setAction:@selector(cancelClicked:)];
    [contentView addSubview:cancelButton];

    NSButton* findButton = [[NSButton alloc] initWithFrame:NSMakeRect(300, 20, 80, 28)];
    [findButton setTitle:@"Find"];
    [findButton setBezelStyle:NSBezelStyleRounded];
    [findButton setKeyEquivalent:@"\r"];
    [findButton setTarget:self];
    [findButton setAction:@selector(findClicked:)];
    [contentView addSubview:findButton];
}

- (BOOL)runModal {
    [self.window makeFirstResponder:self.findField];
    [self.window makeKeyAndOrderFront:nil];

    // Use runModalForWindow instead of manual session loop
    NSModalResponse response = [NSApp runModalForWindow:self.window];

    // Hide window instead of closing it
    [self.window orderOut:nil];

    return self.userConfirmed;
}

- (BOOL)windowShouldClose:(NSWindow*)sender {
    // Stop modal when user clicks close button
    [NSApp stopModal];
    return YES;
}

- (FindOptions)getOptions {
    FindOptions options = FindOptionNone;

    if ([self.caseSensitiveCheck state] == NSControlStateValueOn) {
        options |= FindOptionCaseSensitive;
    }
    if ([self.wholeWordCheck state] == NSControlStateValueOn) {
        options |= FindOptionWholeWord;
    }
    if ([self.wrapAroundCheck state] == NSControlStateValueOn) {
        options |= FindOptionWrapAround;
    }

    return options;
}

- (void)findClicked:(id)sender {
    if ([[self.findField stringValue] length] == 0) {
        NSBeep();
        return;
    }

    // Copy values before stopping modal and closing window
    self.resultText = [self.findField stringValue];
    self.resultOptions = [self getOptions];
    self.userConfirmed = YES;
    [NSApp stopModal];
}

- (void)cancelClicked:(id)sender {
    self.userConfirmed = NO;
    [NSApp stopModal];
}

@end

// =============================================================================
// FindReplaceDialog Implementation
// =============================================================================

@interface FindReplaceDialog () <NSWindowDelegate>
@property (strong) NSWindow* window;
@property (strong) NSTextField* findField;
@property (strong) NSTextField* replaceField;
@property (strong) NSButton* caseSensitiveCheck;
@property (strong) NSButton* wholeWordCheck;
@property (strong) NSButton* wrapAroundCheck;
@property (assign) FindReplaceAction action;
@property (copy) NSString* resultFindText;
@property (copy) NSString* resultReplaceText;
@property (assign) FindOptions resultOptions;
@end

@implementation FindReplaceDialog

// Singleton instance
+ (instancetype)sharedDialog {
    static FindReplaceDialog* sharedInstance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sharedInstance = [[FindReplaceDialog alloc] init];
        [sharedInstance createWindow];
    });
    return sharedInstance;
}

+ (BOOL)showDialogWithDefaultFindText:(NSString*)defaultFindText
                   defaultReplaceText:(NSString*)defaultReplaceText
                          outFindText:(NSString**)outFindText
                       outReplaceText:(NSString**)outReplaceText
                           outOptions:(FindOptions*)outOptions
                            outAction:(FindReplaceAction*)outAction {
    FindReplaceDialog* dialog = [FindReplaceDialog sharedDialog];

    // Reset dialog state
    if (defaultFindText) {
        [dialog.findField setStringValue:defaultFindText];
    } else {
        [dialog.findField setStringValue:@""];
    }
    if (defaultReplaceText) {
        [dialog.replaceField setStringValue:defaultReplaceText];
    } else {
        [dialog.replaceField setStringValue:@""];
    }
    [dialog.findField selectText:nil];

    dialog.resultFindText = nil;
    dialog.resultReplaceText = nil;
    dialog.resultOptions = FindOptionNone;
    dialog.action = FindReplaceActionCancel;

    if ([dialog runModal]) {
        *outFindText = dialog.resultFindText;
        *outReplaceText = dialog.resultReplaceText;
        *outOptions = dialog.resultOptions;
        *outAction = dialog.action;
        return YES;
    }
    return NO;
}

+ (void)showPanelWithDefaultFindText:(NSString*)defaultFindText
                  defaultReplaceText:(NSString*)defaultReplaceText
                              target:(id)target {
    // TODO: Implement non-modal panel
    NSLog(@"Non-modal find/replace panel not yet implemented");
}

- (void)createWindow {
    // Create window
    NSRect frame = NSMakeRect(0, 0, 400, 250);
    NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable;

    self.window = [[NSWindow alloc] initWithContentRect:frame
                                              styleMask:style
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    [self.window setTitle:@"Find and Replace"];
    [self.window setDelegate:self];
    [self.window setReleasedWhenClosed:NO]; // Keep window alive
    [self.window center];

    NSView* contentView = self.window.contentView;

    // Find label and field
    NSTextField* findLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 210, 60, 20)];
    [findLabel setStringValue:@"Find:"];
    [findLabel setBezeled:NO];
    [findLabel setDrawsBackground:NO];
    [findLabel setEditable:NO];
    [findLabel setSelectable:NO];
    [contentView addSubview:findLabel];

    self.findField = [[NSTextField alloc] initWithFrame:NSMakeRect(80, 208, 300, 24)];
    [contentView addSubview:self.findField];

    // Replace label and field
    NSTextField* replaceLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 175, 60, 20)];
    [replaceLabel setStringValue:@"Replace:"];
    [replaceLabel setBezeled:NO];
    [replaceLabel setDrawsBackground:NO];
    [replaceLabel setEditable:NO];
    [replaceLabel setSelectable:NO];
    [contentView addSubview:replaceLabel];

    self.replaceField = [[NSTextField alloc] initWithFrame:NSMakeRect(80, 173, 300, 24)];
    [contentView addSubview:self.replaceField];

    // Options checkboxes
    self.caseSensitiveCheck = [[NSButton alloc] initWithFrame:NSMakeRect(20, 135, 200, 20)];
    [self.caseSensitiveCheck setButtonType:NSButtonTypeSwitch];
    [self.caseSensitiveCheck setTitle:@"Case Sensitive"];
    [contentView addSubview:self.caseSensitiveCheck];

    self.wholeWordCheck = [[NSButton alloc] initWithFrame:NSMakeRect(20, 110, 200, 20)];
    [self.wholeWordCheck setButtonType:NSButtonTypeSwitch];
    [self.wholeWordCheck setTitle:@"Whole Word"];
    [contentView addSubview:self.wholeWordCheck];

    self.wrapAroundCheck = [[NSButton alloc] initWithFrame:NSMakeRect(20, 85, 200, 20)];
    [self.wrapAroundCheck setButtonType:NSButtonTypeSwitch];
    [self.wrapAroundCheck setTitle:@"Wrap Around"];
    [self.wrapAroundCheck setState:NSControlStateValueOn];
    [contentView addSubview:self.wrapAroundCheck];

    // Buttons
    NSButton* cancelButton = [[NSButton alloc] initWithFrame:NSMakeRect(20, 20, 80, 28)];
    [cancelButton setTitle:@"Cancel"];
    [cancelButton setBezelStyle:NSBezelStyleRounded];
    [cancelButton setKeyEquivalent:@"\e"];
    [cancelButton setTarget:self];
    [cancelButton setAction:@selector(cancelClicked:)];
    [contentView addSubview:cancelButton];

    NSButton* findButton = [[NSButton alloc] initWithFrame:NSMakeRect(120, 20, 80, 28)];
    [findButton setTitle:@"Find Next"];
    [findButton setBezelStyle:NSBezelStyleRounded];
    [findButton setKeyEquivalent:@"\r"];
    [findButton setTarget:self];
    [findButton setAction:@selector(findNextClicked:)];
    [contentView addSubview:findButton];

    NSButton* replaceButton = [[NSButton alloc] initWithFrame:NSMakeRect(210, 20, 80, 28)];
    [replaceButton setTitle:@"Replace"];
    [replaceButton setBezelStyle:NSBezelStyleRounded];
    [replaceButton setTarget:self];
    [replaceButton setAction:@selector(replaceClicked:)];
    [contentView addSubview:replaceButton];

    NSButton* replaceAllButton = [[NSButton alloc] initWithFrame:NSMakeRect(300, 20, 80, 28)];
    [replaceAllButton setTitle:@"Replace All"];
    [replaceAllButton setBezelStyle:NSBezelStyleRounded];
    [replaceAllButton setTarget:self];
    [replaceAllButton setAction:@selector(replaceAllClicked:)];
    [contentView addSubview:replaceAllButton];
}

- (BOOL)runModal {
    [self.window makeFirstResponder:self.findField];
    [self.window makeKeyAndOrderFront:nil];

    self.action = FindReplaceActionCancel;

    // Use runModalForWindow instead of manual session loop
    NSModalResponse response = [NSApp runModalForWindow:self.window];

    // Hide window instead of closing it
    [self.window orderOut:nil];

    return self.action != FindReplaceActionCancel;
}

- (BOOL)windowShouldClose:(NSWindow*)sender {
    // Stop modal when user clicks close button
    [NSApp stopModal];
    return YES;
}

- (FindOptions)getOptions {
    FindOptions options = FindOptionNone;

    if ([self.caseSensitiveCheck state] == NSControlStateValueOn) {
        options |= FindOptionCaseSensitive;
    }
    if ([self.wholeWordCheck state] == NSControlStateValueOn) {
        options |= FindOptionWholeWord;
    }
    if ([self.wrapAroundCheck state] == NSControlStateValueOn) {
        options |= FindOptionWrapAround;
    }

    return options;
}

- (void)findNextClicked:(id)sender {
    if ([[self.findField stringValue] length] == 0) {
        NSBeep();
        return;
    }

    // Copy values before stopping modal and closing window
    self.resultFindText = [self.findField stringValue];
    self.resultReplaceText = [self.replaceField stringValue];
    self.resultOptions = [self getOptions];
    self.action = FindReplaceActionFindNext;
    [NSApp stopModal];
}

- (void)replaceClicked:(id)sender {
    if ([[self.findField stringValue] length] == 0) {
        NSBeep();
        return;
    }

    // Copy values before stopping modal and closing window
    self.resultFindText = [self.findField stringValue];
    self.resultReplaceText = [self.replaceField stringValue];
    self.resultOptions = [self getOptions];
    self.action = FindReplaceActionReplace;
    [NSApp stopModal];
}

- (void)replaceAllClicked:(id)sender {
    if ([[self.findField stringValue] length] == 0) {
        NSBeep();
        return;
    }

    // Copy values before stopping modal and closing window
    self.resultFindText = [self.findField stringValue];
    self.resultReplaceText = [self.replaceField stringValue];
    self.resultOptions = [self getOptions];
    self.action = FindReplaceActionReplaceAll;
    [NSApp stopModal];
}

- (void)cancelClicked:(id)sender {
    self.action = FindReplaceActionCancel;
    [NSApp stopModal];
}

@end

// =============================================================================
// TextSearcher Implementation
// =============================================================================

namespace SuperTerminal {

FindResult TextSearcher::findNext(const TextBuffer& buffer,
                                 const Cursor& cursor,
                                 const std::string& searchText,
                                 bool caseSensitive,
                                 bool wholeWord,
                                 bool wrapAround,
                                 bool backward) {
    if (searchText.empty()) {
        return FindResult();
    }

    size_t startLine = cursor.getLine();
    size_t startColumn = cursor.getColumn();
    size_t lineCount = buffer.getLineCount();

    // Search forward
    if (!backward) {
        // Search current line from cursor position
        std::string line = buffer.getLine(startLine);
        size_t pos = findInLine(line, searchText, startColumn + 1, caseSensitive);

        if (pos != std::string::npos) {
            if (!wholeWord || isAtWordBoundary(buffer, startLine, pos)) {
                return FindResult(startLine, pos, searchText.length());
            }
        }

        // Search remaining lines
        for (size_t i = startLine + 1; i < lineCount; ++i) {
            line = buffer.getLine(i);
            pos = findInLine(line, searchText, 0, caseSensitive);

            if (pos != std::string::npos) {
                if (!wholeWord || isAtWordBoundary(buffer, i, pos)) {
                    return FindResult(i, pos, searchText.length());
                }
            }
        }

        // Wrap around to beginning
        if (wrapAround) {
            for (size_t i = 0; i <= startLine; ++i) {
                line = buffer.getLine(i);
                size_t searchStart = (i == startLine) ? 0 : 0;
                size_t searchEnd = (i == startLine) ? startColumn : std::string::npos;

                pos = findInLine(line, searchText, searchStart, caseSensitive);

                if (pos != std::string::npos && (searchEnd == std::string::npos || pos < searchEnd)) {
                    if (!wholeWord || isAtWordBoundary(buffer, i, pos)) {
                        return FindResult(i, pos, searchText.length());
                    }
                }
            }
        }
    }

    return FindResult();
}

bool TextSearcher::replaceCurrent(TextBuffer& buffer,
                                 Cursor& cursor,
                                 const std::string& replaceText) {
    if (!cursor.hasSelection()) {
        return false;
    }

    auto selection = cursor.getSelection();
    Position start = selection.first;
    Position end = selection.second;

    // Delete selection
    buffer.deleteRange(start.line, start.column, end.line, end.column);

    // Insert replacement
    buffer.insertText(start.line, start.column, replaceText);

    // Update cursor
    cursor.setPosition(start.line, start.column + replaceText.length(), buffer);
    cursor.clearSelection();

    return true;
}

int TextSearcher::replaceAll(TextBuffer& buffer,
                            Cursor& cursor,
                            const std::string& searchText,
                            const std::string& replaceText,
                            bool caseSensitive,
                            bool wholeWord) {
    int count = 0;

    // Start from beginning
    cursor.setPosition(0, 0, buffer);

    while (true) {
        FindResult result = findNext(buffer, cursor, searchText, caseSensitive, wholeWord, false, false);

        if (!result.found) {
            break;
        }

        // Select found text
        cursor.setPosition(result.line, result.column, buffer);
        cursor.startSelection();
        cursor.setPosition(result.line, result.column + result.length, buffer);

        // Replace it
        if (replaceCurrent(buffer, cursor, replaceText)) {
            count++;
        }
    }

    return count;
}

bool TextSearcher::isWordBoundary(char ch) {
    return !std::isalnum(static_cast<unsigned char>(ch)) && ch != '_';
}

bool TextSearcher::isAtWordBoundary(const TextBuffer& buffer,
                                   size_t line,
                                   size_t column) {
    std::string lineText = buffer.getLine(line);

    // Check before
    if (column > 0) {
        if (!isWordBoundary(lineText[column - 1])) {
            return false;
        }
    }

    // Check after
    if (column < lineText.length()) {
        if (!isWordBoundary(lineText[column])) {
            return false;
        }
    }

    return true;
}

bool TextSearcher::equalsCaseInsensitive(const std::string& a, const std::string& b) {
    if (a.length() != b.length()) {
        return false;
    }

    for (size_t i = 0; i < a.length(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }

    return true;
}

size_t TextSearcher::findInLine(const std::string& line,
                               const std::string& searchText,
                               size_t startColumn,
                               bool caseSensitive) {
    if (startColumn >= line.length()) {
        return std::string::npos;
    }

    if (caseSensitive) {
        return line.find(searchText, startColumn);
    } else {
        // Case-insensitive search
        std::string lowerLine = line;
        std::string lowerSearch = searchText;

        std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(),
                      [](unsigned char c) { return std::tolower(c); });

        return lowerLine.find(lowerSearch, startColumn);
    }
}

} // namespace SuperTerminal
