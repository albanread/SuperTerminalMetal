//
//  ScriptBrowserDialog.mm
//  SuperTerminal Framework - Script Browser Dialog Implementation
//
//  Native macOS dialog for browsing and opening scripts from database
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#import "ScriptBrowserDialog.h"
#include <vector>
#include <algorithm>

using namespace SuperTerminal;

// Structure to represent a script (from database or cart)
struct ScriptEntry {
    std::string name;
    ScriptLanguage language;
    bool isFromCart;
    std::string path;  // For cart scripts
    size_t contentLength;
    time_t modifiedAt;

    ScriptEntry(const std::string& n, ScriptLanguage lang, bool fromCart = false, const std::string& p = "")
        : name(n), language(lang), isFromCart(fromCart), path(p), contentLength(0), modifiedAt(0) {}
};

// =============================================================================
// ScriptBrowserDialog Implementation
// =============================================================================

@interface ScriptBrowserDialog () <NSTableViewDataSource, NSTableViewDelegate, NSWindowDelegate>

@property (strong) NSWindow* window;
@property (strong) NSTableView* tableView;
@property (strong) NSSearchField* searchField;
@property (strong) NSPopUpButton* languagePopup;
@property (strong) NSButton* openButton;
@property (strong) NSButton* cancelButton;
@property (strong) NSButton* deleteButton;
@property (strong) NSButton* changeToBasButton;
@property (strong) NSButton* changeToAbcButton;
@property (strong) NSButton* changeToVscriptButton;
@property (assign) std::shared_ptr<ScriptDatabase> database;
@property (assign) std::shared_ptr<CartManager> cartManager;
@property (assign) std::vector<ScriptEntry> scripts;
@property (assign) std::vector<ScriptEntry> filteredScripts;
@property (assign) ScriptLanguage selectedLanguage;
@property (assign) BOOL userConfirmed;
@property (assign) NSInteger selectedRow;

@end

@implementation ScriptBrowserDialog

// =============================================================================
// Class Methods
// =============================================================================

+ (BOOL)showBrowserWithDatabase:(std::shared_ptr<ScriptDatabase>)database
                       language:(ScriptLanguage)language
                    cartManager:(std::shared_ptr<CartManager>)cartManager
                        outName:(std::string&)outName
                    outLanguage:(ScriptLanguage&)outLanguage
                   outIsFromCart:(BOOL&)outIsFromCart {
    NSLog(@"[ScriptBrowserDialog] === showBrowserWithDatabase called ===");
    NSLog(@"[ScriptBrowserDialog] Creating new dialog instance");
    ScriptBrowserDialog* dialog = [[ScriptBrowserDialog alloc] initWithDatabase:database
                                                                       language:language
                                                                    cartManager:cartManager];
    NSLog(@"[ScriptBrowserDialog] Dialog instance created: %p", dialog);
    NSLog(@"[ScriptBrowserDialog] Calling runModalWithName...");
    BOOL result = [dialog runModalWithName:outName language:outLanguage outIsFromCart:outIsFromCart];
    NSLog(@"[ScriptBrowserDialog] runModalWithName returned: %d", result);
    if (result) {
        NSLog(@"[ScriptBrowserDialog] Selected: '%s'", outName.c_str());
    } else {
        NSLog(@"[ScriptBrowserDialog] User cancelled or no selection");
    }
    return result;
}

// =============================================================================
// Initialization
// =============================================================================

- (instancetype)initWithDatabase:(std::shared_ptr<ScriptDatabase>)database
                        language:(ScriptLanguage)language
                     cartManager:(std::shared_ptr<CartManager>)cartManager {
    NSLog(@"[ScriptBrowserDialog] initWithDatabase called");
    self = [super init];
    if (self) {
        _database = database;
        _cartManager = cartManager;
        _selectedLanguage = language;
        _userConfirmed = NO;
        _selectedRow = -1;
        NSLog(@"[ScriptBrowserDialog] Creating window...");
        [self createWindow];
        NSLog(@"[ScriptBrowserDialog] Loading scripts...");
        [self loadScripts];
        NSLog(@"[ScriptBrowserDialog] Initialization complete");
    }
    return self;
}

- (void)createWindow {
    // Create window
    NSRect frame = NSMakeRect(0, 0, 600, 400);
    NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable;

    self.window = [[NSWindow alloc] initWithContentRect:frame
                                              styleMask:style
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    [self.window setTitle:@"Open Script"];
    [self.window setDelegate:self];
    [self.window center];

    NSView* contentView = self.window.contentView;

    // Search field
    self.searchField = [[NSSearchField alloc] initWithFrame:NSMakeRect(20, 350, 360, 24)];
    [self.searchField setPlaceholderString:@"Search scripts..."];
    [self.searchField setTarget:self];
    [self.searchField setAction:@selector(searchFieldChanged:)];
    [contentView addSubview:self.searchField];

    // Language filter popup
    self.languagePopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(390, 348, 190, 28)];
    [self.languagePopup addItemWithTitle:@"All Languages"];
    [self.languagePopup addItemWithTitle:@"Lua"];
    [self.languagePopup addItemWithTitle:@"JavaScript"];
    [self.languagePopup addItemWithTitle:@"BASIC"];
    [self.languagePopup addItemWithTitle:@"Scheme"];
    [self.languagePopup addItemWithTitle:@"ABC"];
    [self.languagePopup addItemWithTitle:@"VoiceScript"];
    [self.languagePopup setTarget:self];
    [self.languagePopup setAction:@selector(languageFilterChanged:)];
    [contentView addSubview:self.languagePopup];

    // Table view with scroll view
    NSScrollView* scrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(20, 60, 560, 275)];
    [scrollView setBorderType:NSBezelBorder];
    [scrollView setHasVerticalScroller:YES];
    [scrollView setAutohidesScrollers:YES];

    self.tableView = [[NSTableView alloc] initWithFrame:scrollView.bounds];
    [self.tableView setDataSource:self];
    [self.tableView setDelegate:self];
    [self.tableView setTarget:self];
    [self.tableView setDoubleAction:@selector(tableViewDoubleClicked:)];
    [self.tableView setAllowsEmptySelection:NO];

    // Add columns
    NSTableColumn* nameColumn = [[NSTableColumn alloc] initWithIdentifier:@"name"];
    [[nameColumn headerCell] setStringValue:@"Name"];
    [nameColumn setWidth:200];
    [self.tableView addTableColumn:nameColumn];

    NSTableColumn* languageColumn = [[NSTableColumn alloc] initWithIdentifier:@"language"];
    [[languageColumn headerCell] setStringValue:@"Language"];
    [languageColumn setWidth:80];
    [self.tableView addTableColumn:languageColumn];

    NSTableColumn* sourceColumn = [[NSTableColumn alloc] initWithIdentifier:@"source"];
    [[sourceColumn headerCell] setStringValue:@"Source"];
    [sourceColumn setWidth:80];
    [self.tableView addTableColumn:sourceColumn];

    NSTableColumn* sizeColumn = [[NSTableColumn alloc] initWithIdentifier:@"size"];
    [[sizeColumn headerCell] setStringValue:@"Size"];
    [sizeColumn setWidth:80];
    [self.tableView addTableColumn:sizeColumn];

    NSTableColumn* modifiedColumn = [[NSTableColumn alloc] initWithIdentifier:@"modified"];
    [[modifiedColumn headerCell] setStringValue:@"Modified"];
    [modifiedColumn setWidth:120];
    [self.tableView addTableColumn:modifiedColumn];

    [scrollView setDocumentView:self.tableView];
    [contentView addSubview:scrollView];

    // Buttons
    self.deleteButton = [[NSButton alloc] initWithFrame:NSMakeRect(20, 20, 80, 28)];
    [self.deleteButton setTitle:@"Delete"];
    [self.deleteButton setBezelStyle:NSBezelStyleRounded];
    [self.deleteButton setTarget:self];
    [self.deleteButton setAction:@selector(deleteButtonClicked:)];
    [contentView addSubview:self.deleteButton];

    // Type change buttons
    self.changeToBasButton = [[NSButton alloc] initWithFrame:NSMakeRect(120, 20, 60, 28)];
    [self.changeToBasButton setTitle:@".bas"];
    [self.changeToBasButton setBezelStyle:NSBezelStyleRounded];
    [self.changeToBasButton setTarget:self];
    [self.changeToBasButton setAction:@selector(changeTypeToBasic:)];
    [contentView addSubview:self.changeToBasButton];

    self.changeToAbcButton = [[NSButton alloc] initWithFrame:NSMakeRect(190, 20, 60, 28)];
    [self.changeToAbcButton setTitle:@".abc"];
    [self.changeToAbcButton setBezelStyle:NSBezelStyleRounded];
    [self.changeToAbcButton setTarget:self];
    [self.changeToAbcButton setAction:@selector(changeTypeToAbc:)];
    [contentView addSubview:self.changeToAbcButton];

    self.changeToVscriptButton = [[NSButton alloc] initWithFrame:NSMakeRect(260, 20, 80, 28)];
    [self.changeToVscriptButton setTitle:@".vscript"];
    [self.changeToVscriptButton setBezelStyle:NSBezelStyleRounded];
    [self.changeToVscriptButton setTarget:self];
    [self.changeToVscriptButton setAction:@selector(changeTypeToVoiceScript:)];
    [contentView addSubview:self.changeToVscriptButton];

    self.cancelButton = [[NSButton alloc] initWithFrame:NSMakeRect(400, 20, 80, 28)];
    [self.cancelButton setTitle:@"Cancel"];
    [self.cancelButton setBezelStyle:NSBezelStyleRounded];
    [self.cancelButton setKeyEquivalent:@"\e"];
    [self.cancelButton setTarget:self];
    [self.cancelButton setAction:@selector(cancelButtonClicked:)];
    [contentView addSubview:self.cancelButton];

    self.openButton = [[NSButton alloc] initWithFrame:NSMakeRect(500, 20, 80, 28)];
    [self.openButton setTitle:@"Open"];
    [self.openButton setBezelStyle:NSBezelStyleRounded];
    [self.openButton setKeyEquivalent:@"\r"];
    [self.openButton setTarget:self];
    [self.openButton setAction:@selector(openButtonClicked:)];
    [contentView addSubview:self.openButton];
}

// =============================================================================
// Data Loading
// =============================================================================

- (void)loadScripts {
    NSLog(@"[ScriptBrowserDialog] loadScripts called");
    _scripts.clear();

    // Check if cart is active
    bool cartActive = _cartManager && _cartManager->isCartActive();

    if (cartActive) {
        NSLog(@"[ScriptBrowserDialog] Cart is active - loading scripts from cart...");

        // Get cart loader
        auto cartLoader = _cartManager->getLoader();
        if (cartLoader && cartLoader->isLoaded()) {
            // List all data files in the cart
            auto dataFiles = cartLoader->listDataFiles();
            NSLog(@"[ScriptBrowserDialog] Found %zu data files in cart", dataFiles.size());

            // Filter for script files (common extensions)
            for (const auto& path : dataFiles) {
                // Check if it's a script file based on extension
                ScriptLanguage lang = ScriptLanguage::LUA;
                bool isScript = false;

                if (path.find(".bas") != std::string::npos) {
                    lang = ScriptLanguage::BASIC;
                    isScript = true;
                } else if (path.find(".lua") != std::string::npos) {
                    lang = ScriptLanguage::LUA;
                    isScript = true;
                } else if (path.find(".js") != std::string::npos) {
                    lang = ScriptLanguage::JAVASCRIPT;
                    isScript = true;
                } else if (path.find(".scm") != std::string::npos || path.find(".scheme") != std::string::npos) {
                    lang = ScriptLanguage::SCHEME;
                    isScript = true;
                } else if (path.find(".abc") != std::string::npos) {
                    lang = ScriptLanguage::ABC;
                    isScript = true;
                } else if (path.find(".vscript") != std::string::npos) {
                    lang = ScriptLanguage::VOICESCRIPT;
                    isScript = true;
                }

                if (isScript) {
                    // Extract filename from path
                    std::string name = path;
                    size_t lastSlash = path.find_last_of('/');
                    if (lastSlash != std::string::npos) {
                        name = path.substr(lastSlash + 1);
                    }

                    _scripts.emplace_back(name, lang, true, path);
                    NSLog(@"[ScriptBrowserDialog] Added cart script: %s", name.c_str());
                }
            }

            NSLog(@"[ScriptBrowserDialog] Loaded %zu scripts from cart", _scripts.size());
        }
    } else {
        NSLog(@"[ScriptBrowserDialog] No cart active - loading scripts from database...");

        if (!_database) {
            NSLog(@"[ScriptBrowserDialog] ERROR: No database!");
            return;
        }

        // Load all scripts from database
        auto dbScripts = _database->listScripts(ScriptLanguage::LUA, true);
        NSLog(@"[ScriptBrowserDialog] Loaded %zu scripts from database", dbScripts.size());

        // Convert to ScriptEntry format
        for (const auto& dbScript : dbScripts) {
            ScriptEntry entry(dbScript.name, dbScript.language, false, "");
            entry.contentLength = dbScript.contentLength;
            entry.modifiedAt = dbScript.modifiedAt;
            _scripts.push_back(entry);
        }
    }

    [self applyFilters];
}

- (void)applyFilters {
    NSLog(@"[ScriptBrowserDialog] applyFilters called");
    _filteredScripts.clear();

    NSString* searchText = [self.searchField stringValue];
    NSInteger langIndex = [self.languagePopup indexOfSelectedItem];
    NSLog(@"[ScriptBrowserDialog] Search text: '%@', Language index: %ld", searchText, (long)langIndex);

    for (const auto& script : _scripts) {
        // Language filter
        if (langIndex > 0) {
            ScriptLanguage filterLang = (ScriptLanguage)(langIndex - 1);
            if (script.language != filterLang) {
                continue;
            }
        }

        // Search filter
        if (searchText.length > 0) {
            NSString* scriptName = [NSString stringWithUTF8String:script.name.c_str()];
            if ([scriptName rangeOfString:searchText options:NSCaseInsensitiveSearch].location == NSNotFound) {
                continue;
            }
        }

        _filteredScripts.push_back(script);
    }

    NSLog(@"[ScriptBrowserDialog] After filtering: %zu scripts", _filteredScripts.size());
    [self.tableView reloadData];
    NSLog(@"[ScriptBrowserDialog] Table view reloaded");
}

// =============================================================================
// Modal Execution
// =============================================================================

- (BOOL)runModalWithName:(std::string&)outName
                language:(ScriptLanguage&)outLanguage
            outIsFromCart:(BOOL&)outIsFromCart {
    NSLog(@"[ScriptBrowserDialog] runModalWithName called");
    NSLog(@"[ScriptBrowserDialog] Window: %p, visible: %d", self.window, [self.window isVisible]);
    NSLog(@"[ScriptBrowserDialog] Centering window...");
    [self.window center];

    NSLog(@"[ScriptBrowserDialog] Starting modal session...");
    // Use runModalForWindow
    NSModalResponse response = [NSApp runModalForWindow:self.window];
    NSLog(@"[ScriptBrowserDialog] Modal session ended with response: %ld", (long)response);

    // Just hide window, don't close to avoid autorelease crash
    [self.window orderOut:nil];

    // Handle the response
    if (response == NSModalResponseOK && _selectedRow >= 0 && _selectedRow < _filteredScripts.size()) {
        const auto& script = _filteredScripts[_selectedRow];
        NSLog(@"[ScriptBrowserDialog] User selected script: '%s'", script.name.c_str());

        if (script.isFromCart) {
            // Return the full path for cart scripts
            outName = script.path;
            outIsFromCart = YES;
            NSLog(@"[ScriptBrowserDialog] Selected cart script: '%s'", script.path.c_str());
        } else {
            // Return just the name for database scripts
            outName = script.name;
            outIsFromCart = NO;
            NSLog(@"[ScriptBrowserDialog] Selected database script: '%s'", script.name.c_str());
        }

        outLanguage = script.language;
        return YES;
    }

    NSLog(@"[ScriptBrowserDialog] No valid selection (response=%ld, row=%ld, size=%zu)",
          (long)response, (long)_selectedRow, _filteredScripts.size());
    return NO;
}

// Window delegate method to handle "X" button
- (BOOL)windowShouldClose:(NSWindow *)sender {
    [NSApp stopModalWithCode:NSModalResponseCancel];
    return NO;
}

// =============================================================================
// Actions
// =============================================================================

- (void)openButtonClicked:(id)sender {
    _selectedRow = [self.tableView selectedRow];
    NSLog(@"[ScriptBrowserDialog] Open button clicked, selectedRow: %ld", (long)_selectedRow);
    if (_selectedRow >= 0) {
        NSLog(@"[ScriptBrowserDialog] Stopping modal with OK response");
        [NSApp stopModalWithCode:NSModalResponseOK];
    } else {
        NSLog(@"[ScriptBrowserDialog] No row selected, not stopping modal");
    }
}

- (void)cancelButtonClicked:(id)sender {
    NSLog(@"[ScriptBrowserDialog] Cancel button clicked");
    [NSApp stopModalWithCode:NSModalResponseCancel];
}

- (void)deleteButtonClicked:(id)sender {
    NSInteger row = [self.tableView selectedRow];
    if (row < 0 || row >= _filteredScripts.size()) {
        return;
    }

    // Copy data to stack before running nested modal to avoid dangling reference
    const auto& script = _filteredScripts[row];
    std::string scriptName = script.name;
    ScriptLanguage scriptLang = script.language;
    bool isFromCart = script.isFromCart;

    NSAlert* alert = [[NSAlert alloc] init];
    [alert setMessageText:@"Delete Script"];
    [alert setInformativeText:[NSString stringWithFormat:@"Are you sure you want to delete '%s'?",
                              scriptName.c_str()]];
    [alert addButtonWithTitle:@"Delete"];
    [alert addButtonWithTitle:@"Cancel"];
    [alert setAlertStyle:NSAlertStyleWarning];

    if ([alert runModal] == NSAlertFirstButtonReturn) {
        if (isFromCart) {
            // Cannot delete cart scripts from this dialog
            NSAlert* errorAlert = [[NSAlert alloc] init];
            [errorAlert setMessageText:@"Cannot Delete"];
            [errorAlert setInformativeText:@"Cannot delete scripts from cart. Use cart management tools."];
            [errorAlert addButtonWithTitle:@"OK"];
            [errorAlert runModal];
        } else {
            if (_database->deleteScript(scriptName, scriptLang)) {
                [self loadScripts];
            }
        }
    }
}

- (void)changeTypeToBasic:(id)sender {
    [self changeScriptType:ScriptLanguage::BASIC withExtension:".bas"];
}

- (void)changeTypeToAbc:(id)sender {
    [self changeScriptType:ScriptLanguage::ABC withExtension:".abc"];
}

- (void)changeTypeToVoiceScript:(id)sender {
    [self changeScriptType:ScriptLanguage::VOICESCRIPT withExtension:".vscript"];
}

- (void)changeScriptType:(ScriptLanguage)newLanguage withExtension:(const char*)extension {
    NSInteger row = [self.tableView selectedRow];
    if (row < 0 || row >= _filteredScripts.size()) {
        NSLog(@"[ScriptBrowserDialog] No script selected for type change");
        return;
    }

    const auto& script = _filteredScripts[row];

    // Can't change type of cart scripts
    if (script.isFromCart) {
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Cannot Change Type"];
        [alert setInformativeText:@"Cannot change type of scripts from cart."];
        [alert addButtonWithTitle:@"OK"];
        [alert runModal];
        return;
    }

    // Copy current script data
    std::string oldName = script.name;
    ScriptLanguage oldLanguage = script.language;

    NSLog(@"[ScriptBrowserDialog] Changing '%s' from language %d to %d",
          oldName.c_str(), (int)oldLanguage, (int)newLanguage);

    // Load the script content
    std::string content;
    if (!_database->loadScript(oldName, oldLanguage, content)) {
        NSLog(@"[ScriptBrowserDialog] Failed to load script for type change");
        return;
    }

    // Create new name with new extension
    std::string newName = oldName;

    // Remove old extension
    size_t lastDot = newName.find_last_of('.');
    if (lastDot != std::string::npos) {
        newName = newName.substr(0, lastDot);
    }

    // Add new extension
    newName += extension;

    // Check if new name already exists
    std::string existingContent;
    if (_database->loadScript(newName, newLanguage, existingContent)) {
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Script Already Exists"];
        [alert setInformativeText:[NSString stringWithFormat:@"A script named '%s' with the target type already exists.",
                                   newName.c_str()]];
        [alert addButtonWithTitle:@"OK"];
        [alert runModal];
        return;
    }

    // Save with new name and language
    if (!_database->saveScript(newName, newLanguage, content)) {
        NSLog(@"[ScriptBrowserDialog] Failed to save script with new type");
        return;
    }

    // Delete old script
    if (!_database->deleteScript(oldName, oldLanguage)) {
        NSLog(@"[ScriptBrowserDialog] Warning: Failed to delete old script (new one created)");
    }

    NSLog(@"[ScriptBrowserDialog] Successfully changed type: '%s' (%d) -> '%s' (%d)",
          oldName.c_str(), (int)oldLanguage, newName.c_str(), (int)newLanguage);

    // Reload the script list
    [self loadScripts];

    // Try to select the renamed script
    for (size_t i = 0; i < _filteredScripts.size(); i++) {
        if (_filteredScripts[i].name == newName && _filteredScripts[i].language == newLanguage) {
            [self.tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:i] byExtendingSelection:NO];
            [self.tableView scrollRowToVisible:i];
            break;
        }
    }
}

- (void)tableViewDoubleClicked:(id)sender {
    _selectedRow = [self.tableView clickedRow];
    if (_selectedRow >= 0) {
        [self openButtonClicked:sender];
    }
}

- (void)searchFieldChanged:(id)sender {
    [self applyFilters];
}

- (void)languageFilterChanged:(id)sender {
    [self applyFilters];
}

// =============================================================================
// NSTableViewDataSource
// =============================================================================

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tableView {
    return _filteredScripts.size();
}

- (id)tableView:(NSTableView*)tableView
objectValueForTableColumn:(NSTableColumn*)tableColumn
            row:(NSInteger)row {
    if (row < 0 || row >= _filteredScripts.size()) {
        return @"";
    }

    const auto& script = _filteredScripts[row];
    NSString* identifier = [tableColumn identifier];

    if ([identifier isEqualToString:@"name"]) {
        return [NSString stringWithUTF8String:script.name.c_str()];
    } else if ([identifier isEqualToString:@"language"]) {
        switch (script.language) {
            case ScriptLanguage::LUA: return @"Lua";
            case ScriptLanguage::JAVASCRIPT: return @"JavaScript";
            case ScriptLanguage::BASIC: return @"BASIC";
            case ScriptLanguage::SCHEME: return @"Scheme";
            case ScriptLanguage::ABC: return @"ABC";
            case ScriptLanguage::VOICESCRIPT: return @"VoiceScript";
        }
    } else if ([identifier isEqualToString:@"source"]) {
        return script.isFromCart ? @"Cart" : @"Database";
    } else if ([identifier isEqualToString:@"size"]) {
        if (script.contentLength < 1024) {
            return [NSString stringWithFormat:@"%zu B", script.contentLength];
        } else if (script.contentLength < 1024 * 1024) {
            return [NSString stringWithFormat:@"%.1f KB", script.contentLength / 1024.0];
        } else {
            return [NSString stringWithFormat:@"%.1f MB", script.contentLength / (1024.0 * 1024.0)];
        }
    } else if ([identifier isEqualToString:@"modified"]) {
        NSDate* date = [NSDate dateWithTimeIntervalSince1970:script.modifiedAt];
        NSDateFormatter* formatter = [[NSDateFormatter alloc] init];
        [formatter setDateStyle:NSDateFormatterShortStyle];
        [formatter setTimeStyle:NSDateFormatterShortStyle];
        return [formatter stringFromDate:date];
    }

    return @"";
}

@end

// =============================================================================
// SaveScriptDialog Implementation
// =============================================================================

@interface SaveScriptDialog () <NSWindowDelegate>

@property (strong) NSWindow* window;
@property (strong) NSTextField* nameField;
@property (strong) NSPopUpButton* languagePopup;
@property (strong) NSTextField* descriptionField;
@property (strong) NSButton* saveButton;
@property (strong) NSButton* cancelButton;
@property (assign) std::shared_ptr<ScriptDatabase> database;
@property (assign) BOOL userConfirmed;

@end

@implementation SaveScriptDialog

+ (BOOL)showSaveDialogWithDatabase:(std::shared_ptr<ScriptDatabase>)database
                       defaultName:(const std::string&)defaultName
                   defaultLanguage:(ScriptLanguage)defaultLanguage
                           outName:(std::string&)outName
                       outLanguage:(ScriptLanguage&)outLanguage {
    SaveScriptDialog* dialog = [[SaveScriptDialog alloc] init];
    dialog.database = database;
    [dialog createWindowWithDefaultName:defaultName language:defaultLanguage];

    return [dialog runModalWithName:outName language:outLanguage];
}

- (void)createWindowWithDefaultName:(const std::string&)defaultName
                           language:(ScriptLanguage)defaultLanguage {
    // Create window
    NSRect frame = NSMakeRect(0, 0, 400, 220);
    NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable;

    self.window = [[NSWindow alloc] initWithContentRect:frame
                                              styleMask:style
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    [self.window setTitle:@"Save Script"];
    [self.window setDelegate:self];
    [self.window center];

    NSView* contentView = self.window.contentView;

    // Name label
    NSTextField* nameLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 170, 80, 20)];
    [nameLabel setStringValue:@"Name:"];
    [nameLabel setBezeled:NO];
    [nameLabel setDrawsBackground:NO];
    [nameLabel setEditable:NO];
    [nameLabel setSelectable:NO];
    [contentView addSubview:nameLabel];

    // Name field
    self.nameField = [[NSTextField alloc] initWithFrame:NSMakeRect(100, 168, 280, 24)];
    [self.nameField setStringValue:[NSString stringWithUTF8String:defaultName.c_str()]];
    [contentView addSubview:self.nameField];

    // Language label
    NSTextField* langLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 130, 80, 20)];
    [langLabel setStringValue:@"Language:"];
    [langLabel setBezeled:NO];
    [langLabel setDrawsBackground:NO];
    [langLabel setEditable:NO];
    [langLabel setSelectable:NO];
    [contentView addSubview:langLabel];

    // Language popup
    self.languagePopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(100, 126, 280, 28)];
    [self.languagePopup addItemWithTitle:@"Lua"];
    [self.languagePopup addItemWithTitle:@"JavaScript"];
    [self.languagePopup addItemWithTitle:@"BASIC"];
    [self.languagePopup addItemWithTitle:@"Scheme"];
    [self.languagePopup selectItemAtIndex:(NSInteger)defaultLanguage];
    [contentView addSubview:self.languagePopup];

    // Description label
    NSTextField* descLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 90, 80, 20)];
    [descLabel setStringValue:@"Description:"];
    [descLabel setBezeled:NO];
    [descLabel setDrawsBackground:NO];
    [descLabel setEditable:NO];
    [descLabel setSelectable:NO];
    [contentView addSubview:descLabel];

    // Description field
    self.descriptionField = [[NSTextField alloc] initWithFrame:NSMakeRect(100, 88, 280, 24)];
    [self.descriptionField setPlaceholderString:@"Optional"];
    [contentView addSubview:self.descriptionField];

    // Buttons
    self.cancelButton = [[NSButton alloc] initWithFrame:NSMakeRect(200, 20, 80, 28)];
    [self.cancelButton setTitle:@"Cancel"];
    [self.cancelButton setBezelStyle:NSBezelStyleRounded];
    [self.cancelButton setKeyEquivalent:@"\e"];
    [self.cancelButton setTarget:self];
    [self.cancelButton setAction:@selector(cancelButtonClicked:)];
    [contentView addSubview:self.cancelButton];

    self.saveButton = [[NSButton alloc] initWithFrame:NSMakeRect(300, 20, 80, 28)];
    [self.saveButton setTitle:@"Save"];
    [self.saveButton setBezelStyle:NSBezelStyleRounded];
    [self.saveButton setKeyEquivalent:@"\r"];
    [self.saveButton setTarget:self];
    [self.saveButton setAction:@selector(saveButtonClicked:)];
    [contentView addSubview:self.saveButton];
}

- (BOOL)runModalWithName:(std::string&)outName
                language:(ScriptLanguage&)outLanguage {
    [self.window makeFirstResponder:self.nameField];

    // Use runModalForWindow
    NSModalResponse response = [NSApp runModalForWindow:self.window];

    BOOL success = NO;

    if (response == NSModalResponseOK) {
        // Read values BEFORE hiding window
        NSString* name = [self.nameField stringValue];
        if (name.length > 0) {
            outName = std::string([name UTF8String]);
            outLanguage = (ScriptLanguage)[self.languagePopup indexOfSelectedItem];
            success = YES;
        }
    }

    // Just hide window, don't close to avoid autorelease crash
    [self.window orderOut:nil];

    return success;
}

// Window delegate method to handle "X" button
- (BOOL)windowShouldClose:(NSWindow *)sender {
    [NSApp stopModalWithCode:NSModalResponseCancel];
    return NO;
}

- (void)saveButtonClicked:(id)sender {
    NSString* name = [self.nameField stringValue];

    if (name.length == 0) {
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Invalid Name"];
        [alert setInformativeText:@"Please enter a script name."];
        [alert addButtonWithTitle:@"OK"];
        [alert runModal];
        return;
    }

    // Check if script already exists
    std::string scriptName([name UTF8String]);
    ScriptLanguage lang = (ScriptLanguage)[self.languagePopup indexOfSelectedItem];

    if (_database && _database->scriptExists(scriptName, lang)) {
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Script Exists"];
        [alert setInformativeText:@"A script with this name and language already exists. Overwrite?"];
        [alert addButtonWithTitle:@"Overwrite"];
        [alert addButtonWithTitle:@"Cancel"];
        [alert setAlertStyle:NSAlertStyleWarning];

        if ([alert runModal] != NSAlertFirstButtonReturn) {
            return;
        }
    }

    [NSApp stopModalWithCode:NSModalResponseOK];
}

- (void)cancelButtonClicked:(id)sender {
    [NSApp stopModalWithCode:NSModalResponseCancel];
}

@end

// =============================================================================
// UnsavedChangesDialog Implementation
// =============================================================================

@implementation UnsavedChangesDialog

+ (UnsavedChangesAction)showAlertForScript:(const std::string&)scriptName {
    NSAlert* alert = [[NSAlert alloc] init];
    [alert setMessageText:@"Unsaved Changes"];

    NSString* message;
    if (scriptName.empty() || scriptName == "untitled") {
        message = @"Do you want to save the changes to this script?";
    } else {
        message = [NSString stringWithFormat:@"Do you want to save the changes to '%s'?",
                  scriptName.c_str()];
    }
    [alert setInformativeText:message];

    [alert addButtonWithTitle:@"Save"];
    [alert addButtonWithTitle:@"Don't Save"];
    [alert addButtonWithTitle:@"Cancel"];
    [alert setAlertStyle:NSAlertStyleWarning];

    NSModalResponse response = [alert runModal];

    switch (response) {
        case NSAlertFirstButtonReturn:
            return UnsavedChangesActionSave;
        case NSAlertSecondButtonReturn:
            return UnsavedChangesActionDiscard;
        default:
            return UnsavedChangesActionCancel;
    }
}

@end
