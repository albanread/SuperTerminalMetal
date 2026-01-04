//
//  ScriptBrowserDialog.h
//  SuperTerminal Framework - Script Browser Dialog
//
//  Native macOS dialog for browsing and opening scripts from database
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#include <string>
#include <memory>
#include "Editor/ScriptDatabase.h"
#include "Cart/CartManager.h"

// =============================================================================
// ScriptBrowserDialog - Native macOS script browser
// =============================================================================
//
// Features:
// - Browse scripts from database
// - Filter by language
// - Search by name
// - Show metadata (size, modified date)
// - Delete scripts
// - Double-click to open
//

@interface ScriptBrowserDialog : NSObject <NSTableViewDataSource, NSTableViewDelegate>

// =========================================================================
// Class Methods
// =========================================================================

/// Show script browser dialog modally
/// @param database Script database to browse
/// @param language Default language filter (or ScriptLanguage::LUA for all)
/// @param cartManager Cart manager (optional, for listing cart scripts)
/// @param outName Output: selected script name
/// @param outLanguage Output: selected script language
/// @param outIsFromCart Output: true if script is from cart
/// @return YES if user selected a script, NO if cancelled
+ (BOOL)showBrowserWithDatabase:(std::shared_ptr<SuperTerminal::ScriptDatabase>)database
                       language:(SuperTerminal::ScriptLanguage)language
                    cartManager:(std::shared_ptr<SuperTerminal::CartManager>)cartManager
                        outName:(std::string&)outName
                    outLanguage:(SuperTerminal::ScriptLanguage&)outLanguage
                   outIsFromCart:(BOOL&)outIsFromCart;

// =========================================================================
// Instance Methods
// =========================================================================

/// Initialize dialog
/// @param database Script database to browse
/// @param language Default language filter
/// @param cartManager Cart manager (optional)
- (instancetype)initWithDatabase:(std::shared_ptr<SuperTerminal::ScriptDatabase>)database
                        language:(SuperTerminal::ScriptLanguage)language
                     cartManager:(std::shared_ptr<SuperTerminal::CartManager>)cartManager;

/// Show dialog modally
/// @param outName Output: selected script name
/// @param outLanguage Output: selected script language
/// @param outIsFromCart Output: true if script is from cart
/// @return YES if user selected a script, NO if cancelled
- (BOOL)runModalWithName:(std::string&)outName
                language:(SuperTerminal::ScriptLanguage&)outLanguage
            outIsFromCart:(BOOL&)outIsFromCart;

@end

// =============================================================================
// SaveScriptDialog - Save script with name and language
// =============================================================================

@interface SaveScriptDialog : NSObject

/// Show save dialog modally
/// @param database Script database (for checking existing names)
/// @param defaultName Default script name
/// @param defaultLanguage Default language
/// @param outName Output: entered script name
/// @param outLanguage Output: selected language
/// @return YES if user confirmed, NO if cancelled
+ (BOOL)showSaveDialogWithDatabase:(std::shared_ptr<SuperTerminal::ScriptDatabase>)database
                       defaultName:(const std::string&)defaultName
                   defaultLanguage:(SuperTerminal::ScriptLanguage)defaultLanguage
                           outName:(std::string&)outName
                       outLanguage:(SuperTerminal::ScriptLanguage&)outLanguage;

@end

// =============================================================================
// UnsavedChangesDialog - Confirm before losing unsaved changes
// =============================================================================

@interface UnsavedChangesDialog : NSObject

typedef NS_ENUM(NSInteger, UnsavedChangesAction) {
    UnsavedChangesActionSave,       // Save changes
    UnsavedChangesActionDiscard,    // Discard changes
    UnsavedChangesActionCancel      // Cancel operation
};

/// Show unsaved changes alert
/// @param scriptName Name of script with unsaved changes
/// @return Action to take
+ (UnsavedChangesAction)showAlertForScript:(const std::string&)scriptName;

@end
