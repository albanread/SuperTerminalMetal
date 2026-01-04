//
// BaseRunner.mm
// SuperTerminal Framework - Base Runner Application Class Implementation
//
// Provides common functionality for all runner applications.
//

#import "LuaBaseRunner.h"
#include "Debug/Logger.h"
#include "Particles/ParticleSystem.h"
#include "Data/PaletteLibrary.h"
#include "Display/RectangleManager.h"
#include "Display/CircleManager.h"
#include "Display/LineManager.h"
#include "Display/PolygonManager.h"
#include "Display/StarManager.h"
#include "Display/TextDisplayManager.h"
#include "Tilemap/TilemapManager.h"
#include "Tilemap/TilemapRenderer.h"
#include "Editor/TextEditor.h"
#include "Editor/ScriptDatabase.h"
#include "Editor/ScreenState.h"
#include "Editor/ScreenMode.h"
#include "Editor/EditorStatusBar.h"
#include "Editor/InputHandler.h"
#include "Editor/ScriptBrowserDialog.h"
#include "Editor/FindDialogs.h"
#include "Editor/Cursor.h"
#include "Editor/TextBuffer.h"
#include "Editor/EditorAPI.h"
#include "Editor/LuaFormatterWrapper.h"
#include "API/superterminal_api.h"
#include <algorithm>

using namespace SuperTerminal;

// =============================================================================
// BaseRunner Implementation
// =============================================================================

@implementation LuaBaseRunner {
    // Startup state machine - controls initialization sequence
    std::shared_ptr<AppStartupStateMachine> _startupStateMachine;

    std::shared_ptr<DisplayManager> _displayManager;
    std::shared_ptr<TextGrid> _textGrid;
    std::shared_ptr<GraphicsLayer> _graphicsLayer;
    std::shared_ptr<SpriteManager> _spriteManager;
    std::shared_ptr<RectangleManager> _rectangleManager;
    std::shared_ptr<CircleManager> _circleManager;
    std::shared_ptr<LineManager> _lineManager;
    std::shared_ptr<PolygonManager> _polygonManager;
    std::shared_ptr<StarManager> _starManager;
    std::shared_ptr<TilemapManager> _tilemapManager;
    std::shared_ptr<TilemapRenderer> _tilemapRenderer;
    std::shared_ptr<MetalRenderer> _renderer;
    std::shared_ptr<FontAtlas> _fontAtlas;
    std::shared_ptr<InputManager> _inputManager;
    std::shared_ptr<AudioManager> _audioManager;
    std::shared_ptr<TextDisplayManager> _textDisplayManager;

    // Editor components
    std::shared_ptr<TextEditor> _textEditor;
    std::shared_ptr<ScriptDatabase> _scriptDatabase;
    std::shared_ptr<Document> _currentDocument;
    std::shared_ptr<ScreenStateManager> _screenStateManager;
    std::shared_ptr<ScreenModeManager> _screenModeManager;

    std::atomic<bool> _running;
    std::thread _scriptThread;
    std::atomic<double> _lastFrameTime;
    std::atomic<uint64_t> _frameCount;

    CVDisplayLinkRef _displayLink;

    // Frame synchronization for script threads
    std::mutex _frameMutex;
    std::condition_variable _frameCV;
    std::atomic<bool> _frameReady;
}

// Synthesize readonly properties
@synthesize displayManager = _displayManager;
@synthesize textGrid = _textGrid;
@synthesize graphicsLayer = _graphicsLayer;
@synthesize spriteManager = _spriteManager;
@synthesize renderer = _renderer;
@synthesize fontAtlas = _fontAtlas;
@synthesize inputManager = _inputManager;
@synthesize audioManager = _audioManager;
@synthesize textDisplayManager = _textDisplayManager;
@synthesize cartManager = _cartManager;

// Editor properties
@synthesize textEditor = _textEditor;
@synthesize scriptDatabase = _scriptDatabase;
@synthesize currentDocument = _currentDocument;
@synthesize screenStateManager = _screenStateManager;
@synthesize screenModeManager = _screenModeManager;
@synthesize exportImportManager = _exportImportManager;
@synthesize editorStatusBar;
@synthesize editorMode;
@synthesize currentScriptLanguage;

- (std::atomic<bool>&)running {
    return _running;
}

- (uint64_t)frameCount {
    return _frameCount;
}

- (double)lastFrameTime {
    return _lastFrameTime;
}

// =============================================================================
// Initialization
// =============================================================================

- (instancetype)initWithScriptPath:(const std::string&)scriptPath
                        runnerName:(const std::string&)runnerName {
    self = [super init];
    if (self) {
        // Create startup state machine first
        _startupStateMachine = std::make_shared<AppStartupStateMachine>();

        self.scriptPath = scriptPath;
        self.runnerName = runnerName;
        _running = false;
        _frameCount = 0;
        _lastFrameTime = 0.0;
        _displayLink = nullptr;
        _frameReady = false;

        // Editor defaults
        self.editorMode = NO;
        self.currentScriptLanguage = "text";

        LOG_INFOF("Initialized with script: %s", scriptPath.c_str());
    }
    return self;
}

// =============================================================================
// Frame Synchronization
// =============================================================================

- (void)waitForNextFrame {
    std::unique_lock<std::mutex> lock(_frameMutex);
    // Wait for frame signal with timeout to prevent deadlock on shutdown
    _frameCV.wait_for(lock, std::chrono::milliseconds(100), [self]() {
        return _frameReady.load() || !_running.load();
    });
    _frameReady = false;
}

- (void)signalFrameReady {
    {
        std::lock_guard<std::mutex> lock(_frameMutex);
        _frameReady = true;
    }
    _frameCV.notify_one();
}

// =============================================================================
// Menu Bar Setup
// =============================================================================

- (void)setupMenuBar {
    NSMenu* mainMenu = [[NSMenu alloc] init];

    // Application Menu
    NSMenuItem* appMenuItem = [[NSMenuItem alloc] init];
    [mainMenu addItem:appMenuItem];

    NSMenu* appMenu = [[NSMenu alloc] init];
    [appMenuItem setSubmenu:appMenu];

    NSString* appName = [NSString stringWithUTF8String:self.runnerName.c_str()];

    // About item
    NSString* aboutTitle = [NSString stringWithFormat:@"About %@", appName];
    NSMenuItem* aboutItem = [[NSMenuItem alloc] initWithTitle:aboutTitle
                                                       action:@selector(orderFrontStandardAboutPanel:)
                                                keyEquivalent:@""];
    [appMenu addItem:aboutItem];

    [appMenu addItem:[NSMenuItem separatorItem]];

    // Hide item
    NSString* hideTitle = [NSString stringWithFormat:@"Hide %@", appName];
    NSMenuItem* hideItem = [[NSMenuItem alloc] initWithTitle:hideTitle
                                                      action:@selector(hide:)
                                               keyEquivalent:@"h"];
    [appMenu addItem:hideItem];

    // Hide Others
    NSMenuItem* hideOthersItem = [[NSMenuItem alloc] initWithTitle:@"Hide Others"
                                                            action:@selector(hideOtherApplications:)
                                                     keyEquivalent:@"h"];
    [hideOthersItem setKeyEquivalentModifierMask:NSEventModifierFlagOption | NSEventModifierFlagCommand];
    [appMenu addItem:hideOthersItem];

    // Show All
    NSMenuItem* showAllItem = [[NSMenuItem alloc] initWithTitle:@"Show All"
                                                         action:@selector(unhideAllApplications:)
                                                  keyEquivalent:@""];
    [appMenu addItem:showAllItem];

    [appMenu addItem:[NSMenuItem separatorItem]];

    // Quit item
    NSString* quitTitle = [NSString stringWithFormat:@"Quit %@", appName];
    NSMenuItem* quitItem = [[NSMenuItem alloc] initWithTitle:quitTitle
                                                      action:@selector(terminate)
                                               keyEquivalent:@"q"];
    [quitItem setTarget:self];
    [appMenu addItem:quitItem];

    // File Menu
    NSMenuItem* fileMenuItem = [[NSMenuItem alloc] init];
    [mainMenu addItem:fileMenuItem];

    NSMenu* fileMenu = [[NSMenu alloc] initWithTitle:@"File"];
    [fileMenuItem setSubmenu:fileMenu];

    // New Script submenu
    NSMenuItem* newItem = [[NSMenuItem alloc] initWithTitle:@"New Script" action:nil keyEquivalent:@"n"];
    NSMenu* newSubmenu = [[NSMenu alloc] initWithTitle:@"New Script"];
    [newItem setSubmenu:newSubmenu];

    NSMenuItem* newLuaItem = [[NSMenuItem alloc] initWithTitle:@"Lua"
                                                        action:@selector(newLuaScript)
                                                 keyEquivalent:@""];
    [newLuaItem setTarget:self];
    [newSubmenu addItem:newLuaItem];

    NSMenuItem* newBasicItem = [[NSMenuItem alloc] initWithTitle:@"BASIC"
                                                          action:@selector(newBasicScript)
                                                   keyEquivalent:@""];
    [newBasicItem setTarget:self];
    [newSubmenu addItem:newBasicItem];

    NSMenuItem* newAbcItem = [[NSMenuItem alloc] initWithTitle:@"ABC Notation"
                                                        action:@selector(newAbcScript)
                                                 keyEquivalent:@""];
    [newAbcItem setTarget:self];
    [newSubmenu addItem:newAbcItem];

    NSMenuItem* newVoiceScriptItem = [[NSMenuItem alloc] initWithTitle:@"VoiceScript"
                                                                action:@selector(newVoiceScript)
                                                         keyEquivalent:@""];
    [newVoiceScriptItem setTarget:self];
    [newSubmenu addItem:newVoiceScriptItem];

    [fileMenu addItem:newItem];

    NSMenuItem* openItem = [[NSMenuItem alloc] initWithTitle:@"Open Script..."
                                                      action:@selector(openScript)
                                               keyEquivalent:@"o"];
    [openItem setTarget:self];
    [fileMenu addItem:openItem];

    [fileMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* saveItem = [[NSMenuItem alloc] initWithTitle:@"Save"
                                                      action:@selector(saveScript)
                                               keyEquivalent:@"s"];
    [saveItem setTarget:self];
    [fileMenu addItem:saveItem];

    NSMenuItem* saveAsItem = [[NSMenuItem alloc] initWithTitle:@"Save As..."
                                                        action:@selector(saveScriptAs)
                                                 keyEquivalent:@"S"];
    [saveAsItem setTarget:self];
    [fileMenu addItem:saveAsItem];

    [fileMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* closeItem = [[NSMenuItem alloc] initWithTitle:@"Close Script"
                                                       action:@selector(closeScript)
                                                keyEquivalent:@"w"];
    [closeItem setTarget:self];
    [fileMenu addItem:closeItem];

    [fileMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* exportItem = [[NSMenuItem alloc] initWithTitle:@"Export..."
                                                        action:@selector(exportContent)
                                                 keyEquivalent:@""];
    [exportItem setTarget:self];
    [fileMenu addItem:exportItem];

    NSMenuItem* importItem = [[NSMenuItem alloc] initWithTitle:@"Import..."
                                                        action:@selector(importContent)
                                                 keyEquivalent:@""];
    [importItem setTarget:self];
    [fileMenu addItem:importItem];

    // Edit Menu
    NSMenuItem* editMenuItem = [[NSMenuItem alloc] init];
    [mainMenu addItem:editMenuItem];

    NSMenu* editMenu = [[NSMenu alloc] initWithTitle:@"Edit"];
    [editMenuItem setSubmenu:editMenu];

    NSMenuItem* undoItem = [[NSMenuItem alloc] initWithTitle:@"Undo"
                                                      action:@selector(undo:)
                                               keyEquivalent:@"z"];
    [editMenu addItem:undoItem];

    NSMenuItem* redoItem = [[NSMenuItem alloc] initWithTitle:@"Redo"
                                                      action:@selector(redo:)
                                               keyEquivalent:@"Z"];
    [editMenu addItem:redoItem];

    [editMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* cutItem = [[NSMenuItem alloc] initWithTitle:@"Cut"
                                                     action:@selector(cutText)
                                              keyEquivalent:@"x"];
    [cutItem setTarget:self];
    [editMenu addItem:cutItem];

    NSMenuItem* copyItem = [[NSMenuItem alloc] initWithTitle:@"Copy"
                                                      action:@selector(copyText)
                                               keyEquivalent:@"c"];
    [copyItem setTarget:self];
    [editMenu addItem:copyItem];

    NSMenuItem* pasteItem = [[NSMenuItem alloc] initWithTitle:@"Paste"
                                                       action:@selector(pasteText)
                                                keyEquivalent:@"v"];
    [pasteItem setTarget:self];
    [editMenu addItem:pasteItem];

    [editMenu addItem:[NSMenuItem separatorItem]];

    [editMenu addItemWithTitle:@"Select All" action:@selector(selectAll:) keyEquivalent:@"a"];

    [editMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* findItem = [[NSMenuItem alloc] initWithTitle:@"Find..."
                                                      action:@selector(showFind:)
                                               keyEquivalent:@"f"];
    [findItem setTarget:self];
    [editMenu addItem:findItem];

    NSMenuItem* findNextItem = [[NSMenuItem alloc] initWithTitle:@"Find Next"
                                                          action:@selector(findNext:)
                                                   keyEquivalent:@"g"];
    [findNextItem setTarget:self];
    [editMenu addItem:findNextItem];

    NSMenuItem* findReplaceItem = [[NSMenuItem alloc] initWithTitle:@"Find and Replace..."
                                                             action:@selector(showFindReplace:)
                                                      keyEquivalent:@"f"];
    [findReplaceItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagOption];
    [findReplaceItem setTarget:self];
    [editMenu addItem:findReplaceItem];

    NSMenuItem* gotoLineItem = [[NSMenuItem alloc] initWithTitle:@"Go to Line..."
                                                          action:@selector(showGotoLine:)
                                                   keyEquivalent:@"l"];
    [gotoLineItem setTarget:self];
    [editMenu addItem:gotoLineItem];

    [editMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* formatItem = [[NSMenuItem alloc] initWithTitle:@"Format (Lua)"
                                                        action:@selector(formatScript)
                                                 keyEquivalent:@"F"];
    [formatItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagShift];
    [formatItem setTarget:self];
    [editMenu addItem:formatItem];

    // Run Menu
    NSMenuItem* runMenuItem = [[NSMenuItem alloc] init];
    [mainMenu addItem:runMenuItem];

    NSMenu* runMenu = [[NSMenu alloc] initWithTitle:@"Run"];
    [runMenuItem setSubmenu:runMenu];

    NSMenuItem* executeItem = [[NSMenuItem alloc] initWithTitle:@"Run/Play"
                                                         action:@selector(executeCurrentFile)
                                                  keyEquivalent:@"r"];
    [executeItem setTarget:self];
    [runMenu addItem:executeItem];

    [runMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* runItem = [[NSMenuItem alloc] initWithTitle:@"Run Lua Script"
                                                     action:@selector(runScript)
                                              keyEquivalent:@"R"];
    [runItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagShift];
    [runItem setTarget:self];
    [runMenu addItem:runItem];

    NSMenuItem* playItem = [[NSMenuItem alloc] initWithTitle:@"Play Music File"
                                                      action:@selector(playCurrentFile)
                                               keyEquivalent:@"P"];
    [playItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagShift];
    [playItem setTarget:self];
    [runMenu addItem:playItem];

    [runMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* stopItem = [[NSMenuItem alloc] initWithTitle:@"Stop Script"
                                                      action:@selector(stopScript)
                                               keyEquivalent:@"."];
    [stopItem setTarget:self];
    [runMenu addItem:stopItem];

    [runMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* clearItem = [[NSMenuItem alloc] initWithTitle:@"Clear Output"
                                                       action:@selector(clearOutput)
                                                keyEquivalent:@"k"];
    [clearItem setTarget:self];
    [runMenu addItem:clearItem];

    // View Menu
    NSMenuItem* viewMenuItem = [[NSMenuItem alloc] init];
    [mainMenu addItem:viewMenuItem];

    NSMenu* viewMenu = [[NSMenu alloc] initWithTitle:@"View"];
    [viewMenuItem setSubmenu:viewMenu];

    NSMenuItem* editorModeItem = [[NSMenuItem alloc] initWithTitle:@"Editor Mode"
                                                            action:@selector(enterEditorMode)
                                                     keyEquivalent:@"1"];
    [editorModeItem setTarget:self];
    [viewMenu addItem:editorModeItem];

    NSMenuItem* runtimeModeItem = [[NSMenuItem alloc] initWithTitle:@"Runtime Mode"
                                                             action:@selector(enterRuntimeMode)
                                                      keyEquivalent:@"2"];
    [runtimeModeItem setTarget:self];
    [viewMenu addItem:runtimeModeItem];

    [viewMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* toggleModeItem = [[NSMenuItem alloc] initWithTitle:@"Toggle Mode"
                                                            action:@selector(toggleEditorMode)
                                                     keyEquivalent:@"e"];
    [toggleModeItem setTarget:self];
    [viewMenu addItem:toggleModeItem];

    [viewMenu addItem:[NSMenuItem separatorItem]];

    // Window Size submenu
    NSMenuItem* windowSizeMenuItem = [[NSMenuItem alloc] initWithTitle:@"Window Size" action:nil keyEquivalent:@""];
    NSMenu* windowSizeMenu = [[NSMenu alloc] initWithTitle:@"Window Size"];
    [windowSizeMenuItem setSubmenu:windowSizeMenu];
    [viewMenu addItem:windowSizeMenuItem];

    NSMenuItem* smallWindowItem = [[NSMenuItem alloc] initWithTitle:@"Small (640×400)"
                                                             action:@selector(setWindowSizeSmall:)
                                                      keyEquivalent:@""];
    [smallWindowItem setTarget:self];
    [windowSizeMenu addItem:smallWindowItem];

    NSMenuItem* mediumWindowItem = [[NSMenuItem alloc] initWithTitle:@"Medium (800×600)"
                                                              action:@selector(setWindowSizeMedium:)
                                                       keyEquivalent:@""];
    [mediumWindowItem setTarget:self];
    [windowSizeMenu addItem:mediumWindowItem];

    NSMenuItem* largeWindowItem = [[NSMenuItem alloc] initWithTitle:@"Large (1280×720)"
                                                             action:@selector(setWindowSizeLarge:)
                                                      keyEquivalent:@""];
    [largeWindowItem setTarget:self];
    [windowSizeMenu addItem:largeWindowItem];

    NSMenuItem* fullHDWindowItem = [[NSMenuItem alloc] initWithTitle:@"Full HD (1920×1080)"
                                                              action:@selector(setWindowSizeFullHD:)
                                                       keyEquivalent:@""];
    [fullHDWindowItem setTarget:self];
    [windowSizeMenu addItem:fullHDWindowItem];

    // Window Menu
    NSMenuItem* windowMenuItem = [[NSMenuItem alloc] init];
    [mainMenu addItem:windowMenuItem];

    NSMenu* windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];
    [windowMenuItem setSubmenu:windowMenu];

    [windowMenu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
    [windowMenu addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];
    [windowMenu addItem:[NSMenuItem separatorItem]];
    [windowMenu addItemWithTitle:@"Bring All to Front" action:@selector(arrangeInFront:) keyEquivalent:@""];

    [NSApp setMainMenu:mainMenu];

    LOG_INFO("Menu bar configured with Quit menu (Cmd+Q)");
}

// =============================================================================
// Framework Initialization
// =============================================================================

- (BOOL)initializeFramework {
    DisplayConfig config;
    config.windowWidth = 800;
    config.windowHeight = 600;
    config.cellWidth = 8;
    config.cellHeight = 16;
    config.fullscreen = false;
    config.vsync = true;
    config.targetFPS = 60.0f;
    config.windowTitle = self.runnerName.c_str();

    return [self initializeFrameworkWithConfig:config];
}

- (BOOL)initializeFrameworkWithConfig:(DisplayConfig)config {
    // Initialize logger first so all subsequent operations can use it
    LoggerConfig logConfig;
    logConfig.minLevel = LogLevel::INFO;
    logConfig.output = LogOutput::STDERR;
    logConfig.includeTimestamp = true;
    logConfig.includeThreadId = false;
    logConfig.flushImmediately = true;

    if (!Logger::instance().initialize(logConfig)) {
        NSLog(@"[LuaBaseRunner] ERROR: Failed to initialize logger");
        // Continue anyway, logger will use defaults
    }

    LOG_INFO("=== Framework Initialization Started ===");
    LOG_INFOF("Window size: %dx%d", config.windowWidth, config.windowHeight);
    LOG_INFOF("Cell size: %dx%d", config.cellWidth, config.cellHeight);
    LOG_INFOF("VSync: %s, Target FPS: %.1f", config.vsync ? "ON" : "OFF", config.targetFPS);

    // Initialize Standard Palette Library for SPRTZ v2 support
    LOG_INFO("Initializing StandardPaletteLibrary...");
    NSBundle* mainBundle = [NSBundle mainBundle];
    NSString* palettePath = [mainBundle pathForResource:@"standard_palettes" ofType:@"json"];
    if (palettePath) {
        std::string palettePathStr = [palettePath UTF8String];
        if (SuperTerminal::StandardPaletteLibrary::initialize(palettePathStr)) {
            LOG_INFO("✓ StandardPaletteLibrary initialized");
        } else {
            LOG_WARNING("Failed to initialize StandardPaletteLibrary - SPRTZ v2 with standard palettes may not load");
            LOG_WARNINGF("Error: %s", SuperTerminal::StandardPaletteLibrary::getLastError().c_str());
        }
    } else {
        LOG_WARNING("standard_palettes.json not found in bundle - SPRTZ v2 with standard palettes may not load");
    }

    // STATE: CREATING_WINDOW
    if (!_startupStateMachine->transitionTo(StartupState::CREATING_WINDOW, "Creating DisplayManager window")) {
        LOG_ERROR("Invalid state transition to CREATING_WINDOW");
        return NO;
    }

    // Create display manager
    LOG_INFO("Creating DisplayManager...");
    _displayManager = std::make_shared<DisplayManager>();

    // Set state machine reference in DisplayManager
    _displayManager->setStartupStateMachine(_startupStateMachine);

    // Initialize display (creates window but keeps it hidden initially)
    LOG_INFO("Initializing DisplayManager...");
    if (!_displayManager->initialize(config)) {
        LOG_ERROR("Failed to initialize display manager");
        _startupStateMachine->transitionTo(StartupState::ERROR, "DisplayManager initialization failed");
        return NO;
    }
    LOG_INFO("✓ DisplayManager initialized");

    // STATE: WINDOW_CREATED
    if (!_startupStateMachine->transitionTo(StartupState::WINDOW_CREATED, "Window created successfully")) {
        LOG_ERROR("Invalid state transition to WINDOW_CREATED");
        return NO;
    }

    // STATE: INITIALIZING_METAL
    if (!_startupStateMachine->transitionTo(StartupState::INITIALIZING_METAL, "Initializing Metal renderer")) {
        LOG_ERROR("Invalid state transition to INITIALIZING_METAL");
        return NO;
    }

    // Create Metal renderer
    LOG_INFO("Creating MetalRenderer...");
    _renderer = std::make_shared<MetalRenderer>();
    LOG_INFO("✓ MetalRenderer created");

    // Create FontAtlas
    LOG_INFO("Creating FontAtlas...");
    _fontAtlas = std::make_shared<FontAtlas>(_renderer->getDevice());

    LOG_INFO("Loading Unscii font...");

    // Try Unscii fonts first (preferred for retro aesthetic)
    if (_fontAtlas->loadUnsciiFont("unscii-16", 16)) {
        LOG_INFO("✓ FontAtlas loaded (Unscii-16 8x16)");
    } else if (_fontAtlas->loadUnsciiFont("unscii-8", 8)) {
        LOG_INFO("✓ FontAtlas loaded (Unscii-8 8x8)");
    } else if (_fontAtlas->loadUnsciiFont("unscii-16-full", 16)) {
        LOG_INFO("✓ FontAtlas loaded (Unscii-16-full 8x16)");
    } else {
        // Fallback to system fonts if Unscii not available
        LOG_INFO("Unscii fonts not available, trying system fonts...");
        std::string fontPath = "/System/Library/Fonts/Menlo.ttc";
        if (!_fontAtlas->loadTrueTypeFont(fontPath, 16)) {
            fontPath = "/System/Library/Fonts/Monaco.dfont";
            if (!_fontAtlas->loadTrueTypeFont(fontPath, 16)) {
                if (!_fontAtlas->loadBuiltinFont("vga_8x16")) {
                    LOG_ERROR("Failed to load any font");
                    return NO;
                }
                LOG_INFO("✓ FontAtlas loaded (fallback VGA font)");
            } else {
                LOG_INFO("✓ FontAtlas loaded (Monaco 16pt)");
            }
        } else {
            LOG_INFO("✓ FontAtlas loaded (Menlo 16pt)");
        }
    }

    // Set font atlas in renderer
    LOG_INFO("Setting FontAtlas in renderer...");
    _renderer->setFontAtlas(_fontAtlas);

    // Wire components to display manager
    // STATE: INITIALIZING_COMPONENTS
    if (!_startupStateMachine->transitionTo(StartupState::INITIALIZING_COMPONENTS, "Creating framework components")) {
        LOG_ERROR("Invalid state transition to INITIALIZING_COMPONENTS");
        return NO;
    }

    // Create framework components
    // Calculate text grid size based on ACTUAL Metal view bounds (not requested window size)
    // This is critical for Retina displays where the actual view size may differ
    NSView* metalView = _displayManager->getMetalView();
    if (!metalView) {
        LOG_ERROR("Could not get Metal view for grid size calculation");
        return NO;
    }

    NSRect actualBounds = metalView.bounds;
    NSScreen* screen = _displayManager->getWindow().screen ? _displayManager->getWindow().screen : [NSScreen mainScreen];
    CGFloat backingScale = screen.backingScaleFactor;

    // CRITICAL: On Retina displays with fullhd mode, the view bounds are DOUBLED
    // e.g. 1920x1058 points (not 960x529). We need to use the backing scale
    // to get the actual grid dimensions that were requested.
    float actualWidth = actualBounds.size.width;
    float actualHeight = actualBounds.size.height;

    // Calculate grid size by dividing REQUESTED size by REQUESTED cell size
    // This ensures grid matches what was configured
    int textCols = std::min((int)(config.windowWidth / config.cellWidth), 120);  // Max 120 columns
    int textRows = (int)(config.windowHeight / config.cellHeight);

    LOG_INFOF("Creating TextGrid (%dx%d) based on config (%dx%d / %dx%d cells) actualView=(%.0fx%.0f points, scale=%.1f)...",
          textCols, textRows, config.windowWidth, config.windowHeight, config.cellWidth, config.cellHeight,
          actualWidth, actualHeight, backingScale);
    _textGrid = std::make_shared<TextGrid>(textCols, textRows);
    LOG_INFOF("✓ TextGrid created (%dx%d)", textCols, textRows);

    LOG_INFO("Creating GraphicsLayer...");
    _graphicsLayer = std::make_shared<GraphicsLayer>();
    LOG_INFO("✓ GraphicsLayer created");

    LOG_INFO("Creating InputManager...");
    _inputManager = std::make_shared<InputManager>();
    LOG_INFO("✓ InputManager created");

    LOG_INFO("Creating AudioManager...");
    _audioManager = std::make_shared<AudioManager>();

    if (!_audioManager->initialize()) {
        LOG_WARNING("Failed to initialize AudioManager");
    } else {
        LOG_INFO("✓ AudioManager initialized");
    }

    LOG_INFO("Wiring components to DisplayManager...");
    _displayManager->setTextGrid(_textGrid);
    _displayManager->setRenderer(_renderer);
    _displayManager->setGraphicsLayer(_graphicsLayer);
    _displayManager->setInputManager(_inputManager);
    LOG_INFO("✓ Components wired to DisplayManager");

    // Create sprite manager
    LOG_INFO("Creating SpriteManager...");
    _spriteManager = std::make_shared<SpriteManager>(_renderer->getDevice());
    _displayManager->setSpriteManager(_spriteManager);
    LOG_INFO("✓ SpriteManager created and registered");

    // Create text display manager for overlay text
    LOG_INFO("Creating TextDisplayManager...");
    _textDisplayManager = std::make_shared<TextDisplayManager>(_renderer->getDevice());
    _textDisplayManager->setFontAtlas(_fontAtlas);
    _renderer->setTextDisplayManager(_textDisplayManager);
    LOG_INFO("✓ TextDisplayManager created and registered");

    // Create rectangle manager
    LOG_INFO("Creating RectangleManager...");
    _rectangleManager = std::make_shared<RectangleManager>();
    if (!_rectangleManager->initialize(_renderer->getDevice(), config.windowWidth, config.windowHeight)) {
        LOG_WARNING("Failed to initialize RectangleManager");
    } else {
        _displayManager->setRectangleManager(_rectangleManager);
        LOG_INFO("✓ RectangleManager created and registered");
    }

    // Create circle manager
    LOG_INFO("Creating CircleManager...");
    _circleManager = std::make_shared<CircleManager>();
    if (!_circleManager->initialize(_renderer->getDevice(), config.windowWidth, config.windowHeight)) {
        LOG_WARNING("Failed to initialize CircleManager");
    } else {
        _displayManager->setCircleManager(_circleManager);
        LOG_INFO("✓ CircleManager created and registered");
    }

    // Create line manager
    LOG_INFO("Creating LineManager...");
    _lineManager = std::make_shared<LineManager>();
    if (!_lineManager->initialize(_renderer->getDevice(), config.windowWidth, config.windowHeight)) {
        LOG_WARNING("Failed to initialize LineManager");
    } else {
        _displayManager->setLineManager(_lineManager);
        LOG_INFO("✓ LineManager created and registered");
    }

    // Create polygon manager
    LOG_INFO("Creating PolygonManager...");
    _polygonManager = std::make_shared<PolygonManager>();
    if (!_polygonManager->initialize(_renderer->getDevice(), config.windowWidth, config.windowHeight)) {
        LOG_WARNING("Failed to initialize PolygonManager");
    } else {
        _displayManager->setPolygonManager(_polygonManager);
        LOG_INFO("✓ PolygonManager created and registered");
    }

    // Create star manager
    LOG_INFO("Creating StarManager...");
    _starManager = std::make_shared<StarManager>();
    if (!_starManager->initialize(_renderer->getDevice(), config.windowWidth, config.windowHeight)) {
        LOG_WARNING("Failed to initialize StarManager");
    } else {
        _displayManager->setStarManager(_starManager);
        LOG_INFO("✓ StarManager created and registered");
    }

    // Create tilemap manager and renderer
    LOG_INFO("Creating TilemapManager and TilemapRenderer...");
    _tilemapManager = std::make_shared<TilemapManager>();
    _tilemapRenderer = std::make_shared<TilemapRenderer>(_renderer->getDevice());

    // Initialize TilemapRenderer with viewport dimensions matching the window
    uint32_t tilemapViewportWidth = config.windowWidth;
    uint32_t tilemapViewportHeight = config.windowHeight;
    if (!_tilemapRenderer->initialize(nullptr, tilemapViewportWidth, tilemapViewportHeight)) {
        LOG_WARNING("Failed to initialize TilemapRenderer");
    } else {
        LOG_INFOF("✓ TilemapRenderer initialized (%dx%d)", tilemapViewportWidth, tilemapViewportHeight);
    }

    _displayManager->setTilemapManager(_tilemapManager);
    _displayManager->setTilemapRenderer(_tilemapRenderer);
    LOG_INFO("✓ TilemapManager and TilemapRenderer created and registered");

    // Initialize particle system
    LOG_INFO("Initializing ParticleSystem (2048 max particles)...");
    if (!st_particle_system_initialize(2048)) {
        LOG_WARNING("Failed to initialize ParticleSystem");
    } else {
        LOG_INFO("✓ ParticleSystem initialized");

        // Connect particle system to sprite manager for sprite-based explosions
        st_particle_system_set_sprite_manager(_spriteManager.get());
        LOG_INFO("✓ ParticleSystem connected to SpriteManager");
    }

    // Create cart manager
    LOG_INFO("Creating CartManager...");
    _cartManager = std::make_shared<SuperTerminal::CartManager>();
    SuperTerminal::CartManagerConfig cartConfig;
    cartConfig.autoSave = true;
    cartConfig.autoSaveIntervalSeconds = 30;
    cartConfig.validateOnLoad = true;
    _cartManager->initialize(nullptr, cartConfig);
    LOG_INFO("✓ CartManager initialized");

    // Register with API context
    LOG_INFO("Registering components with API Context...");
    auto& ctx = STApi::Context::instance();
    ctx.setTextGrid(_textGrid);
    ctx.setGraphics(_graphicsLayer);
    ctx.setSprites(_spriteManager);
    ctx.setTilemap(_tilemapManager);
    ctx.setInput(_inputManager);
    ctx.setAudio(_audioManager);
    ctx.setDisplay(_displayManager);
    ctx.setTextDisplay(_textDisplayManager);
    ctx.setCartManager(_cartManager);
    LOG_INFO("✓ All components registered with API Context");

    // Frame wait queue will be processed in render loop
    LOG_INFO("✓ Frame wait queue system ready");

    // Store window reference
    self.window = _displayManager->getWindow();

    // Register mouse callback to call editor or interactive view
    __weak LuaBaseRunner* weakSelf = self;
    _displayManager->setMouseCallback([weakSelf](int gridX, int gridY, int button, bool pressed) {
        LuaBaseRunner* strongSelf = weakSelf;
        if (!strongSelf) return;

        if (strongSelf.editorMode && strongSelf->_textEditor) {
            // Editor mode - pass to text editor
            if (pressed) {
                strongSelf->_textEditor->handleMouseClick(gridX, gridY, button);
            } else {
                strongSelf->_textEditor->handleMouseRelease();
            }
        } else {
            // Interactive/runtime mode - handle selection in subclass
            if (pressed) {
                [strongSelf handleInteractiveMouseClick:gridX y:gridY button:button];
            } else if (button == 0) {
                [strongSelf handleInteractiveMouseRelease];
            }
        }
    });

    // STATE: LOADING_CONTENT (will be transitioned by subclass after loading content)
    LOG_INFO("=== Framework Initialization Complete ===");
    return YES;
}

// =============================================================================
// Application Lifecycle
// =============================================================================

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    NSLog(@"[LuaBaseRunner] %s starting...", self.runnerName.c_str());

    // Setup menu bar first
    NSLog(@"[LuaBaseRunner] Setting up menu bar...");
    [self setupMenuBar];

    // Initialize framework components
    if (![self initializeFramework]) {
        [self showError:@"Failed to initialize framework"];
        [NSApp terminate:nil];
        return;
    }

    // STATE: LOADING_CONTENT
    if (!_startupStateMachine->transitionTo(StartupState::LOADING_CONTENT, "Loading initial content")) {
        LOG_ERROR("Invalid state transition to LOADING_CONTENT");
        [NSApp terminate:nil];
        return;
    }

    // Initialize language-specific runtime
    LOG_INFO("Initializing language runtime...");
    if (![self initializeRuntime]) {
        _startupStateMachine->transitionTo(StartupState::ERROR, "Runtime initialization failed");
        [self showError:@"Failed to initialize runtime"];
        [NSApp terminate:nil];
        return;
    }
    LOG_INFO("✓ Runtime initialized");

    // Only start script thread if not in editor mode
    // Editor mode will run scripts on demand via runScript action
    if (!self.editorMode) {
        // Start the background script thread
        LOG_INFO("Starting script thread...");
        _running = true;
        _scriptThread = std::thread([self]() {
            @autoreleasepool {
                LOG_INFO("Script thread started");
                if (![self loadAndExecuteScript]) {
                    LOG_ERROR("Script execution failed");
                }
                LOG_INFO("Script thread finished");
                _running = false;
            }
        });
    } else {
        LOG_INFO("Editor mode - script thread will start on demand");
        _running = true;  // Keep app running in editor mode
    }

    // Mark DisplayManager as ready for rendering (prevents race with CVDisplayLink)
    NSLog(@"[LuaBaseRunner] Marking DisplayManager ready for rendering...");
    _displayManager->setReady();
    NSLog(@"[LuaBaseRunner] ✓ DisplayManager ready");

    // STATE: RENDERING_INITIAL (mark that initial content is rendered)
    if (!_startupStateMachine->transitionTo(StartupState::RENDERING_INITIAL, "Initial content rendered")) {
        LOG_ERROR("Invalid state transition to RENDERING_INITIAL");
        [NSApp terminate:nil];
        return;
    }

    // STATE: READY_TO_SHOW
    if (!_startupStateMachine->transitionTo(StartupState::READY_TO_SHOW, "Ready to show window")) {
        LOG_ERROR("Invalid state transition to READY_TO_SHOW");
        [NSApp terminate:nil];
        return;
    }

    // STATE: SHOWING_WINDOW - Now safe to show window
    if (!_startupStateMachine->transitionTo(StartupState::SHOWING_WINDOW, "Showing window")) {
        LOG_ERROR("Invalid state transition to SHOWING_WINDOW");
        [NSApp terminate:nil];
        return;
    }

    // Make window visible NOW (after everything is initialized and content loaded)
    // Skip if running in headless mode (batch mode)
    if (!self.headless) {
        LOG_INFO("Making window visible...");
        [self.window makeKeyAndOrderFront:nil];
        [self.window center];
    } else {
        NSLog(@"[LuaBaseRunner] Headless mode - window not shown");
    }

    // Small delay to let window server settle before starting render loop
    [NSThread sleepForTimeInterval:0.1];

    LOG_INFO("✓ Window shown");

    // STATE: STARTING_RENDER_LOOP
    if (!_startupStateMachine->transitionTo(StartupState::STARTING_RENDER_LOOP, "Starting render loop")) {
        NSLog(@"[LuaBaseRunner] ERROR: Invalid state transition to STARTING_RENDER_LOOP");
        [NSApp terminate:nil];
        return;
    }

    // Start the main render loop
    [self startRenderLoop];

    // STATE: RUNNING - Fully operational
    if (!_startupStateMachine->transitionTo(StartupState::RUNNING, "Application running")) {
        LOG_ERROR("Invalid state transition to RUNNING");
        [NSApp terminate:nil];
        return;
    }

    // Activate the app (unless headless)
    if (!self.headless) {
        [NSApp activateIgnoringOtherApps:YES];
    }

    NSLog(@"[LuaBaseRunner] %s initialized successfully - STATE: RUNNING", self.runnerName.c_str());
}

- (void)applicationWillTerminate:(NSNotification*)notification {
    NSLog(@"[LuaBaseRunner] %s shutting down...", self.runnerName.c_str());

    _running = false;

    // Signal any waiting threads to wake up
    [self signalFrameReady];

    // Stop display link
    if (_displayLink) {
        NSLog(@"[LuaBaseRunner] Stopping display link...");
        CVDisplayLinkStop(_displayLink);
        CVDisplayLinkRelease(_displayLink);
        _displayLink = nullptr;
    }

    // Wait for script thread
    if (_scriptThread.joinable()) {
        NSLog(@"[LuaBaseRunner] Waiting for script thread to finish...");
        _scriptThread.join();
        NSLog(@"[LuaBaseRunner] Script thread joined");
    }

    // Clean up language-specific runtime
    NSLog(@"[LuaBaseRunner] Cleaning up runtime...");
    [self cleanupRuntime];

    NSLog(@"[LuaBaseRunner] %s shutdown complete", self.runnerName.c_str());
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender {
    // Tell macOS to terminate immediately without asking about unsaved changes
    return NSTerminateNow;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    return YES;
}

// =============================================================================
// Render Loop
// =============================================================================

- (void)startRenderLoop {
    _lastFrameTime = CACurrentMediaTime();
    _frameCount = 0;

    NSLog(@"Starting render loop (60 FPS)");

    // Use CVDisplayLink for vsync at 60 Hz
    CVDisplayLinkCreateWithActiveCGDisplays(&_displayLink);
    CVDisplayLinkSetOutputCallback(_displayLink, &DisplayLinkCallback, (__bridge void*)self);

    // Show window (unless headless)
    if (!self.headless) {
        [self.window makeKeyAndOrderFront:nil];
        [self.window center];
    }

    // CRITICAL: Defer CVDisplayLinkStart to avoid race condition
    // Start it after a small delay to ensure window server and all systems are ready
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.2 * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
        NSLog(@"Starting CVDisplayLink (deferred)...");
        CVDisplayLinkStart(self->_displayLink);
        NSLog(@"Render loop started");
    });
}

static CVReturn DisplayLinkCallback(CVDisplayLinkRef displayLink,
                                   const CVTimeStamp* now,
                                   const CVTimeStamp* outputTime,
                                   CVOptionFlags flagsIn,
                                   CVOptionFlags* flagsOut,
                                   void* displayLinkContext) {
    @autoreleasepool {
        LuaBaseRunner* self = (__bridge LuaBaseRunner*)displayLinkContext;
        [self onFrameTick];
    }
    return kCVReturnSuccess;
}

- (void)onFrameTick {
    // Check if we're in RUNNING state before rendering
    if (!_startupStateMachine->canRenderThreadRun()) {
        // Not ready to render yet, skip this frame
        return;
    }

    if (!_running && _scriptThread.joinable()) {
        // Script finished, shut down
        NSLog(@"[LuaBaseRunner] Script finished, shutting down...");
        _scriptThread.join();
        dispatch_async(dispatch_get_main_queue(), ^{
            [NSApp terminate:self];
        });
        return;
    }

    double currentTime = CACurrentMediaTime();
    double deltaTime = currentTime - _lastFrameTime;
     _lastFrameTime = currentTime;
    _frameCount++;

    // Update API context timing
    auto& ctx = STApi::Context::instance();
    ctx.updateTime(currentTime, deltaTime);
    ctx.incrementFrame();

    // Update input
    _inputManager->beginFrame();

    // In editor mode, process input and update editor state
    if (self.editorMode) {
        [self updateEditor:deltaTime];
    }

    // If line input mode is active, drive the SimpleLineEditor from render thread
    // This is how INPUT_AT works - render thread processes input, script thread blocks
    if (ctx.isLineInputActive()) {
        ctx.updateLineInput();
    }

    // Update and render frame
    // Renders all layers: TextGrid, graphics, sprites, tilemaps, and particles
    // Content is updated by editor input handlers or script calls, not per-frame
    _displayManager->renderFrame();

    // Process frame wait queue - wake any scripts waiting for this frame
    // MUST happen BEFORE endFrame() so scripts can read input flags
    ctx.processFrameWaits();

    // Clear input flags for next frame (after scripts have read them)
    _inputManager->endFrame();
}

// =============================================================================
// Abstract Methods (Subclasses Must Override)
// =============================================================================

- (BOOL)initializeRuntime {
    NSLog(@"ERROR: Subclass must override initializeRuntime");
    return NO;
}

- (BOOL)loadAndExecuteScript {
    NSLog(@"ERROR: Subclass must override loadAndExecuteScript");
    return NO;
}

- (void)cleanupRuntime {
    NSLog(@"WARNING: Subclass should override cleanupRuntime");
}

// =============================================================================
// Utility Methods
// =============================================================================

- (void)terminate {
    [NSApp terminate:self];
}

- (void)showError:(NSString*)message {
    NSLog(@"ERROR: %@", message);

    dispatch_async(dispatch_get_main_queue(), ^{
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Error"];
        [alert setInformativeText:message];
        [alert setAlertStyle:NSAlertStyleCritical];
        [alert addButtonWithTitle:@"OK"];

        // Make dialog wider by setting accessory view with minimum width
        NSView* accessory = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 500, 0)];
        [alert setAccessoryView:accessory];

        [alert runModal];
    });
}

// =============================================================================
// Editor Support
// =============================================================================

- (BOOL)initializeEditor {
    NSLog(@"[LuaBaseRunner] Initializing editor subsystem...");

    // Create script database directory if needed
    NSString *supportDir = [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Application Support/SuperTerminal"];
    NSError *error = nil;
    [[NSFileManager defaultManager] createDirectoryAtPath:supportDir
                                withIntermediateDirectories:YES
                                                 attributes:nil
                                                      error:&error];
    if (error) {
        LOG_WARNINGF("Could not create support directory: %s", [error.localizedDescription UTF8String]);
    }

    // Create carts directory if needed
    NSString *cartsDir = [supportDir stringByAppendingPathComponent:@"carts"];
    error = nil;
    [[NSFileManager defaultManager] createDirectoryAtPath:cartsDir
                                withIntermediateDirectories:YES
                                                 attributes:nil
                                                      error:&error];
    if (error) {
        LOG_WARNINGF("Could not create carts directory: %s", [error.localizedDescription UTF8String]);
    } else {
        LOG_INFOF("✓ Carts directory ready at: %s", [cartsDir UTF8String]);
    }

    // Create script database
    std::string dbPath = std::string(NSHomeDirectory().UTF8String) + "/Library/Application Support/SuperTerminal/scripts.db";
    _scriptDatabase = std::make_shared<ScriptDatabase>();
    if (!_scriptDatabase->open(dbPath)) {
        LOG_ERROR("Failed to open script database");
        return NO;
    }
    LOG_INFO("✓ Script database opened");

    // Create screen mode manager
    _screenModeManager = std::make_shared<ScreenModeManager>();
    LOG_INFO("✓ Screen managers created");

    // Create screen state manager
    _screenStateManager = std::make_shared<ScreenStateManager>(_textGrid, _graphicsLayer, _spriteManager);
    NSLog(@"[LuaBaseRunner] ✓ Screen state manager created");

    // Create export/import manager
    _exportImportManager = std::make_shared<SuperTerminal::ExportImportManager>();
    LOG_INFO("✓ Export/Import manager created");

    // Create text editor
    _textEditor = std::make_shared<TextEditor>(_textGrid, _inputManager);
    if (!_textEditor->initialize()) {
        LOG_ERROR("Failed to create TextEditor");
        return NO;
    }
    NSLog(@"[LuaBaseRunner] ✓ Text editor initialized");

    // Register editor instance with C API
    editor_set_instance(_textEditor.get());
    NSLog(@"[LuaBaseRunner] ✓ Editor instance registered with C API");

    // Set up clipboard callback
    __weak LuaBaseRunner* weakSelf = self;
    _textEditor->setClipboardCallback([weakSelf](const std::string& operation, const std::string& text) -> std::string {
        LuaBaseRunner* strongSelf = weakSelf;
        if (!strongSelf) return "";

        if (operation == "cut" || operation == "copy") {
            NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
            [pasteboard clearContents];
            NSString* nsText = [NSString stringWithUTF8String:text.c_str()];
            [pasteboard setString:nsText forType:NSPasteboardTypeString];
            return "";
        } else if (operation == "paste") {
            NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
            NSString* nsText = [pasteboard stringForType:NSPasteboardTypeString];
            if (nsText) {
                return std::string([nsText UTF8String]);
            }
        }
        return "";
    });

    // Set up action callback - dispatch to main thread since these actions involve UI operations
    _textEditor->setActionCallback([weakSelf](EditorAction action) {
        dispatch_async(dispatch_get_main_queue(), ^{
            LuaBaseRunner* strongSelf = weakSelf;
            if (!strongSelf) return;

            switch (action) {
                case SuperTerminal::EditorAction::NEW_FILE:
                    [strongSelf newScript];
                    break;
                case SuperTerminal::EditorAction::OPEN_FILE:
                    [strongSelf openScript];
                    break;
                case SuperTerminal::EditorAction::SAVE_FILE:
                    [strongSelf saveScript];
                    break;
                case SuperTerminal::EditorAction::SAVE_FILE_AS:
                    [strongSelf saveScriptAs];
                    break;
                case SuperTerminal::EditorAction::CLOSE_FILE:
                    [strongSelf closeScript];
                    break;
                case SuperTerminal::EditorAction::RUN_SCRIPT:
                    [strongSelf runScript];
                    break;
                case SuperTerminal::EditorAction::STOP_SCRIPT:
                    [strongSelf stopScript];
                    break;
                case SuperTerminal::EditorAction::CLEAR_OUTPUT:
                    [strongSelf clearOutput];
                    break;
                default:
                    break;
            }
        });
    });

    // Set up syntax highlighter
    _textEditor->setSyntaxHighlighter([weakSelf](const std::string& line, size_t lineNumber) -> std::vector<uint32_t> {
        LuaBaseRunner* strongSelf = weakSelf;
        if (!strongSelf) return std::vector<uint32_t>();
        return [strongSelf highlightLine:line lineNumber:lineNumber];
    });

    // Status bar is now built into DisplayManager (v1 pattern for Metal view coexistence)
    // Start with status bar hidden, will show in editor mode
    if (_displayManager) {
        _displayManager->setStatusBarVisible(false);
    }

    NSLog(@"[LuaBaseRunner] ✓ Editor subsystem initialized (using DisplayManager status bar)");
    return YES;
}

- (void)shutdownEditor {
    LOG_INFO("Shutting down editor subsystem...");

    if (_textEditor) {
        _textEditor->shutdown();
        _textEditor.reset();
    }

    // Cleanup EditorAPI
    editor_shutdown();

    if (_scriptDatabase) {
        _scriptDatabase->close();
        _scriptDatabase.reset();
    }

    _screenStateManager.reset();
    _screenModeManager.reset();

    // Hide status bar when shutting down editor
    if (_displayManager) {
        _displayManager->setStatusBarVisible(false);
    }

    LOG_INFO("✓ Editor subsystem initialized");
}

- (void)updateEditor:(double)deltaTime {
    if (!_textEditor || !self.editorMode) {
        return;
    }

    // Process input first
    _textEditor->processInput();

    // Then update state
    _textEditor->update(deltaTime);

    // Render editor content to TextGrid
    _textEditor->render();

    // Update status bar using DisplayManager
    if (_displayManager) {
        std::string filename = _textEditor->getFilename();
        std::string language = _textEditor->getLanguage();

        // Debug: Log filename on first few frames to catch initialization issues
        static int logCount = 0;
        if (logCount < 10) {
            NSLog(@"[LuaBaseRunner] Frame %d - Filename from editor: '%s' (length: %zu)",
                  logCount, filename.c_str(), filename.length());
            logCount++;
        }

        // Sanitize filename - check for valid printable characters
        bool filenameValid = true;
        if (!filename.empty()) {
            for (char c : filename) {
                if (c < 32 || c > 126) {  // Not printable ASCII
                    filenameValid = false;
                    NSLog(@"[LuaBaseRunner] WARNING: Invalid character in filename (code: %d)", (int)c);
                    break;
                }
            }
        }

        // If filename is invalid or empty, use "untitled"
        if (!filenameValid || filename.empty()) {
            filename = "untitled";
            if (!filenameValid) {
                NSLog(@"[LuaBaseRunner] WARNING: Filename contained non-printable characters, reset to 'untitled'");
            }
        }

        // Format status: "filename · language"
        std::string statusText = filename;
        if (!language.empty()) {
            statusText += " · " + language;
        }

        _displayManager->updateStatus(statusText.c_str());
        _displayManager->updateCursorPosition(
            (int)_textEditor->getCursorLine() + 1,
            (int)_textEditor->getCursorColumn() + 1
        );

        // Show line count and modified status in script label
        std::string scriptInfo = std::to_string(_textEditor->getLineCount()) + " lines";
        if (_textEditor->isDirty()) {
            scriptInfo += " · Modified";
        }
        _displayManager->updateScriptName(scriptInfo.c_str());
    }
}

- (void)renderEditor {
    if (!_textEditor || !self.editorMode) {
        return;
    }

    // Debug: Log that we're rendering
    if (_frameCount % 60 == 0) {
        uint64_t currentFrame = _frameCount.load();
        NSLog(@"[EDITOR RENDER] Calling _textEditor->render() at frame %llu", currentFrame);
    }

    _textEditor->render();

    // Debug: Check what was written to TextGrid
    if (_frameCount % 60 == 0 && _textGrid) {
        auto cell00 = _textGrid->getCell(0, 0);
        auto cell10 = _textGrid->getCell(1, 0);
        NSLog(@"[EDITOR RENDER] After render: (0,0)='%c' (1,0)='%c'",
              (char)cell00.character, (char)cell10.character);
    }
}

- (void)toggleEditorMode {
    if (self.editorMode) {
        [self enterRuntimeMode];
    } else {
        [self enterEditorMode];
    }
}

- (void)enterEditorMode {
    // Clear all input state to prevent stuck keys and hyper-speed auto-repeat
    // This is critical when returning from script execution or error dialogs
    st_key_clear_all();

    // Also clear InputManager state to reset keyboard/mouse handling
    if (_inputManager) {
        _inputManager->clearAll();
    }

    // CRITICAL: Always restore TEXT mode when entering editor mode
    // Scripts may have switched to graphics modes (LORES, XRES, WRES, URES)
    // Editor needs text display, so disable all graphics modes
    if (_displayManager) {
        _displayManager->setLoResMode(false);
        _displayManager->setXResMode(false);
        _displayManager->setWResMode(false);
        _displayManager->setUResMode(false);
    }

    // Switch to editor mode
    self.editorMode = YES;

    // Update EditorAPI active state
    editor_set_active(YES);

    // Show status bar in editor mode
    if (_displayManager) {
        _displayManager->setStatusBarVisible(true);
        _displayManager->updateStatus("● Editor");
    }

    // Use ScreenStateManager to handle state switching
    if (_screenStateManager && _frameCount > 0) {
        _screenStateManager->switchToEditorMode();
    } else {
        // First time - just clear and render editor
        if (_textGrid) {
            _textGrid->clear();
        }
    }

    // Make sure cursor is visible
    if (_textEditor && _textEditor->getCursor()) {
        _textEditor->getCursor()->resetBlink();
    }

    // Force initial render to display editor content immediately
    if (_textEditor) {
        _textEditor->render();
    }
}

- (void)enterEditorModeAndClear {
    // This method is used when opening/creating documents to ensure a clean slate
    // It prevents the ScreenStateManager from restoring old content

    NSLog(@"[LuaBaseRunner] === enterEditorModeAndClear ===");

    // Invalidate saved editor state to prevent restoring old content
    if (_screenStateManager) {
        _screenStateManager->invalidateEditorState();
        NSLog(@"[LuaBaseRunner]   Invalidated editor state");
    }

    // Clear all input state
    st_key_clear_all();
    if (_inputManager) {
        _inputManager->clearAll();
    }

    // Restore TEXT mode
    if (_displayManager) {
        _displayManager->setLoResMode(false);
    }

    // Switch to editor mode
    self.editorMode = YES;
    editor_set_active(YES);

    // Show status bar
    if (_displayManager) {
        _displayManager->setStatusBarVisible(true);
        _displayManager->updateStatus("● Editor");
    }

    // Clear text grid (don't restore from saved state)
    if (_textGrid) {
        _textGrid->clear();
        NSLog(@"[LuaBaseRunner]   Cleared text grid");
    }

    // Reset cursor
    if (_textEditor && _textEditor->getCursor()) {
        _textEditor->getCursor()->resetBlink();
    }

    // Render the current document content
    if (_textEditor) {
        _textEditor->render();
        NSLog(@"[LuaBaseRunner]   Rendered editor");
    }

    NSLog(@"[LuaBaseRunner] ✓ Editor mode entered with clear state");
}

- (void)enterRuntimeMode {
    NSLog(@"[LuaBaseRunner] Entering runtime mode");

    // === APPLE II STYLE RUNTIME MODE ===
    // Like the Apple II, we clear the screen and reset to TEXT mode
    // before script execution begins. This ensures a clean slate.

    // Clear all input state to prevent keys from editor affecting runtime
    st_key_clear_all();

    // Also clear InputManager state
    if (_inputManager) {
        _inputManager->clearAll();
    }

    // Ensure TEXT mode is active at start of runtime
    // (Scripts can switch to graphics modes if needed, but start clean)
    if (_displayManager) {
        _displayManager->setLoResMode(false);
        _displayManager->setXResMode(false);
        _displayManager->setWResMode(false);
        _displayManager->setUResMode(false);
    }

    // Switch to runtime mode
    self.editorMode = NO;

    // Update EditorAPI active state
    editor_set_active(NO);

    // Hide status bar in runtime mode
    if (_displayManager) {
        _displayManager->setStatusBarVisible(false);
    }

    // Always clear the screen when entering runtime mode
    // This ensures editor content doesn't remain visible
    if (_textGrid) {
        _textGrid->clear();
    }
    // Don't automatically clear graphics when entering runtime mode
    // Graphics should only be cleared by explicit commands (CLG, CLS, etc.)
    // if (_graphicsLayer) {
    //     _graphicsLayer->clear();
    // }

    NSLog(@"[LuaBaseRunner] ✓ Runtime mode active (screen cleared)");
}

// =============================================================================
// Window Size Management
// =============================================================================

- (void)setWindowSizeSmall:(id)sender {
    NSLog(@"[LuaBaseRunner] Changing window size to Small (640x400)");
    [self changeWindowSizeWithWidth:640 height:400 cellWidth:8 cellHeight:16];
}

- (void)setWindowSizeMedium:(id)sender {
    NSLog(@"[LuaBaseRunner] Changing window size to Medium (800x600)");
    [self changeWindowSizeWithWidth:800 height:600 cellWidth:8 cellHeight:16];
}

- (void)setWindowSizeLarge:(id)sender {
    NSLog(@"[LuaBaseRunner] Changing window size to Large (1280x720)");
    [self changeWindowSizeWithWidth:1280 height:720 cellWidth:10 cellHeight:20];
}

- (void)setWindowSizeFullHD:(id)sender {
    NSLog(@"[LuaBaseRunner] Changing window size to Full HD (1920x1080)");
    [self changeWindowSizeWithWidth:1920 height:1080 cellWidth:16 cellHeight:32];
}

- (void)changeWindowSizeWithWidth:(uint32_t)width height:(uint32_t)height cellWidth:(uint32_t)cellW cellHeight:(uint32_t)cellH {
    NSLog(@"[LuaBaseRunner] Resizing window to %dx%d (cells: %dx%d)", width, height, cellW, cellH);

    // Save current state based on mode
    if (_screenStateManager) {
        if (self.editorMode) {
            _screenStateManager->saveEditorState();
            NSLog(@"[LuaBaseRunner] Saved editor state");
        } else {
            _screenStateManager->saveRuntimeState();
            NSLog(@"[LuaBaseRunner] Saved runtime state");
        }
    }

    // Calculate new text grid dimensions
    int newTextCols = std::min((int)(width / cellW), 120);
    int newTextRows = (int)(height / cellH);

    NSLog(@"[LuaBaseRunner] New text grid: %dx%d", newTextCols, newTextRows);

    // Resize the window
    if (_displayManager) {
        _displayManager->setWindowSize(width, height);
    }

    // Recreate text grid with new dimensions
    _textGrid = std::make_shared<TextGrid>(newTextCols, newTextRows);
    NSLog(@"[LuaBaseRunner] TextGrid recreated (%dx%d)", newTextCols, newTextRows);

    // Recreate graphics layer
    _graphicsLayer = std::make_shared<GraphicsLayer>();
    NSLog(@"[LuaBaseRunner] GraphicsLayer recreated");

    // Reload FontAtlas at new size
    NSLog(@"[LuaBaseRunner] Reloading FontAtlas for cell size %dx%d", cellW, cellH);
    _fontAtlas = std::make_shared<FontAtlas>(_renderer->getDevice());

    // Calculate font size based on cell height (roughly 75% of cell height works well)
    int fontSize = (cellH * 3) / 4;
    if (fontSize < 8) fontSize = 8;
    if (fontSize > 32) fontSize = 32;

    // Try to use appropriate Unscii variant based on cell size
    std::string unsciiVariant;
    if (cellW == 8 && cellH == 8) {
        unsciiVariant = "unscii-8";
    } else if (cellW == 8 && cellH == 16) {
        unsciiVariant = "unscii-16";
    } else {
        unsciiVariant = "unscii-16"; // Default to 16 for better readability
    }

    if (_fontAtlas->loadUnsciiFont(unsciiVariant, fontSize)) {
        NSLog(@"[LuaBaseRunner] FontAtlas reloaded (Unscii %s %dpt)", unsciiVariant.c_str(), fontSize);
    } else {
        // Fallback to system fonts
        NSLog(@"[LuaBaseRunner] Unscii not available, using system fonts...");
        std::string fontPath = "/System/Library/Fonts/Menlo.ttc";
        if (!_fontAtlas->loadTrueTypeFont(fontPath, fontSize)) {
            fontPath = "/System/Library/Fonts/Monaco.dfont";
            if (!_fontAtlas->loadTrueTypeFont(fontPath, fontSize)) {
                // Fallback to builtin font
                std::string builtinFont = (cellW == 8 && cellH == 16) ? "vga_8x16" : "vga_8x16";
                if (!_fontAtlas->loadBuiltinFont(builtinFont)) {
                    NSLog(@"[LuaBaseRunner] WARNING: Failed to reload font at size %d", fontSize);
                } else {
                    NSLog(@"[LuaBaseRunner] FontAtlas reloaded (builtin VGA)");
                }
            } else {
                NSLog(@"[LuaBaseRunner] FontAtlas reloaded (Monaco %dpt)", fontSize);
            }
        } else {
            NSLog(@"[LuaBaseRunner] FontAtlas reloaded (Menlo %dpt)", fontSize);
        }
    }

    // Update renderer with new font atlas
    _renderer->setFontAtlas(_fontAtlas);

    // Update renderer and display manager with new grid
    if (_displayManager) {
        _displayManager->setTextGrid(_textGrid);
    }

    // Update input manager with new cell size for mouse grid tracking
    // CRITICAL: Mouse coordinates come from NSEvent in POINTS (view bounds space),
    // NOT in physical pixels (drawable space). On retina displays, drawable size
    // is 2x the view bounds size. The InputManager must use cell sizes in the
    // SAME coordinate space as the mouse input (points), so we must use the
    // ACTUAL view bounds, not the requested window size, to account for title bars
    // and window chrome that affect the actual rendering area.
    if (_inputManager && _textGrid && _displayManager) {
        // Get the ACTUAL Metal view bounds (not the requested window size)
        NSView* metalView = _displayManager->getMetalView();
        if (metalView) {
            NSRect actualBounds = metalView.bounds;
            float actualWidth = actualBounds.size.width;
            float actualHeight = actualBounds.size.height;

            // Calculate cell sizes based on ACTUAL view dimensions
            // This must match how the TextGrid was created
            float actualCellW = actualWidth / (float)newTextCols;
            float actualCellH = actualHeight / (float)newTextRows;

            _inputManager->setCellSize(actualCellW, actualCellH);
            NSLog(@"[LuaBaseRunner] InputManager cell size updated to %.2fx%.2f (actual view: %.0fx%.0f, grid: %dx%d)",
                  actualCellW, actualCellH, actualWidth, actualHeight, newTextCols, newTextRows);
            NSLog(@"[LuaBaseRunner] This ensures mouse coordinates (in view points) correctly map to grid cells");
        } else {
            NSLog(@"[LuaBaseRunner] WARNING: Could not get metalView for cell size calculation");
        }
    }

    // Recreate screen state manager with new components
    _screenStateManager = std::make_unique<ScreenStateManager>(
        _textGrid, _graphicsLayer, _spriteManager
    );

    // Update editor with new text grid and re-render
    if (_textEditor) {
        NSLog(@"[LuaBaseRunner] Updating editor with new text grid");
        _textEditor->setTextGrid(_textGrid);
        NSLog(@"[LuaBaseRunner] Editor updated and re-rendered");
    }

    // Restore state for current mode
    if (self.editorMode) {
        NSLog(@"[LuaBaseRunner] In editor mode - editor should be visible");
        // Editor already updated and rendered above
    } else {
        NSLog(@"[LuaBaseRunner] In runtime mode");
        // Clear screen for runtime
        if (_textGrid) {
            _textGrid->clear();
        }
        if (_graphicsLayer) {
            _graphicsLayer->clear();
        }
    }

    NSLog(@"[LuaBaseRunner] ✓ Window resize complete");
}

// =============================================================================
// Script Management (Editor Actions)
// =============================================================================

- (void)newScript {
    NSLog(@"[LuaBaseRunner] Creating new script (default)");
    [self newLuaScript];  // Default to Lua for LuaRunner2
}

- (void)newLuaScript {
    NSLog(@"[LuaBaseRunner] === Creating new Lua script (DOCUMENT-BASED) ===");

    if (!_textEditor || !_scriptDatabase) return;

    // Close any open document without prompting
    if (_currentDocument && _currentDocument->isOpen()) {
        _currentDocument->close();
    }

    // Create a completely fresh Document object
    _currentDocument = std::make_shared<Document>();

    // Create new empty document
    _currentDocument->createNew(_scriptDatabase, "", ScriptLanguage::LUA);

    // Clear the editor's text buffer first
    _textEditor->clear();

    // Connect Document to TextEditor (this loads the document content into the buffer)
    _textEditor->setDocument(_currentDocument);
    _textEditor->setLanguage("lua");
    self.currentScriptLanguage = "lua";

    NSLog(@"[LuaBaseRunner] ✓ New Lua document created and connected");

    // Enter editor mode with full clear
    [self enterEditorModeAndClear];
}

- (void)newBasicScript {
    NSLog(@"[LuaBaseRunner] === Creating new BASIC script (DOCUMENT-BASED) ===");

    if (!_textEditor || !_scriptDatabase) return;

    // Close any open document without prompting
    if (_currentDocument && _currentDocument->isOpen()) {
        _currentDocument->close();
    }

    // Create a completely fresh Document object
    _currentDocument = std::make_shared<Document>();

    // Create new empty document
    _currentDocument->createNew(_scriptDatabase, "", ScriptLanguage::BASIC);

    // Clear the editor's text buffer first
    _textEditor->clear();

    // Connect Document to TextEditor (this loads the document content into the buffer)
    _textEditor->setDocument(_currentDocument);
    _textEditor->setLanguage("basic");
    self.currentScriptLanguage = "basic";

    NSLog(@"[LuaBaseRunner] ✓ New BASIC document created and connected");

    // Enter editor mode with full clear
    [self enterEditorModeAndClear];
}

- (void)newAbcScript {
    NSLog(@"[LuaBaseRunner] === Creating new ABC script (DOCUMENT-BASED) ===");

    if (!_textEditor || !_scriptDatabase) return;

    // Close any open document without prompting
    if (_currentDocument && _currentDocument->isOpen()) {
        _currentDocument->close();
    }

    // Create a completely fresh Document object
    _currentDocument = std::make_shared<Document>();

    // Create new empty document
    _currentDocument->createNew(_scriptDatabase, "", ScriptLanguage::ABC);

    // Clear the editor's text buffer first
    _textEditor->clear();

    // Connect Document to TextEditor (this loads the document content into the buffer)
    _textEditor->setDocument(_currentDocument);
    _textEditor->setLanguage("abc");
    self.currentScriptLanguage = "abc";

    NSLog(@"[LuaBaseRunner] ✓ New ABC document created and connected");

    // Enter editor mode with full clear
    [self enterEditorModeAndClear];
}

- (void)newVoiceScript {
    NSLog(@"[LuaBaseRunner] === Creating new VoiceScript (DOCUMENT-BASED) ===");

    if (!_textEditor || !_scriptDatabase) return;

    // Close any open document without prompting
    if (_currentDocument && _currentDocument->isOpen()) {
        _currentDocument->close();
    }

    // Create a completely fresh Document object
    _currentDocument = std::make_shared<Document>();

    // Create new empty document
    _currentDocument->createNew(_scriptDatabase, "", ScriptLanguage::VOICESCRIPT);

    // Clear the editor's text buffer first
    _textEditor->clear();

    // Connect Document to TextEditor (this loads the document content into the buffer)
    _textEditor->setDocument(_currentDocument);
    _textEditor->setLanguage("voicescript");
    self.currentScriptLanguage = "voicescript";

    NSLog(@"[LuaBaseRunner] ✓ New VoiceScript document created and connected");

    // Enter editor mode with full clear
    [self enterEditorModeAndClear];
}

- (void)openScript {
    // Ensure we're on the main thread for UI operations
    if (![NSThread isMainThread]) {
        NSLog(@"[LuaBaseRunner] openScript called from background thread, dispatching to main");
        dispatch_async(dispatch_get_main_queue(), ^{
            [self openScript];
        });
        return;
    }

    NSLog(@"[LuaBaseRunner] === OPEN SCRIPT (DOCUMENT-BASED) ===");

    if (!_scriptDatabase || !_textEditor) {
        NSLog(@"[LuaBaseRunner] ERROR: Missing components - database=%p editor=%p",
              _scriptDatabase.get(), _textEditor.get());
        return;
    }

    // Close any open document without prompting
    if (_currentDocument && _currentDocument->isOpen()) {
        NSLog(@"[LuaBaseRunner] Closing existing document: '%s'", _currentDocument->getName().c_str());
        _currentDocument->close();
    }

    // Clear saved editor state to ensure clean slate for opening
    if (_screenStateManager) {
        _screenStateManager->invalidateEditorState();
        NSLog(@"[LuaBaseRunner] Cleared saved editor state before opening");
    }

    // Ensure we're not in editor mode yet
    self.editorMode = NO;

    // Show script browser dialog
    std::string selectedName;
    ScriptLanguage selectedLanguage;
    BOOL isFromCart = NO;

    // Default to current language or Lua
    ScriptLanguage currentLang = ScriptLanguage::LUA;
    if (self.currentScriptLanguage == "javascript" || self.currentScriptLanguage == "js") {
        currentLang = ScriptLanguage::JAVASCRIPT;
    } else if (self.currentScriptLanguage == "basic") {
        currentLang = ScriptLanguage::BASIC;
    } else if (self.currentScriptLanguage == "scheme") {
        currentLang = ScriptLanguage::SCHEME;
    } else if (self.currentScriptLanguage == "abc") {
        currentLang = ScriptLanguage::ABC;
    } else if (self.currentScriptLanguage == "voicescript" || self.currentScriptLanguage == "vscript") {
        currentLang = ScriptLanguage::VOICESCRIPT;
    }

    BOOL dialogResult = [ScriptBrowserDialog showBrowserWithDatabase:_scriptDatabase
                                                             language:currentLang
                                                          cartManager:_cartManager
                                                              outName:selectedName
                                                          outLanguage:selectedLanguage
                                                        outIsFromCart:isFromCart];

    if (!dialogResult || selectedName.empty()) {
        NSLog(@"[LuaBaseRunner] User cancelled or invalid selection");
        return;
    }

    NSLog(@"[LuaBaseRunner] Selected: '%s' (language: %d, from cart: %d)",
          selectedName.c_str(), (int)selectedLanguage, isFromCart);

    // =========================================================================
    // STEP 1: Create/Open Document (SINGLE SOURCE OF TRUTH)
    // =========================================================================
    NSLog(@"[LuaBaseRunner] === Creating Document instance ===");

    // Create new document if needed
    if (!_currentDocument) {
        _currentDocument = std::make_shared<Document>();
        NSLog(@"[LuaBaseRunner]   Created new Document instance");
    }

    // Open the document from database or cart
    if (isFromCart) {
        // Load from cart data file
        NSLog(@"[LuaBaseRunner] Loading script from cart: '%s'", selectedName.c_str());

        if (!_cartManager || !_cartManager->isCartActive()) {
            NSLog(@"[LuaBaseRunner] ERROR: Cart not active");
            [self showError:@"Cart is not active"];
            return;
        }

        auto cartLoader = _cartManager->getLoader();
        if (!cartLoader) {
            NSLog(@"[LuaBaseRunner] ERROR: No cart loader");
            [self showError:@"Failed to access cart"];
            return;
        }

        // Load the data file from cart
        CartDataFile dataFile;
        if (!cartLoader->loadDataFile(selectedName, dataFile)) {
            NSLog(@"[LuaBaseRunner] ERROR: Failed to load cart file: %s",
                  cartLoader->getLastError().c_str());
            [self showError:@"Failed to load script from cart"];
            return;
        }

        // Convert data to string
        std::string content(dataFile.data.begin(), dataFile.data.end());

        // Create a new document with the content
        _currentDocument->createNew(_scriptDatabase, selectedName, selectedLanguage);
        _currentDocument->setText(content);
        _currentDocument->markClean();  // Cart scripts start clean

        NSLog(@"[LuaBaseRunner] ✓ Loaded %zu bytes from cart", content.length());
    } else {
        // Load from database
        if (!_currentDocument->open(_scriptDatabase, selectedName, selectedLanguage)) {
            NSLog(@"[LuaBaseRunner] ERROR: Failed to open document: %s",
                  _currentDocument->getLastError().c_str());
            [self showError:@"Failed to open script from database"];
            return;
        }
    }

    NSLog(@"[LuaBaseRunner] ✓ Document opened successfully");
    NSLog(@"[LuaBaseRunner]   Name: '%s'", _currentDocument->getName().c_str());
    NSLog(@"[LuaBaseRunner]   Lines: %zu", _currentDocument->getLineCount());
    NSLog(@"[LuaBaseRunner]   Language: %d", (int)_currentDocument->getLanguage());

    // =========================================================================
    // STEP 2: Set Active Document in Database
    // =========================================================================
    _scriptDatabase->setActiveDocument(selectedName, selectedLanguage);

    // =========================================================================
    // STEP 3: Update Language
    // =========================================================================
    switch (selectedLanguage) {
        case ScriptLanguage::LUA:
            _textEditor->setLanguage("lua");
            self.currentScriptLanguage = "lua";
            break;
        case ScriptLanguage::JAVASCRIPT:
            _textEditor->setLanguage("javascript");
            self.currentScriptLanguage = "javascript";
            break;
        case ScriptLanguage::BASIC:
            _textEditor->setLanguage("basic");
            self.currentScriptLanguage = "basic";
            break;
        case ScriptLanguage::SCHEME:
            _textEditor->setLanguage("scheme");
            self.currentScriptLanguage = "scheme";
            break;
        case ScriptLanguage::ABC:
            _textEditor->setLanguage("abc");
            self.currentScriptLanguage = "abc";
            break;
        case ScriptLanguage::VOICESCRIPT:
            _textEditor->setLanguage("voicescript");
            self.currentScriptLanguage = "voicescript";
            break;
    }

    // =========================================================================
    // STEP 4: Format BASIC scripts on load
    // =========================================================================
    // Auto-format removed - not needed for LuaRunner

    // =========================================================================
    // STEP 5: Connect Document to TextEditor
    // =========================================================================
    NSLog(@"[LuaBaseRunner] === Connecting Document to TextEditor ===");

    // Set filename
    _textEditor->setFilename(selectedName);
    NSLog(@"[LuaBaseRunner]   Set filename: '%s'", selectedName.c_str());

    // Connect Document to TextEditor (this loads the document content into the buffer)
    // NOTE: Do NOT call clear() before setDocument() - it clears the document content!
    _textEditor->setDocument(_currentDocument);
    _textEditor->markClean();

    NSLog(@"[LuaBaseRunner]   Document connected: %zu chars, %zu lines",
          _currentDocument->getText().length(), _currentDocument->getLineCount());
    NSLog(@"[LuaBaseRunner]   Editor buffer now has: %zu lines",
          _textEditor->getLineCount());

    // =========================================================================
    // STEP 6: Switch to Editor Mode and Refresh Display
    // =========================================================================

    // Enter editor mode with full clear
    [self enterEditorModeAndClear];

    NSLog(@"[LuaBaseRunner] ✓ Script opened: '%s' (%zu lines)",
          selectedName.c_str(), _currentDocument->getLineCount());
    NSLog(@"[LuaBaseRunner] === OPEN COMPLETE (Document is now single source of truth) ===");
}

- (void)saveScript {
    // Ensure we're on the main thread for UI operations
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self saveScript];
        });
        return;
    }

    NSLog(@"[LuaBaseRunner] === SAVE SCRIPT (DOCUMENT-BASED) ===");

    if (!_currentDocument || !_currentDocument->isOpen()) {
        NSLog(@"[LuaBaseRunner] No document open");
        return;
    }

    std::string currentFilename = _currentDocument->getName();

    // Check if this is an unsaved new file (starts with "untitled")
    if (currentFilename.empty() || currentFilename.find("untitled") == 0) {
        NSLog(@"[LuaBaseRunner] File is new/untitled, redirecting to Save As");
        [self saveScriptAs];
        return;
    }

    NSLog(@"[LuaBaseRunner] Saving document: '%s'", currentFilename.c_str());

    // =========================================================================
    // NOTE: TextEditor auto-syncs changes to Document, so Document is current
    // =========================================================================

    // =========================================================================
    // STEP 1: Save Document to database
    // =========================================================================
    if (_currentDocument->save()) {
        _textEditor->markClean();
        NSLog(@"[LuaBaseRunner] ✓ Document saved successfully: '%s'", currentFilename.c_str());

        // Update status bar
        [self updateEditor:0.0];
    } else {
        NSLog(@"[LuaBaseRunner] ERROR: Failed to save document: %s",
              _currentDocument->getLastError().c_str());

        // Show error to user
        NSAlert* errorAlert = [[NSAlert alloc] init];
        [errorAlert setMessageText:@"Save Failed"];
        [errorAlert setInformativeText:[NSString stringWithFormat:@"Could not save script '%s'.\n\nError: %s",
                                        currentFilename.c_str(),
                                        _currentDocument->getLastError().c_str()]];
        [errorAlert addButtonWithTitle:@"OK"];
        [errorAlert setAlertStyle:NSAlertStyleWarning];
        [errorAlert runModal];
    }
}

- (void)saveScriptAs {
    // Ensure we're on the main thread for UI operations
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self saveScriptAs];
        });
        return;
    }

    NSLog(@"[LuaBaseRunner] === Save script as... (DOCUMENT-BASED) ===");

    if (!_currentDocument || !_currentDocument->isOpen() || !_textEditor) {
        NSLog(@"[LuaBaseRunner] ERROR: No document open or missing editor");
        return;
    }

    std::string currentFilename = _currentDocument->getName();
    NSLog(@"[LuaBaseRunner] Current document name: '%s'", currentFilename.c_str());

    // Use simple NSAlert with text input
    NSAlert* alert = [[NSAlert alloc] init];
    [alert setMessageText:@"Save Script"];
    [alert setInformativeText:@"Enter a name for your script:"];
    [alert addButtonWithTitle:@"Save"];
    [alert addButtonWithTitle:@"Cancel"];

    NSTextField* input = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 300, 24)];
    NSString* defaultName = (currentFilename.empty() || currentFilename.find("untitled") == 0)
        ? @"untitled"
        : [NSString stringWithUTF8String:currentFilename.c_str()];
    [input setStringValue:defaultName];
    [alert setAccessoryView:input];

    [alert beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse returnCode) {
        if (returnCode == NSAlertFirstButtonReturn) {
            NSString* name = [input stringValue];
            if (name.length == 0) {
                NSLog(@"[LuaBaseRunner] Save cancelled - no name entered");
                return;
            }

            std::string newName = std::string([name UTF8String]);
            NSLog(@"[LuaBaseRunner] User entered name: '%s'", newName.c_str());

            // Auto-append appropriate extension if missing based on current language
            ScriptLanguage currentLang = _currentDocument->getLanguage();

            if (currentLang == ScriptLanguage::ABC && newName.find(".abc") == std::string::npos) {
                newName += ".abc";
                NSLog(@"[LuaBaseRunner] Auto-added .abc extension: '%s'", newName.c_str());
            } else if (currentLang == ScriptLanguage::VOICESCRIPT &&
                       newName.find(".vscript") == std::string::npos &&
                       newName.find(".voicescript") == std::string::npos) {
                newName += ".vscript";
                NSLog(@"[LuaBaseRunner] Auto-added .vscript extension: '%s'", newName.c_str());
            }

            // Use Document's saveAs method (maintains single source of truth)
            NSLog(@"[LuaBaseRunner] Calling Document::saveAs with new name: '%s'", newName.c_str());
            if (_currentDocument->saveAs(newName)) {
                _textEditor->markClean();
                NSLog(@"[LuaBaseRunner] ✓ Script saved as: %s", newName.c_str());
                NSLog(@"[LuaBaseRunner]   Document name: '%s'", _currentDocument->getName().c_str());
                NSLog(@"[LuaBaseRunner]   Language: %d", (int)_currentDocument->getLanguage());

                // Update status bar to reflect the new filename
                dispatch_async(dispatch_get_main_queue(), ^{
                    [self updateEditor:0.0];
                });
            } else {
                NSLog(@"[LuaBaseRunner] ERROR: Failed to save document: %s",
                      _currentDocument->getLastError().c_str());

                // Show error to user
                NSAlert* errorAlert = [[NSAlert alloc] init];
                [errorAlert setMessageText:@"Save Failed"];
                [errorAlert setInformativeText:[NSString stringWithFormat:@"Could not save script '%s'.\n\nError: %s",
                                                newName.c_str(),
                                                _currentDocument->getLastError().c_str()]];
                [errorAlert addButtonWithTitle:@"OK"];
                [errorAlert setAlertStyle:NSAlertStyleWarning];
                [errorAlert runModal];
            }
        }
    }];
}

- (void)removeLineNumbers {
        // Ensure we're on the main thread
        if (![NSThread isMainThread]) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [self removeLineNumbers];
            });
            return;
        }

        NSLog(@"[LuaBaseRunner] Removing line numbers from script");

        if (!_textEditor) {
            NSLog(@"[LuaBaseRunner] ERROR: No text editor available");
            return;
        }

        // Check if this is a BASIC script
        if (self.currentScriptLanguage != "basic") {
            NSAlert* alert = [[NSAlert alloc] init];
            [alert setMessageText:@"Remove Numbers Not Available"];
            [alert setInformativeText:@"Line number removal is only available for BASIC scripts."];
            [alert addButtonWithTitle:@"OK"];
            [alert beginSheetModalForWindow:self.window completionHandler:nil];
            return;
        }

        // Get source code from editor
        std::string source_code = _textEditor->getText();

        if (source_code.empty()) {
            NSLog(@"[LuaBaseRunner] No code to process");
            return;
        }

        // Remove line numbers - not supported in LuaRunner
        NSLog(@"[LuaBaseRunner] Remove line numbers not supported for Lua");

        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Not Supported"];
        [alert setInformativeText:@"Line number removal is only for BASIC code"];
        [alert addButtonWithTitle:@"OK"];
        [alert beginSheetModalForWindow:self.window completionHandler:nil];
}



- (void)formatScript {
    [self formatScriptWithStartLine:1000 step:10];
}

- (void)formatScriptWithStartLine:(int)startLine step:(int)step {
    // Ensure we're on the main thread
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self formatScriptWithStartLine:startLine step:step];
        });
        return;
    }

    NSLog(@"[LuaBaseRunner] Formatting script");

    if (!_textEditor) {
        NSLog(@"[LuaBaseRunner] ERROR: No text editor available");
        return;
    }

    // Get current code
    std::string source_code = _textEditor->getText();

    if (source_code.empty()) {
        NSLog(@"[LuaBaseRunner] No code to format");
        return;
    }

    // Check language and format accordingly
    if (self.currentScriptLanguage == "lua") {
        // Format Lua code using LuaFormatter
        NSLog(@"[LuaBaseRunner] Formatting Lua code...");

        // Check if formatter is available
        if (!SuperTerminal::LuaFormatterWrapper::isAvailable()) {
            NSAlert* alert = [[NSAlert alloc] init];
            [alert setMessageText:@"Formatter Not Available"];
            [alert setInformativeText:@"The Lua code formatter is not available. Please check the build configuration."];
            [alert addButtonWithTitle:@"OK"];
            [alert beginSheetModalForWindow:self.window completionHandler:nil];
            return;
        }

        // Format the code with default settings
        auto result = SuperTerminal::LuaFormatterWrapper::formatWithDefaults(source_code);

        if (result.has_value()) {
            // Success - update the editor with formatted code
            std::string formatted_code = result.value();

            // Store cursor position
            size_t cursor_line = _textEditor->getCursorLine();
            size_t cursor_col = _textEditor->getCursorColumn();

            // Update text
            _textEditor->loadText(formatted_code);

            // Try to restore cursor position (may need adjustment if line count changed)
            if (cursor_line < _textEditor->getLineCount()) {
                _textEditor->setCursorPosition(cursor_line, cursor_col);
            } else {
                _textEditor->setCursorPosition(_textEditor->getLineCount() - 1, 0);
            }

            // Mark as modified
            if (_currentDocument) {
                _currentDocument->markDirty();
            }

            NSLog(@"[LuaBaseRunner] Lua code formatted successfully");

            // Show success notification
            NSAlert* alert = [[NSAlert alloc] init];
            [alert setMessageText:@"Format Complete"];
            [alert setInformativeText:@"Your Lua code has been formatted."];
            [alert addButtonWithTitle:@"OK"];
            [alert beginSheetModalForWindow:self.window completionHandler:nil];
        } else {
            // Error formatting
            std::string error = SuperTerminal::LuaFormatterWrapper::getLastError();
            NSLog(@"[LuaBaseRunner] ERROR: Failed to format Lua code: %s", error.c_str());

            NSAlert* alert = [[NSAlert alloc] init];
            [alert setMessageText:@"Format Error"];
            [alert setInformativeText:[NSString stringWithFormat:@"Failed to format Lua code:\n%s", error.c_str()]];
            [alert addButtonWithTitle:@"OK"];
            [alert setAlertStyle:NSAlertStyleWarning];
            [alert beginSheetModalForWindow:self.window completionHandler:nil];
        }
    } else if (self.currentScriptLanguage == "basic") {
        // Format BASIC code (not implemented in LuaBaseRunner)
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Format Not Available"];
        [alert setInformativeText:@"BASIC code formatting is only available in FBRunner3."];
        [alert addButtonWithTitle:@"OK"];
        [alert beginSheetModalForWindow:self.window completionHandler:nil];
    } else {
        // Unsupported language
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Format Not Available"];
        [alert setInformativeText:[NSString stringWithFormat:@"Code formatting is not available for %s scripts.", self.currentScriptLanguage.c_str()]];
        [alert addButtonWithTitle:@"OK"];
        [alert beginSheetModalForWindow:self.window completionHandler:nil];
    }
}

- (void)renumberScript {
    [self renumberScriptWithStartLine:1000 step:10];
}

- (void)renumberScriptWithStartLine:(int)startLine step:(int)step {
    // Ensure we're on the main thread
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self renumberScriptWithStartLine:startLine step:step];
        });
        return;
    }

    NSLog(@"[LuaBaseRunner] Renumbering script (start: %d, step: %d)", startLine, step);

    if (!_textEditor) {
        NSLog(@"[LuaBaseRunner] ERROR: No text editor available");
        return;
    }

    // Check if current language is BASIC
    if (self.currentScriptLanguage != "basic") {
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Renumber Not Available"];
        [alert setInformativeText:@"Code renumbering is only available for BASIC scripts."];
        [alert addButtonWithTitle:@"OK"];
        [alert beginSheetModalForWindow:self.window completionHandler:nil];
        return;
    }

    // Get current code
    std::string source_code = _textEditor->getText();

    if (source_code.empty()) {
        NSLog(@"[LuaBaseRunner] No code to renumber");
        return;
    }

    // Renumber - not supported in LuaRunner
    NSLog(@"[LuaBaseRunner] Renumber script not supported for Lua");

    NSAlert* alert = [[NSAlert alloc] init];
    [alert setMessageText:@"Not Supported"];
    [alert setInformativeText:@"Line renumbering is only for BASIC code"];
    [alert addButtonWithTitle:@"OK"];
    [alert beginSheetModalForWindow:self.window completionHandler:nil];
}

- (void)closeScript {
    NSLog(@"[LuaBaseRunner] Closing script");

    if (!_textEditor) return;

    // Don't prompt for unsaved changes - user is responsible for saving
    NSLog(@"[LuaBaseRunner] Closing script (no unsaved changes check)");

    [self enterRuntimeMode];
}

- (void)executeCurrentFile {
    NSLog(@"[LuaBaseRunner] === Execute current file (smart run/play) ===");

    if (!_currentDocument || !_currentDocument->isOpen()) {
        NSLog(@"[LuaBaseRunner] ERROR: No document open");
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:@"No Document Open"];
        [alert setInformativeText:@"Please open or create a document first."];
        [alert addButtonWithTitle:@"OK"];
        [alert beginSheetModalForWindow:self.window completionHandler:nil];
        return;
    }

    // Get language from Document (single source of truth)
    ScriptLanguage language = _currentDocument->getLanguage();
    std::string filename = _currentDocument->getName();

    NSLog(@"[LuaBaseRunner] Document: '%s', Language: %d", filename.c_str(), (int)language);

    // Decide whether to run or play based on file type
    if (language == ScriptLanguage::ABC || language == ScriptLanguage::VOICESCRIPT) {
        // Music files - play them
        NSLog(@"[LuaBaseRunner] Music file detected, playing...");
        [self playCurrentFile];
    } else if (language == ScriptLanguage::LUA) {
        // Lua files - run them
        NSLog(@"[LuaBaseRunner] Lua script detected, running...");
        [self runScript];
    } else if (language == ScriptLanguage::BASIC) {
        // BASIC files - show message that they need FBRunner3
        NSLog(@"[LuaBaseRunner] BASIC file detected");
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:@"BASIC Not Supported"];
        [alert setInformativeText:@"BASIC scripts can only be run in FBRunner3.\n\nThis is SuperTerminal Lua edition."];
        [alert addButtonWithTitle:@"OK"];
        [alert beginSheetModalForWindow:self.window completionHandler:nil];
    } else {
        // Unknown file type
        NSLog(@"[LuaBaseRunner] Unknown file type");
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:@"Cannot Execute"];
        [alert setInformativeText:@"This file type cannot be executed.\n\nSupported: Lua scripts, ABC music, VoiceScript"];
        [alert addButtonWithTitle:@"OK"];
        [alert beginSheetModalForWindow:self.window completionHandler:nil];
    }
}

- (void)playCurrentFile {
    NSLog(@"[LuaBaseRunner] === Playing current file ===");

    if (!_currentDocument || !_currentDocument->isOpen()) {
        NSLog(@"[LuaBaseRunner] ERROR: No document open");
        return;
    }

    // Get language from Document (single source of truth)
    ScriptLanguage language = _currentDocument->getLanguage();
    std::string filename = _currentDocument->getName();
    std::string content = _currentDocument->getText();

    NSLog(@"[LuaBaseRunner] Document: '%s', Language: %d", filename.c_str(), (int)language);

    // Handle ABC music files
    if (language == ScriptLanguage::ABC) {
        NSLog(@"[LuaBaseRunner] ABC file detected, playing music...");

        // Use the SuperTerminal audio API to play the ABC notation
        st_music_play(content.c_str());

        // Show status message
        if (_displayManager) {
            _displayManager->updateStatus("♫ Playing ABC music...");
        }

        NSLog(@"[LuaBaseRunner] ABC music playing");
        return;
    }

    // Handle VoiceScript files
    if (language == ScriptLanguage::VOICESCRIPT) {
        NSLog(@"[LuaBaseRunner] VoiceScript file detected, playing audio...");

        if (_audioManager) {
            // Load and compile the VoiceScript with a temporary name
            std::string scriptName = "__play_current__";
            std::string error;
            if (_audioManager->voiceScriptDefine(scriptName, content, error)) {
                // Play the script
                if (_audioManager->voiceScriptPlay(scriptName, 120.0f)) {
                    if (_displayManager) {
                        _displayManager->updateStatus("♫ Playing VoiceScript...");
                    }
                    NSLog(@"[LuaBaseRunner] VoiceScript audio playing");
                } else {
                    NSLog(@"[LuaBaseRunner] ERROR: Failed to play VoiceScript");
                    [self showError:@"Failed to play VoiceScript"];
                }
            } else {
                NSLog(@"[LuaBaseRunner] ERROR: Failed to compile VoiceScript: %s", error.c_str());
                [self showError:[NSString stringWithFormat:@"Failed to compile VoiceScript:\n%s", error.c_str()]];
            }
        } else {
            NSLog(@"[LuaBaseRunner] ERROR: AudioManager not available");
            [self showError:@"Audio system not available"];
        }
        return;
    }

    // Not a music file
    NSLog(@"[LuaBaseRunner] File is not a playable music file (language: %d)", (int)language);
    [self showError:@"Current file is not a playable music file.\nSupported formats: ABC, VoiceScript"];
}

- (void)runScript {
    NSLog(@"[LuaBaseRunner] === Running script (DOCUMENT-BASED) ===");

    if (!_currentDocument || !_currentDocument->isOpen() || !_textEditor) {
        NSLog(@"[LuaBaseRunner] ERROR: No document open or missing editor");
        return;
    }

    // Get language from Document (single source of truth)
    ScriptLanguage language = _currentDocument->getLanguage();
    std::string filename = _currentDocument->getName();

    NSLog(@"[LuaBaseRunner] Document: '%s', Language: %d", filename.c_str(), (int)language);

    // Handle ABC music files
    if (language == ScriptLanguage::ABC) {
        NSLog(@"[LuaBaseRunner] ABC file detected, playing music...");

        // Get content from Document
        std::string content = _currentDocument->getText();

        // Use the SuperTerminal audio API to play the ABC notation
        st_music_play(content.c_str());

        // Show status message
        if (_displayManager) {
            _displayManager->updateStatus("♫ Playing ABC music...");
        }

        // Stay in editor mode for ABC files
        return;
    }

    // For other script types, enter runtime mode
    // TODO: Execute script in runtime based on language
    // This will depend on the language runner implementation

    NSLog(@"[LuaBaseRunner] Entering runtime mode for language: %d", (int)language);
    [self enterRuntimeMode];
}

- (void)stopScript {
    NSLog(@"[LuaBaseRunner] Stopping script");

    // Clear all text display items to prevent stale GPU resource access
    NSLog(@"[LuaBaseRunner] Clearing text display items...");
    st_text_clear_displayed();
    NSLog(@"[LuaBaseRunner] Text display items cleared");

    // Reset display mode to text mode (mode 0)
    NSLog(@"[LuaBaseRunner] Resetting display mode to text mode...");
    st_mode(0);
    NSLog(@"[LuaBaseRunner] Display mode reset complete");

    // TODO: Stop script execution
}

- (void)exportContent {
    NSLog(@"[LuaBaseRunner] Export content");

    // Ensure we're on the main thread for UI operations
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self exportContent];
        });
        return;
    }

    // Determine what to export based on current state
    BOOL hasActiveCart = _cartManager && _cartManager->isCartActive();

    // Show export dialog to get description from user
    NSAlert* alert = [[NSAlert alloc] init];
    alert.messageText = hasActiveCart ? @"Export Cart Content" : @"Export Scripts Database";
    alert.informativeText = hasActiveCart ?
        @"Export the current cart's content and assets to your FasterBASIC folder." :
        @"Export all scripts from the database to your FasterBASIC folder.";
    alert.alertStyle = NSAlertStyleInformational;
    [alert addButtonWithTitle:@"Export"];
    [alert addButtonWithTitle:@"Cancel"];

    // Add description text field
    NSTextField* descriptionField = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 300, 24)];
    descriptionField.placeholderString = @"Optional description for this export";
    alert.accessoryView = descriptionField;

    NSModalResponse response = [alert runModal];
    if (response != NSAlertFirstButtonReturn) {
        return; // User cancelled
    }

    std::string description = std::string([descriptionField.stringValue UTF8String]);

    // Perform export in background
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        SuperTerminal::ExportResult result;

        if (hasActiveCart) {
            result = _exportImportManager->exportCartContent(_cartManager, description);
        } else {
            result = _exportImportManager->exportScriptsDatabase(_scriptDatabase, description);
        }

        // Show result on main thread
        dispatch_async(dispatch_get_main_queue(), ^{
            NSAlert* resultAlert = [[NSAlert alloc] init];
            resultAlert.messageText = result.success ? @"Export Successful" : @"Export Failed";
            resultAlert.informativeText = [NSString stringWithUTF8String:result.message.c_str()];
            resultAlert.alertStyle = result.success ? NSAlertStyleInformational : NSAlertStyleWarning;
            [resultAlert addButtonWithTitle:@"OK"];

            if (result.success && !result.exportPath.empty()) {
                [resultAlert addButtonWithTitle:@"Show in Finder"];
                NSModalResponse response = [resultAlert runModal];
                if (response == NSAlertSecondButtonReturn) {
                    // Show in Finder
                    NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:result.exportPath.c_str()]];
                    [[NSWorkspace sharedWorkspace] selectFile:url.path inFileViewerRootedAtPath:nil];
                }
            } else {
                [resultAlert runModal];
            }
        });
    });
}

- (void)importContent {
    NSLog(@"[LuaBaseRunner] Import content");

    // Ensure we're on the main thread for UI operations
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self importContent];
        });
        return;
    }

    // Get available exports
    auto availableExports = _exportImportManager->getAvailableExports();

    if (availableExports.empty()) {
        NSAlert* alert = [[NSAlert alloc] init];
        alert.messageText = @"No Exports Found";
        alert.informativeText = @"No export folders were found in your FasterBASIC directory. Use Export first to create backups.";
        alert.alertStyle = NSAlertStyleInformational;
        [alert addButtonWithTitle:@"OK"];
        [alert runModal];
        return;
    }

    // Create selection dialog
    NSAlert* alert = [[NSAlert alloc] init];
    alert.messageText = @"Select Export to Import";
    alert.informativeText = @"Choose which export folder to import from:";
    alert.alertStyle = NSAlertStyleInformational;
    [alert addButtonWithTitle:@"Import"];
    [alert addButtonWithTitle:@"Cancel"];

    // Create popup button with export options
    NSPopUpButton* popup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(0, 24, 400, 24)];

    for (const auto& exportFolder : availableExports) {
        std::string typeStr = (exportFolder.type == SuperTerminal::ExportType::SCRIPTS_DATABASE) ? "Scripts" : "Cart";
        std::string title = exportFolder.name + " (" + typeStr + ") - " +
                           std::to_string(exportFolder.itemCount) + " items";
        if (!exportFolder.description.empty()) {
            title += " - " + exportFolder.description;
        }

        NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:[NSString stringWithUTF8String:title.c_str()]
                                                      action:nil
                                               keyEquivalent:@""];
        [popup.menu addItem:item];
    }

    // Add checkbox for overwrite option
    NSButton* overwriteCheck = [[NSButton alloc] initWithFrame:NSMakeRect(0, 0, 300, 18)];
    [overwriteCheck setButtonType:NSButtonTypeSwitch];
    overwriteCheck.title = @"Overwrite existing items";
    overwriteCheck.state = NSControlStateValueOff;

    // Create container view
    NSView* accessoryView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 400, 48)];
    [accessoryView addSubview:popup];
    [accessoryView addSubview:overwriteCheck];
    alert.accessoryView = accessoryView;

    NSModalResponse response = [alert runModal];
    if (response != NSAlertFirstButtonReturn) {
        return; // User cancelled
    }

    NSInteger selectedIndex = [popup indexOfSelectedItem];
    if (selectedIndex < 0 || selectedIndex >= availableExports.size()) {
        return;
    }

    const auto& selectedExport = availableExports[selectedIndex];
    bool overwriteExisting = (overwriteCheck.state == NSControlStateValueOn);

    // Determine import type and get target for cart imports
    std::string targetCartPath;
    if (selectedExport.type == SuperTerminal::ExportType::CART_CONTENT) {
        // Ask user for cart file location
        NSSavePanel* savePanel = [NSSavePanel savePanel];
        savePanel.title = @"Create Cart File";
        savePanel.message = @"Choose location and name for the new cart file:";
        savePanel.allowedFileTypes = @[@"crt"];
        savePanel.nameFieldStringValue = @"imported_cart.crt";

        if ([savePanel runModal] != NSModalResponseOK) {
            return; // User cancelled
        }

        targetCartPath = std::string([savePanel.URL.path UTF8String]);
    }

    // Perform import in background
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        SuperTerminal::ImportResult result;

        if (selectedExport.type == SuperTerminal::ExportType::SCRIPTS_DATABASE) {
            result = _exportImportManager->importScriptsFromExport(selectedExport, _scriptDatabase, overwriteExisting);
        } else {
            result = _exportImportManager->importCartFromExport(selectedExport, _cartManager, targetCartPath);
        }

        // Show result on main thread
        dispatch_async(dispatch_get_main_queue(), ^{
            NSAlert* resultAlert = [[NSAlert alloc] init];
            resultAlert.messageText = result.success ? @"Import Successful" : @"Import Failed";

            NSMutableString* message = [NSMutableString stringWithUTF8String:result.message.c_str()];
            if (!result.warnings.empty()) {
                [message appendString:@"\n\nWarnings:"];
                for (const auto& warning : result.warnings) {
                    [message appendFormat:@"\n• %s", warning.c_str()];
                }
            }

            resultAlert.informativeText = message;
            resultAlert.alertStyle = result.success ? NSAlertStyleInformational : NSAlertStyleWarning;
            [resultAlert addButtonWithTitle:@"OK"];
            [resultAlert runModal];
        });
    });
}

- (void)clearOutput {
    NSLog(@"[LuaBaseRunner] Clearing output");

    if (_textGrid) {
        _textGrid->clear();
    }
    if (_graphicsLayer) {
        _graphicsLayer->clear();
    }
}

// =============================================================================
// Clipboard Support
// =============================================================================

- (void)cutText {
    if (_textEditor && self.editorMode) {
        _textEditor->cut();
    }
}

- (void)copyText {
    if (_textEditor && self.editorMode) {
        _textEditor->copy();
    }
}

- (void)pasteText {
    if (_textEditor && self.editorMode) {
        _textEditor->paste();
    }
}

// =============================================================================
// Find/Replace and Navigation
// =============================================================================

- (void)showFind:(id)sender {
    if (!_textEditor || !self.editorMode) {
        return;
    }

    // Get selected text as default search text
    NSString* defaultText = nil;
    if (_textEditor->hasSelection()) {
        std::string selected = _textEditor->getSelectedText();
        defaultText = [NSString stringWithUTF8String:selected.c_str()];
    }

    NSString* findText = nil;
    FindOptions options = FindOptionNone;

    if ([FindDialog showDialogWithDefaultText:defaultText
                                      outText:&findText
                                   outOptions:&options]) {
        [self performFind:findText options:options];
    }
}

- (void)findNext:(id)sender {
    // TODO: Track last search and repeat it
    NSLog(@"[LuaBaseRunner] Find next - not yet implemented (need to track last search)");
}

- (void)showFindReplace:(id)sender {
    if (!_textEditor || !self.editorMode) {
        return;
    }

    // Get selected text as default search text
    NSString* defaultFindText = nil;
    if (_textEditor->hasSelection()) {
        std::string selected = _textEditor->getSelectedText();
        defaultFindText = [NSString stringWithUTF8String:selected.c_str()];
    }

    NSString* findText = nil;
    NSString* replaceText = nil;
    FindOptions options = FindOptionNone;
    FindReplaceAction action = FindReplaceActionCancel;

    if ([FindReplaceDialog showDialogWithDefaultFindText:defaultFindText
                                     defaultReplaceText:nil
                                            outFindText:&findText
                                         outReplaceText:&replaceText
                                             outOptions:&options
                                              outAction:&action]) {
        [self performFindReplace:findText
                     replaceText:replaceText
                         options:options
                          action:action];
    }
}

- (void)showGotoLine:(id)sender {
    if (!_textEditor || !self.editorMode) {
        return;
    }

    NSInteger currentLine = (NSInteger)_textEditor->getCursorLine() + 1;
    NSInteger maxLine = (NSInteger)_textEditor->getLineCount();
    NSInteger targetLine = 0;

    if ([GotoLineDialog showDialogWithCurrentLine:currentLine
                                          maxLine:maxLine
                                          outLine:&targetLine]) {
        // Convert to 0-based and jump to line
        if (targetLine > 0 && targetLine <= maxLine) {
            _textEditor->setCursorPosition((size_t)(targetLine - 1), 0);
            NSLog(@"[LuaBaseRunner] Jumped to line %ld", (long)targetLine);
        }
    }
}

- (void)performFind:(NSString*)findText options:(FindOptions)options {
    if (!_textEditor || !findText) {
        return;
    }

    std::string searchText([findText UTF8String]);
    bool caseSensitive = (options & FindOptionCaseSensitive) != 0;
    bool wholeWord = (options & FindOptionWholeWord) != 0;
    bool wrapAround = (options & FindOptionWrapAround) != 0;

    // Use TextSearcher to find next occurrence
    TextBuffer* buffer = _textEditor->getTextBuffer();
    Cursor* cursor = _textEditor->getCursor();

    if (!buffer || !cursor) {
        return;
    }

    FindResult result = TextSearcher::findNext(*buffer,
                                              *cursor,
                                              searchText,
                                              caseSensitive,
                                              wholeWord,
                                              wrapAround,
                                              false);

    if (result.found) {
        // Select found text
        _textEditor->setCursorPosition(result.line, result.column);
        cursor->startSelection();
        _textEditor->setCursorPosition(result.line, result.column + result.length);
        NSLog(@"[LuaBaseRunner] Found at line %zu, column %zu", result.line + 1, result.column + 1);
    } else {
        NSBeep();
        NSLog(@"[LuaBaseRunner] Not found: %@", findText);
    }
}

- (void)performFindReplace:(NSString*)findText
               replaceText:(NSString*)replaceText
                   options:(FindOptions)options
                    action:(FindReplaceAction)action {
    if (!_textEditor || !findText || !replaceText) {
        return;
    }

    std::string searchText([findText UTF8String]);
    std::string replaceStr([replaceText UTF8String]);
    bool caseSensitive = (options & FindOptionCaseSensitive) != 0;
    bool wholeWord = (options & FindOptionWholeWord) != 0;

    TextBuffer* buffer = _textEditor->getTextBuffer();
    Cursor* cursor = _textEditor->getCursor();

    if (!buffer || !cursor) {
        return;
    }

    switch (action) {
        case FindReplaceActionFindNext: {
            bool wrapAround = (options & FindOptionWrapAround) != 0;
            FindResult result = TextSearcher::findNext(*buffer,
                                                      *cursor,
                                                      searchText,
                                                      caseSensitive,
                                                      wholeWord,
                                                      wrapAround,
                                                      false);
            if (result.found) {
                _textEditor->setCursorPosition(result.line, result.column);
                cursor->startSelection();
                _textEditor->setCursorPosition(result.line, result.column + result.length);
            } else {
                NSBeep();
            }
            break;
        }

        case FindReplaceActionReplace: {
            // Replace current selection if it matches
            if (_textEditor->hasSelection()) {
                std::string selected = _textEditor->getSelectedText();
                bool matches;
                if (caseSensitive) {
                    matches = (selected == searchText);
                } else {
                    // Case-insensitive comparison
                    std::string lowerSelected = selected;
                    std::string lowerSearch = searchText;
                    std::transform(lowerSelected.begin(), lowerSelected.end(), lowerSelected.begin(),
                                  [](unsigned char c) { return std::tolower(c); });
                    std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(),
                                  [](unsigned char c) { return std::tolower(c); });
                    matches = (lowerSelected == lowerSearch);
                }

                if (matches) {
                    _textEditor->insertText(replaceStr);
                }
            }
            // Find next
            [self performFind:findText options:options];
            break;
        }

        case FindReplaceActionReplaceAll: {
            int count = TextSearcher::replaceAll(*buffer,
                                                *cursor,
                                                searchText,
                                                replaceStr,
                                                caseSensitive,
                                                wholeWord);
            NSLog(@"[LuaBaseRunner] Replaced %d occurrence(s)", count);
            if (count == 0) {
                NSBeep();
            }
            break;
        }

        default:
            break;
    }
}

// =============================================================================
// Syntax Highlighting (Default Implementation)
// =============================================================================

- (std::vector<uint32_t>)highlightLine:(const std::string&)line
                            lineNumber:(size_t)lineNumber {
    // Default: no highlighting (return empty vector)
    // Subclasses should override to provide language-specific highlighting
    return std::vector<uint32_t>();
}

// =============================================================================
// Interactive View Mouse Handling (default implementation - override in subclass)
// =============================================================================

- (void)handleInteractiveMouseClick:(int)gridX y:(int)gridY button:(int)button {
    // Default implementation - subclasses can override
}

- (void)handleInteractiveMouseRelease {
    // Default implementation - subclasses can override
}

@end
