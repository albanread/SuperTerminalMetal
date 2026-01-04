//
//  EditorStatusBar.mm
//  SuperTerminal Framework - Editor Status Bar Implementation
//
//  Native Cocoa status bar view for bottom of editor window
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#import "EditorStatusBar.h"

// =============================================================================
// EditorStatusBar Implementation
// =============================================================================

@implementation EditorStatusBar {
    // Text fields for different sections
    NSTextField *_leftLabel;
    NSTextField *_rightLabel;

    // Cached values
    NSString *_cachedLeftText;
    NSString *_cachedRightText;
}

// =============================================================================
// Initialization
// =============================================================================

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        [self setupDefaults];
        [self setupSubviews];
        [self useDarkAppearance];  // Default to dark
    }
    return self;
}

- (void)setupDefaults {
    _height = 22.0;  // Standard macOS status bar height
    _filename = @"untitled";
    _language = @"Text";
    _currentLine = 1;
    _currentColumn = 1;
    _totalLines = 1;
    _modified = NO;
    _encoding = @"UTF-8";
    _lineEnding = @"LF";
    _screenMode = @"80×25";

    _cachedLeftText = @"";
    _cachedRightText = @"";
}

- (void)setupSubviews {
    // Left label (file info)
    _leftLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(8, 2, 400, 18)];
    _leftLabel.editable = NO;
    _leftLabel.selectable = NO;
    _leftLabel.bordered = NO;
    _leftLabel.drawsBackground = NO;
    _leftLabel.font = [NSFont systemFontOfSize:11];
    _leftLabel.alignment = NSTextAlignmentLeft;
    [self addSubview:_leftLabel];

    // Right label (encoding, line ending, etc.)
    _rightLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 2, 300, 18)];
    _rightLabel.editable = NO;
    _rightLabel.selectable = NO;
    _rightLabel.bordered = NO;
    _rightLabel.drawsBackground = NO;
    _rightLabel.font = [NSFont systemFontOfSize:11];
    _rightLabel.alignment = NSTextAlignmentRight;
    [self addSubview:_rightLabel];

    [self updateLayout];
}

- (void)updateLayout {
    NSRect bounds = self.bounds;

    // Left label takes left side
    _leftLabel.frame = NSMakeRect(8, 2, bounds.size.width * 0.6, 18);

    // Right label takes right side
    CGFloat rightWidth = bounds.size.width * 0.4;
    _rightLabel.frame = NSMakeRect(bounds.size.width - rightWidth - 8, 2, rightWidth, 18);
}

// =============================================================================
// Layout
// =============================================================================

- (void)resizeSubviewsWithOldSize:(NSSize)oldSize {
    [super resizeSubviewsWithOldSize:oldSize];
    [self updateLayout];
}

- (void)setFrameSize:(NSSize)newSize {
    [super setFrameSize:newSize];
    [self updateLayout];
}

// =============================================================================
// Drawing
// =============================================================================

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];

    // Draw background
    [_backgroundColor setFill];
    NSRectFill(dirtyRect);

    // Draw top border line
    NSRect borderRect = NSMakeRect(0, NSHeight(self.bounds) - 1, NSWidth(self.bounds), 1);
    [[NSColor colorWithWhite:0.3 alpha:1.0] setFill];
    NSRectFill(borderRect);
}

// =============================================================================
// Property Setters
// =============================================================================

- (void)setFilename:(NSString *)filename {
    if (![_filename isEqualToString:filename]) {
        _filename = [filename copy];
        [self updateLeftLabel];
    }
}

- (void)setLanguage:(NSString *)language {
    if (![_language isEqualToString:language]) {
        _language = [language copy];
        [self updateLeftLabel];
    }
}

- (void)setCurrentLine:(NSInteger)currentLine {
    if (_currentLine != currentLine) {
        _currentLine = currentLine;
        [self updateLeftLabel];
    }
}

- (void)setCurrentColumn:(NSInteger)currentColumn {
    if (_currentColumn != currentColumn) {
        _currentColumn = currentColumn;
        [self updateLeftLabel];
    }
}

- (void)setTotalLines:(NSInteger)totalLines {
    if (_totalLines != totalLines) {
        _totalLines = totalLines;
        [self updateLeftLabel];
    }
}

- (void)setModified:(BOOL)modified {
    if (_modified != modified) {
        _modified = modified;
        [self updateLeftLabel];
    }
}

- (void)setEncoding:(NSString *)encoding {
    if (![_encoding isEqualToString:encoding]) {
        _encoding = [encoding copy];
        [self updateRightLabel];
    }
}

- (void)setLineEnding:(NSString *)lineEnding {
    if (![_lineEnding isEqualToString:lineEnding]) {
        _lineEnding = [lineEnding copy];
        [self updateRightLabel];
    }
}

- (void)setScreenMode:(NSString *)screenMode {
    if (![_screenMode isEqualToString:screenMode]) {
        _screenMode = [screenMode copy];
        [self updateRightLabel];
    }
}

- (void)setBackgroundColor:(NSColor *)backgroundColor {
    if (![_backgroundColor isEqual:backgroundColor]) {
        _backgroundColor = backgroundColor;
        [self setNeedsDisplay:YES];
    }
}

- (void)setTextColor:(NSColor *)textColor {
    if (![_textColor isEqual:textColor]) {
        _textColor = textColor;
        _leftLabel.textColor = textColor;
        _rightLabel.textColor = textColor;
    }
}

// =============================================================================
// Update Methods
// =============================================================================

- (void)updateWithFilename:(NSString *)filename
                  language:(NSString *)language
                      line:(NSInteger)line
                    column:(NSInteger)column
                totalLines:(NSInteger)totalLines
                  modified:(BOOL)modified
{
    _filename = [filename copy];
    _language = [language copy];
    _currentLine = line;
    _currentColumn = column;
    _totalLines = totalLines;
    _modified = modified;

    [self updateLeftLabel];
}

- (void)updateLeftLabel {
    // Build left status string
    NSMutableString *leftText = [NSMutableString string];

    // Modified indicator
    if (_modified) {
        [leftText appendString:@"● "];
    }

    // Filename
    [leftText appendString:_filename];

    // Language
    if (_language && _language.length > 0) {
        [leftText appendFormat:@" · %@", _language];
    }

    // Position
    [leftText appendFormat:@" · Line %ld, Col %ld", (long)_currentLine, (long)_currentColumn];

    // Total lines
    [leftText appendFormat:@" · %ld lines", (long)_totalLines];

    // Only update if changed (reduce redraws)
    if (![_cachedLeftText isEqualToString:leftText]) {
        _cachedLeftText = [leftText copy];
        _leftLabel.stringValue = leftText;
    }
}

- (void)updateRightLabel {
    // Build right status string
    NSMutableString *rightText = [NSMutableString string];

    // Screen mode
    if (_screenMode && _screenMode.length > 0) {
        [rightText appendFormat:@"%@", _screenMode];
    }

    // Encoding
    if (_encoding && _encoding.length > 0) {
        if (rightText.length > 0) [rightText appendString:@" · "];
        [rightText appendString:_encoding];
    }

    // Line ending
    if (_lineEnding && _lineEnding.length > 0) {
        if (rightText.length > 0) [rightText appendString:@" · "];
        [rightText appendString:_lineEnding];
    }

    // Only update if changed
    if (![_cachedRightText isEqualToString:rightText]) {
        _cachedRightText = [rightText copy];
        _rightLabel.stringValue = rightText;
    }
}

- (void)refresh {
    [self updateLeftLabel];
    [self updateRightLabel];
    [self setNeedsDisplay:YES];
}

// =============================================================================
// Appearance
// =============================================================================

- (void)useLightAppearance {
    self.backgroundColor = [NSColor colorWithWhite:0.95 alpha:1.0];
    self.textColor = [NSColor colorWithWhite:0.2 alpha:1.0];
}

- (void)useDarkAppearance {
    self.backgroundColor = [NSColor colorWithWhite:0.15 alpha:1.0];
    self.textColor = [NSColor colorWithWhite:0.85 alpha:1.0];
}

- (void)useSystemAppearance {
    if (@available(macOS 10.14, *)) {
        NSString *appearanceName = [[NSApp effectiveAppearance] name];
        if ([appearanceName containsString:@"Dark"]) {
            [self useDarkAppearance];
        } else {
            [self useLightAppearance];
        }
    } else {
        [self useLightAppearance];
    }
}

// =============================================================================
// NSView Overrides
// =============================================================================

- (BOOL)isFlipped {
    return YES;  // Use top-left origin
}

- (BOOL)acceptsFirstResponder {
    return NO;  // Status bar doesn't accept keyboard input
}

- (NSSize)intrinsicContentSize {
    return NSMakeSize(NSViewNoIntrinsicMetric, _height);
}

@end
