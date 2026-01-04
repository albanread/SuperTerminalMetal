//
// BaseRunner.h
// SuperTerminal Framework - Base Runner Application Class
//
// Provides common functionality for all runner applications including:
// - Menu bar with standard menus (Application, File, Edit)
// - Quit menu item and keyboard shortcuts
// - Framework initialization
// - Window management
// - Render loop setup
//
// Subclasses should override:
// - initializeRuntime() - Initialize the specific language runtime
// - loadAndExecuteScript() - Load and run the script
// - cleanupRuntime() - Clean up runtime resources
//

#pragma once

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#import <CoreVideo/CoreVideo.h>
#include <memory>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include "Display/DisplayManager.h"
#include "Display/TextGrid.h"
#include "Display/GraphicsLayer.h"
#include "Display/SpriteManager.h"
#include "Audio/AudioManager.h"
#include "Input/InputManager.h"
#include "Metal/MetalRenderer.h"
#include "Metal/FontAtlas.h"
#include "API/st_api_context.h"
#include "API/superterminal_api.h"
#include "Editor/TextEditor.h"
#include "Editor/ScriptDatabase.h"
#include "Editor/Document.h"
#include "Editor/ExportImportManager.h"
#include "Editor/ScreenState.h"
#include "Editor/ScreenMode.h"
#include "Startup/AppStartupStateMachine.h"
#include "Cart/CartManager.h"

@class EditorStatusBar;

// Forward declare namespace types
namespace SuperTerminal {
    class DisplayManager;
    class TextGrid;
    class GraphicsLayer;
    class SpriteManager;
    class MetalRenderer;
    class FontAtlas;
    class InputManager;
    class AudioManager;
    class TextEditor;
    class ScriptDatabase;
    class Document;
    class ScreenStateManager;
    class ScreenModeManager;
    class AppStartupStateMachine;
    class CartManager;
}

// =============================================================================
// BaseRunner - Abstract Base Class for Runner Applications
// =============================================================================
//
// Thread Safety:
// - All public methods are thread-safe unless otherwise documented
// - Framework components are accessed via thread-safe accessors
// - Frame synchronization (waitForNextFrame) is thread-safe
//

@interface LuaBaseRunner : NSObject <NSApplicationDelegate>

@property (strong) NSWindow* window;
@property (assign) std::string scriptPath;
@property (assign) std::string runnerName;
@property (assign) BOOL headless;  // Don't show window (batch mode)

// Framework components - available to subclasses
@property (readonly) std::shared_ptr<SuperTerminal::DisplayManager> displayManager;
@property (readonly) std::shared_ptr<SuperTerminal::TextGrid> textGrid;
@property (readonly) std::shared_ptr<SuperTerminal::GraphicsLayer> graphicsLayer;
@property (readonly) std::shared_ptr<SuperTerminal::SpriteManager> spriteManager;
@property (readonly) std::shared_ptr<SuperTerminal::MetalRenderer> renderer;
@property (readonly) std::shared_ptr<SuperTerminal::FontAtlas> fontAtlas;
@property (readonly) std::shared_ptr<SuperTerminal::InputManager> inputManager;
@property (readonly) std::shared_ptr<SuperTerminal::AudioManager> audioManager;
@property (readonly) std::shared_ptr<SuperTerminal::TextDisplayManager> textDisplayManager;

// Cart manager
@property (readonly) std::shared_ptr<SuperTerminal::CartManager> cartManager;

// Editor components
@property (readonly) std::shared_ptr<SuperTerminal::TextEditor> textEditor;
@property (readonly) std::shared_ptr<SuperTerminal::ScriptDatabase> scriptDatabase;
@property (readonly) std::shared_ptr<SuperTerminal::Document> currentDocument;
@property (readonly) std::shared_ptr<SuperTerminal::ScreenStateManager> screenStateManager;
@property (readonly) std::shared_ptr<SuperTerminal::ScreenModeManager> screenModeManager;
@property (readonly) std::shared_ptr<SuperTerminal::ExportImportManager> exportImportManager;
@property (strong) EditorStatusBar* editorStatusBar;

// Editor state
@property (assign) BOOL editorMode;           // YES = editor visible, NO = runtime only
@property (assign) std::string currentScriptLanguage;
// Note: Current script name is stored in textEditor->getFilename()

// Runtime state (thread-safe)
@property (readonly) std::atomic<bool>& running;
@property (readonly) uint64_t frameCount;
@property (readonly) double lastFrameTime;

// Frame synchronization - Call from script thread to wait for next frame
// Thread-safe: Can be called from any thread
- (void)waitForNextFrame;

// Signal next frame is ready - Called internally by render loop
// Thread-safe: Called from render thread
- (void)signalFrameReady;

// Initialization (called automatically by base class)
- (instancetype)initWithScriptPath:(const std::string&)scriptPath
                        runnerName:(const std::string&)runnerName;

// Menu bar setup (called automatically)
- (void)setupMenuBar;

// Framework initialization (called automatically)
- (BOOL)initializeFramework;
- (BOOL)initializeFrameworkWithConfig:(SuperTerminal::DisplayConfig)config;

// Render loop (called automatically)
- (void)startRenderLoop;
- (void)onFrameTick;

// Abstract methods - subclasses MUST override these
// Thread Safety: These are called on the script thread
- (BOOL)initializeRuntime;
- (BOOL)loadAndExecuteScript;
- (void)cleanupRuntime;

// Utility methods
- (void)terminate;
- (void)showError:(NSString*)message;

// =========================================================================
// Editor Support
// =========================================================================

// Initialize editor subsystem
- (BOOL)initializeEditor;

// Shutdown editor subsystem
- (void)shutdownEditor;

// Update editor (call each frame when in editor mode)
- (void)updateEditor:(double)deltaTime;

// Render editor (call each frame when in editor mode)
- (void)renderEditor;

// Toggle between editor and runtime mode
- (void)toggleEditorMode;

// Enter editor mode
- (void)enterEditorMode;

// Enter runtime mode
- (void)enterRuntimeMode;

// =========================================================================
// Window Size Management
// =========================================================================

// Change window size (these will recreate the window)
- (void)setWindowSizeSmall:(id)sender;
- (void)setWindowSizeMedium:(id)sender;
- (void)setWindowSizeLarge:(id)sender;
- (void)setWindowSizeFullHD:(id)sender;

// =========================================================================
// Script Management (Editor Actions)
// =========================================================================

// Create new script
- (void)newScript;
- (void)newBasicScript;
- (void)newAbcScript;
- (void)newVoiceScript;

// Open script from database
- (void)openScript;

// Save current script
- (void)saveScript;

// Save current script with new name
- (void)saveScriptAs;

// Close current script
- (void)closeScript;

// Execute current file (smart run/play based on file type)
- (void)executeCurrentFile;

// Play current file (music files: ABC, VoiceScript, SID)
- (void)playCurrentFile;

// Export content (cart or scripts database)
- (void)exportContent;

// Import content (cart or scripts database)
- (void)importContent;

// Format current script (BASIC only)
- (void)formatScript;

// Format current script with options (BASIC only)
- (void)formatScriptWithStartLine:(int)startLine step:(int)step;

// Renumber current script (BASIC only)
- (void)renumberScript;

// Renumber current script with options (BASIC only)
- (void)renumberScriptWithStartLine:(int)startLine step:(int)step;

// Run current script
- (void)runScript;

// Stop running script
- (void)stopScript;

// Clear output/runtime display
- (void)clearOutput;

// =========================================================================
// Clipboard Support
// =========================================================================

// Cut selected text to clipboard
- (void)cutText;

// Copy selected text to clipboard
- (void)copyText;

// Paste text from clipboard
- (void)pasteText;

// =========================================================================
// Syntax Highlighting (Override in Language Runners)
// =========================================================================

// Get syntax highlighter for current language
// Subclasses should override to provide language-specific highlighting
- (std::vector<uint32_t>)highlightLine:(const std::string&)line
                            lineNumber:(size_t)lineNumber;

// =========================================================================
// Interactive View Mouse Handling (Override in Subclass)
// =========================================================================

// Handle mouse click in interactive view (when not in editor mode)
- (void)handleInteractiveMouseClick:(int)gridX y:(int)gridY button:(int)button;

// Handle mouse release in interactive view
- (void)handleInteractiveMouseRelease;

@end