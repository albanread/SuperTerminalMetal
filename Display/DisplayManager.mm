//
// DisplayManager.mm
// SuperTerminal v2
//
// Display management implementation
//

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include "VideoMode/VideoModeManager.h"
#include "DisplayManager.h"
#include "TextGrid.h"
#include "GraphicsLayer.h"
#include "../Metal/MetalRenderer.h"
#include "../Input/InputManager.h"
#include "SpriteManager.h"
#include "RectangleManager.h"
#include "CircleManager.h"
#include "LineManager.h"
#include "PolygonManager.h"
#include "StarManager.h"
#include "LoResPaletteManager.h"
#include "XResPaletteManager.h"
#include "WResPaletteManager.h"
#include "PResPaletteManager.h"
#include "LoResBuffer.h"
#include "UResBuffer.h"
#include "XResBuffer.h"
#include "WResBuffer.h"
#include "PResBuffer.h"
#include "../Particles/ParticleSystem.h"
#include "../Tilemap/TilemapManager.h"
#include "../Tilemap/TilemapRenderer.h"
#include "../Startup/AppStartupStateMachine.h"
#include <chrono>
#include <thread>
#include <CoreVideo/CVDisplayLink.h>

// Window delegate to handle events (declared before STMetalView so it can be used in casts)
@interface STWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) SuperTerminal::DisplayManager* manager;
- (void)handleKeyDown:(NSEvent*)event;
- (void)handleKeyUp:(NSEvent*)event;
- (void)handleFlagsChanged:(NSEvent*)event;
- (void)handleMouseMoved:(NSEvent*)event;
- (void)handleMouseDown:(NSEvent*)event;
- (void)handleMouseUp:(NSEvent*)event;
- (void)handleRightMouseDown:(NSEvent*)event;
- (void)handleRightMouseUp:(NSEvent*)event;
- (void)handleScrollWheel:(NSEvent*)event;
@end

// Custom view that hosts the Metal layer
@interface STMetalView : NSView
@property (nonatomic, strong) CAMetalLayer* metalLayer;
@end

@implementation STMetalView

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.wantsLayer = YES;
        self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawDuringViewResize;

        _metalLayer = [CAMetalLayer layer];
        _metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        _metalLayer.framebufferOnly = YES;
        _metalLayer.frame = self.bounds;

        // Enable VSync and smooth rendering
        _metalLayer.displaySyncEnabled = YES;  // Sync to display refresh
        _metalLayer.presentsWithTransaction = NO;  // Allow Metal to control presentation timing
        _metalLayer.maximumDrawableCount = 3;  // Triple buffering for smooth rendering

        // Reduce latency
        if (@available(macOS 10.13.2, *)) {
            _metalLayer.allowsNextDrawableTimeout = NO;  // Don't timeout on nextDrawable
        }

        [self.layer addSublayer:_metalLayer];

        // Setup mouse tracking
        [self updateTrackingAreas];
    }
    return self;
}

- (void)setFrameSize:(NSSize)newSize {
    [super setFrameSize:newSize];
    _metalLayer.frame = self.bounds;
    _metalLayer.drawableSize = self.bounds.size;
    [self updateTrackingAreas];
}

- (void)viewDidChangeBackingProperties {
    [super viewDidChangeBackingProperties];

    NSScreen* screen = self.window.screen ? self.window.screen : [NSScreen mainScreen];
    CGFloat scale = screen.backingScaleFactor;
    _metalLayer.contentsScale = scale;

    CGSize drawableSize = self.bounds.size;
    drawableSize.width *= scale;
    drawableSize.height *= scale;
    _metalLayer.drawableSize = drawableSize;
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (BOOL)isOpaque {
    return YES;
}

- (void)updateTrackingAreas {
    // Remove old tracking areas
    for (NSTrackingArea* area in self.trackingAreas) {
        [self removeTrackingArea:area];
    }

    // Add new tracking area for entire view
    NSTrackingAreaOptions options = NSTrackingMouseEnteredAndExited |
                                    NSTrackingMouseMoved |
                                    NSTrackingActiveAlways |
                                    NSTrackingInVisibleRect;

    NSTrackingArea* trackingArea = [[NSTrackingArea alloc] initWithRect:self.bounds
                                                                 options:options
                                                                   owner:self
                                                                userInfo:nil];
    [self addTrackingArea:trackingArea];
}

- (void)keyDown:(NSEvent*)event {
    // Ignore auto-repeat events from macOS at the view level
    if (event.isARepeat) {
        return;
    }
    // Forward to window delegate
    if ([self.window.delegate isKindOfClass:[STWindowDelegate class]]) {
        [(STWindowDelegate*)self.window.delegate handleKeyDown:event];
    }
}

- (void)keyUp:(NSEvent*)event {
    // Forward to window delegate
    if ([self.window.delegate isKindOfClass:[STWindowDelegate class]]) {
        [(STWindowDelegate*)self.window.delegate handleKeyUp:event];
    }
}

- (void)flagsChanged:(NSEvent*)event {
    // Forward to window delegate
    if ([self.window.delegate isKindOfClass:[STWindowDelegate class]]) {
        [(STWindowDelegate*)self.window.delegate handleFlagsChanged:event];
    }
}

- (void)mouseMoved:(NSEvent*)event {
    if ([self.window.delegate isKindOfClass:[STWindowDelegate class]]) {
        [(STWindowDelegate*)self.window.delegate handleMouseMoved:event];
    }
}

- (void)mouseDragged:(NSEvent*)event {
    if ([self.window.delegate isKindOfClass:[STWindowDelegate class]]) {
        [(STWindowDelegate*)self.window.delegate handleMouseMoved:event];
    }
}

- (void)mouseDown:(NSEvent*)event {
    if ([self.window.delegate isKindOfClass:[STWindowDelegate class]]) {
        [(STWindowDelegate*)self.window.delegate handleMouseDown:event];
    }
}

- (void)mouseUp:(NSEvent*)event {
    if ([self.window.delegate isKindOfClass:[STWindowDelegate class]]) {
        [(STWindowDelegate*)self.window.delegate handleMouseUp:event];
    }
}

- (void)rightMouseDown:(NSEvent*)event {
    if ([self.window.delegate isKindOfClass:[STWindowDelegate class]]) {
        [(STWindowDelegate*)self.window.delegate handleRightMouseDown:event];
    }
}

- (void)rightMouseUp:(NSEvent*)event {
    if ([self.window.delegate isKindOfClass:[STWindowDelegate class]]) {
        [(STWindowDelegate*)self.window.delegate handleRightMouseUp:event];
    }
}

- (void)scrollWheel:(NSEvent*)event {
    if ([self.window.delegate isKindOfClass:[STWindowDelegate class]]) {
        [(STWindowDelegate*)self.window.delegate handleScrollWheel:event];
    }
}

@end

@implementation STWindowDelegate

- (void)windowWillClose:(NSNotification*)notification {
    if (_manager) {
        _manager->stop();
    }
}

- (void)windowDidResize:(NSNotification*)notification {
    // Window resize is handled by DisplayManager internally
    (void)notification;
}

- (void)handleKeyDown:(NSEvent*)event {
    if (_manager) {
        auto inputMgr = _manager->getInputManager();
        if (inputMgr) {
            SuperTerminal::KeyCode keyCode = SuperTerminal::InputManager::convertMacKeyCode(event.keyCode);
            inputMgr->handleKeyDown(keyCode);

            // Get modifier key states
            NSEventModifierFlags flags = event.modifierFlags;
            bool shift = (flags & NSEventModifierFlagShift) != 0;
            bool ctrl = (flags & NSEventModifierFlagControl) != 0;
            bool alt = (flags & NSEventModifierFlagOption) != 0;
            bool cmd = (flags & NSEventModifierFlagCommand) != 0;

            // Handle character input (but skip control characters from special keys)
            NSString* chars = event.characters;
            if (chars && chars.length > 0) {
                for (NSUInteger i = 0; i < chars.length; i++) {
                    unichar ch = [chars characterAtIndex:i];
                    // Only send printable characters (>= 32) or tab/newline
                    // Exclude Unicode private use area (0xF700-0xF8FF) used by macOS for arrow keys, function keys, etc.
                    if ((ch >= 32 && ch < 0xF700) || ch == 9 || ch == 10 || ch == 13) {
                        // Use new method that handles compose keys and Alt+number input
                        inputMgr->handleCharacterInputWithModifiers(ch, shift, ctrl, alt, cmd);
                    }
                }
            }
        }
    }
}

- (void)handleKeyUp:(NSEvent*)event {
    if (_manager) {
        auto inputMgr = _manager->getInputManager();
        if (inputMgr) {
            SuperTerminal::KeyCode keyCode = SuperTerminal::InputManager::convertMacKeyCode(event.keyCode);
            inputMgr->handleKeyUp(keyCode);
        }
    }
}

- (void)handleFlagsChanged:(NSEvent*)event {
    if (_manager) {
        auto inputMgr = _manager->getInputManager();
        if (inputMgr) {
            NSEventModifierFlags flags = event.modifierFlags;

            // Track modifier key states
            static NSEventModifierFlags lastFlags = 0;

            // Control key
            bool ctrlNow = (flags & NSEventModifierFlagControl) != 0;
            bool ctrlBefore = (lastFlags & NSEventModifierFlagControl) != 0;
            if (ctrlNow != ctrlBefore) {
                if (ctrlNow) {
                    inputMgr->handleKeyDown(SuperTerminal::KeyCode::LeftControl);
                } else {
                    inputMgr->handleKeyUp(SuperTerminal::KeyCode::LeftControl);
                }
            }

            // Shift key
            bool shiftNow = (flags & NSEventModifierFlagShift) != 0;
            bool shiftBefore = (lastFlags & NSEventModifierFlagShift) != 0;
            if (shiftNow != shiftBefore) {
                if (shiftNow) {
                    inputMgr->handleKeyDown(SuperTerminal::KeyCode::LeftShift);
                } else {
                    inputMgr->handleKeyUp(SuperTerminal::KeyCode::LeftShift);
                }
            }

            // Command key
            bool cmdNow = (flags & NSEventModifierFlagCommand) != 0;
            bool cmdBefore = (lastFlags & NSEventModifierFlagCommand) != 0;
            if (cmdNow != cmdBefore) {
                if (cmdNow) {
                    inputMgr->handleKeyDown(SuperTerminal::KeyCode::LeftCommand);
                } else {
                    inputMgr->handleKeyUp(SuperTerminal::KeyCode::LeftCommand);
                }
            }

            // Option/Alt key
            bool altNow = (flags & NSEventModifierFlagOption) != 0;
            bool altBefore = (lastFlags & NSEventModifierFlagOption) != 0;
            if (altNow != altBefore) {
                if (altNow) {
                    inputMgr->handleKeyDown(SuperTerminal::KeyCode::LeftAlt);
                } else {
                    inputMgr->handleKeyUp(SuperTerminal::KeyCode::LeftAlt);
                }
            }

            lastFlags = flags;
        }
    }
}

- (void)handleMouseMoved:(NSEvent*)event {
    if (_manager) {
        auto inputMgr = _manager->getInputManager();
        if (inputMgr) {
            // Update cell size first in case view has been resized
            _manager->updateInputManagerCellSize();

            // Get mouse location in window coordinates
            NSPoint location = [event locationInWindow];

            // Convert to Metal view coordinates (not content view, which includes status bar)
            NSView* metalView = _manager->getMetalView();
            if (metalView) {
                // Convert from window coordinates to Metal view coordinates (in POINTS)
                NSPoint viewLocation = [metalView convertPoint:location fromView:nil];

                // Flip Y coordinate: macOS origin is bottom-left, we want top-left
                CGFloat viewHeight = metalView.bounds.size.height;
                int mouseX = (int)viewLocation.x;
                int mouseY = (int)(viewHeight - viewLocation.y);

                inputMgr->handleMouseMove(mouseX, mouseY);
            }
        }
    }
}

- (void)handleMouseDown:(NSEvent*)event {
    // Check state machine - only process mouse events when RUNNING
    if (_manager && _manager->canProcessMouseEvents()) {
        // Update cell size first in case view has been resized
        _manager->updateInputManagerCellSize();

        NSPoint location = [event locationInWindow];

        // Convert to Metal view coordinates (not content view, which includes status bar)
        NSView* metalView = _manager->getMetalView();
        if (metalView) {
            // Convert from window coordinates to Metal view coordinates (in POINTS)
            NSPoint viewLocation = [metalView convertPoint:location fromView:nil];

            // Flip Y coordinate: macOS origin is bottom-left, we want top-left
            CGFloat viewHeight = metalView.bounds.size.height;
            int mouseX = (int)viewLocation.x;
            int mouseY = (int)(viewHeight - viewLocation.y);

            // Update InputManager with timestamp for double-click detection
            auto inputMgr = _manager->getInputManager();
            if (inputMgr) {
                inputMgr->handleMouseMove(mouseX, mouseY);
                double timestamp = [event timestamp];
                inputMgr->handleMouseButtonDownWithTime(SuperTerminal::MouseButton::Left, timestamp);
            }
        }
    }
}

- (void)handleMouseUp:(NSEvent*)event {
    // Check state machine - only process mouse events when RUNNING
    if (_manager && _manager->canProcessMouseEvents()) {
        NSPoint location = [event locationInWindow];

        // Convert to Metal view coordinates (not content view, which includes status bar)
        NSView* metalView = _manager->getMetalView();
        if (metalView) {
            // Convert from window coordinates to Metal view coordinates (in POINTS)
            NSPoint viewLocation = [metalView convertPoint:location fromView:nil];

            // Flip Y coordinate: macOS origin is bottom-left, we want top-left
            CGFloat viewHeight = metalView.bounds.size.height;
            int mouseX = (int)viewLocation.x;
            int mouseY = (int)(viewHeight - viewLocation.y);

            // Update InputManager
            auto inputMgr = _manager->getInputManager();
            if (inputMgr) {
                inputMgr->handleMouseMove(mouseX, mouseY);
                inputMgr->handleMouseButtonUp(SuperTerminal::MouseButton::Left);
            }
        }
    }
}

- (void)handleRightMouseDown:(NSEvent*)event {
    if (_manager) {
        auto inputMgr = _manager->getInputManager();
        if (inputMgr) {
            // Update mouse position on right click too
            NSPoint location = [event locationInWindow];

            // Convert to Metal view coordinates (not content view, which includes status bar)
            NSView* metalView = _manager->getMetalView();
            if (metalView) {
                // Convert from window coordinates to Metal view coordinates (in POINTS)
                NSPoint viewLocation = [metalView convertPoint:location fromView:nil];

                // Flip Y coordinate: macOS origin is bottom-left, we want top-left
                CGFloat viewHeight = metalView.bounds.size.height;
                int mouseX = (int)viewLocation.x;
                int mouseY = (int)(viewHeight - viewLocation.y);

                inputMgr->handleMouseMove(mouseX, mouseY);
            }
            inputMgr->handleMouseButtonDown(SuperTerminal::MouseButton::Right);
        }
    }
}

- (void)handleRightMouseUp:(NSEvent*)event {
    if (_manager) {
        auto inputMgr = _manager->getInputManager();
        if (inputMgr) {
            // Update mouse position on right release too
            NSPoint location = [event locationInWindow];

            // Convert to Metal view coordinates (not content view, which includes status bar)
            NSView* metalView = _manager->getMetalView();
            if (metalView) {
                // Convert from window coordinates to Metal view coordinates (in POINTS)
                NSPoint viewLocation = [metalView convertPoint:location fromView:nil];

                // Flip Y coordinate: macOS origin is bottom-left, we want top-left
                CGFloat viewHeight = metalView.bounds.size.height;
                int mouseX = (int)viewLocation.x;
                int mouseY = (int)(viewHeight - viewLocation.y);
                inputMgr->handleMouseMove(mouseX, mouseY);
            }
            inputMgr->handleMouseButtonUp(SuperTerminal::MouseButton::Right);
        }
    }
}

- (void)handleScrollWheel:(NSEvent*)event {
    if (_manager) {
        auto inputMgr = _manager->getInputManager();
        if (inputMgr) {
            inputMgr->handleMouseWheel(event.scrollingDeltaX, event.scrollingDeltaY);
        }
    }
}

@end

namespace SuperTerminal {

// Pimpl implementation
struct DisplayManager::Impl {
    NSWindow* window;
    STMetalView* metalView;
    STWindowDelegate* windowDelegate;
    NSView* containerView;
    NSView* statusBar;
    NSTextField* statusLabel;
    NSTextField* cursorLabel;
    NSTextField* scriptLabel;
    NSString* lastScriptName;  // Cache to prevent unnecessary updates

    std::shared_ptr<TextGrid> textGrid;
    std::shared_ptr<MetalRenderer> renderer;
    std::shared_ptr<GraphicsLayer> graphicsLayer;
    std::shared_ptr<InputManager> inputManager;
    std::shared_ptr<SpriteManager> spriteManager;
    std::shared_ptr<RectangleManager> rectangleManager;
    std::shared_ptr<CircleManager> circleManager;
    std::shared_ptr<LineManager> lineManager;
    std::shared_ptr<PolygonManager> polygonManager;
    std::shared_ptr<StarManager> starManager;
    std::shared_ptr<LoResPaletteManager> loresPalette;
    std::shared_ptr<XResPaletteManager> xresPalette;
    std::shared_ptr<WResPaletteManager> wresPalette;
    std::shared_ptr<PResPaletteManager> presPalette;
    std::shared_ptr<LoResBuffer> loResBuffers[8];       // LORES Buffers 0-7 (0=front, 1=back, 2-7=atlas/scratch)
    std::shared_ptr<UResBuffer> uresBuffers[8];         // URES Buffers 0-7 (0=front, 1=back, 2-7=atlas/scratch)
    std::shared_ptr<XResBuffer> xresBuffers[8];         // XRES Buffers 0-7 (0=front, 1=back, 2-7=atlas/scratch)
    std::shared_ptr<WResBuffer> wresBuffers[8];         // WRES Buffers 0-7 (0=front, 1=back, 2-7=atlas/scratch)
    std::shared_ptr<PResBuffer> presBuffers[8];         // PRES Buffers 0-7 (0=front, 1=back, 2-7=atlas/scratch)
    int activeBufferID;                                  // Currently active buffer for drawing (0 or 1) - legacy
    int activeLoResBufferID;                             // Currently active LORES buffer for drawing (0-7)
    int activeUResBufferID;                              // Currently active URES buffer for drawing (0-7)
    int activeXResBufferID;                              // Currently active XRES buffer for drawing (0-7)
    int activeWResBufferID;                              // Currently active WRES buffer for drawing (0-7)
    int activePResBufferID;                              // Currently active PRES buffer for drawing (0-7)
    int displayBufferID;                                 // Currently displayed buffer (0 or 1) - legacy
    std::shared_ptr<TilemapManager> tilemapManager;
    std::shared_ptr<TilemapRenderer> tilemapRenderer;
    std::unique_ptr<VideoModeManager> videoModeManager;

    DisplayConfig config;

    FrameCallback frameCallback;
    ResizeCallback resizeCallback;
    KeyCallback keyCallback;
    MouseCallback mouseCallback;

    // Startup state machine for thread-safe initialization
    std::shared_ptr<AppStartupStateMachine> startupStateMachine;

    bool initialized;
    bool running;
    bool readyForCallbacks;  // Set to true only after render loop starts
    bool useCVDisplayLink;   // Use CVDisplayLink for rendering (recommended)
    bool loResMode;          // true = LORES mode, false = TEXT mode
    bool uresMode;           // true = URES mode (1280×720 direct color)
    bool xresMode;           // true = XRES mode (320×240 256-color Mode X)
    bool wresMode;           // true = WRES mode (432×240 256-color Wide Mode)
    bool presMode;           // true = PRES mode (1280×720 256-color Premium Mode)

    // CVDisplayLink for hardware-synchronized rendering
    CVDisplayLinkRef displayLink;
    std::atomic<bool> displayLinkRunning;
    dispatch_queue_t renderQueue;  // Serial queue for rendering to ensure thread consistency

    // Timing
    std::chrono::high_resolution_clock::time_point lastFrameTime;
    float deltaTime;
    float frameTime;
    float currentFPS;
    uint32_t frameCount;
    float fpsUpdateTimer;

    Impl()
        : window(nil)
        , metalView(nil)
        , windowDelegate(nil)
        , containerView(nil)
        , statusBar(nil)
        , statusLabel(nil)
        , cursorLabel(nil)
        , scriptLabel(nil)
        , lastScriptName(nil)
        , initialized(false)
        , running(false)
        , readyForCallbacks(false)
        , useCVDisplayLink(true)
        , loResMode(false)
        , uresMode(false)
        , xresMode(false)
        , wresMode(false)
        , presMode(false)
        , activeBufferID(0)
        , activeLoResBufferID(0)
        , activeUResBufferID(0)
        , activeXResBufferID(0)
        , activeWResBufferID(0)
        , activePResBufferID(0)
        , displayBufferID(0)
        , displayLink(nullptr)
        , displayLinkRunning(false)
        , renderQueue(dispatch_queue_create("com.superterminal.renderqueue", DISPATCH_QUEUE_SERIAL))
        , deltaTime(0.0f)
        , frameTime(0.0f)
        , currentFPS(0.0f)
        , frameCount(0)
        , fpsUpdateTimer(0.0f)
        , videoModeManager(std::make_unique<VideoModeManager>())
    {
        lastFrameTime = std::chrono::high_resolution_clock::now();
    }

    ~Impl() {
        // Stop and release CVDisplayLink
        if (displayLink) {
            if (CVDisplayLinkIsRunning(displayLink)) {
                CVDisplayLinkStop(displayLink);
            }
            CVDisplayLinkRelease(displayLink);
            displayLink = nullptr;
        }

        // Release render queue
        if (renderQueue) {
            // drain queue before releasing
            dispatch_sync(renderQueue, ^{});
        }

        if (window) {
            [window close];
            window = nil;
        }
        metalView = nil;
        windowDelegate = nil;
    }
};

// CVDisplayLink callback function (called on separate thread)
static CVReturn DisplayLinkCallback(CVDisplayLinkRef displayLink,
                                   const CVTimeStamp* now,
                                   const CVTimeStamp* outputTime,
                                   CVOptionFlags flagsIn,
                                   CVOptionFlags* flagsOut,
                                   void* displayLinkContext) {
    // Get DisplayManager from context
    SuperTerminal::DisplayManager* manager = static_cast<SuperTerminal::DisplayManager*>(displayLinkContext);

    // Dispatch rendering to serial queue for consistent thread execution
    // This prevents race conditions and ensures proper Metal synchronization
    dispatch_queue_t queue = manager->getRenderQueue();
    dispatch_async(queue, ^{
        @autoreleasepool {
            manager->renderFrame();
        }
    });

    return kCVReturnSuccess;
}

DisplayManager::DisplayManager()
    : m_impl(std::make_unique<Impl>())
{
    // Initialize LORES palette manager and buffers (8 buffers for GPU operations)
    m_impl->loresPalette = std::make_shared<LoResPaletteManager>();
    for (int i = 0; i < 8; i++) {
        m_impl->loResBuffers[i] = std::make_shared<LoResBuffer>();  // LORES Buffers 0-7
    }

    // Initialize URES buffers (8 buffers for GPU operations)
    for (int i = 0; i < 8; i++) {
        m_impl->uresBuffers[i] = std::make_shared<UResBuffer>();    // URES Buffers 0-7
    }

    // Initialize XRES palette manager and buffers (8 buffers for double buffering + atlas space)
    m_impl->xresPalette = std::make_shared<XResPaletteManager>();
    for (int i = 0; i < 8; i++) {
        m_impl->xresBuffers[i] = std::make_shared<XResBuffer>();    // XRES Buffers 0-7
    }

    // Initialize WRES palette manager and buffers (8 buffers for double buffering + atlas space)
    m_impl->wresPalette = std::make_shared<WResPaletteManager>();
    for (int i = 0; i < 8; i++) {
        m_impl->wresBuffers[i] = std::make_shared<WResBuffer>();    // WRES Buffers 0-7
    }

    // Initialize PRES palette manager and buffers (8 buffers for double buffering + atlas space)
    m_impl->presPalette = std::make_shared<PResPaletteManager>();
    for (int i = 0; i < 8; i++) {
        m_impl->presBuffers[i] = std::make_shared<PResBuffer>();    // PRES Buffers 0-7
    }
}

DisplayManager::~DisplayManager() {
    stop();
}

DisplayManager::DisplayManager(DisplayManager&&) noexcept = default;
DisplayManager& DisplayManager::operator=(DisplayManager&&) noexcept = default;

bool DisplayManager::initialize(const DisplayConfig& config) {
    if (m_impl->initialized) {
        return true;
    }

    m_impl->config = config;

    @autoreleasepool {
        NSLog(@"[DisplayManager] initialize: Creating window...");
        // Create window
        NSRect frame = NSMakeRect(0, 0, config.windowWidth, config.windowHeight);

        NSWindowStyleMask styleMask = NSWindowStyleMaskTitled
                                     | NSWindowStyleMaskClosable
                                     | NSWindowStyleMaskMiniaturizable
                                     | NSWindowStyleMaskResizable;

        m_impl->window = [[NSWindow alloc] initWithContentRect:frame
                                                     styleMask:styleMask
                                                       backing:NSBackingStoreBuffered
                                                         defer:NO];
        NSLog(@"[DisplayManager] initialize: Window created");

        // Enable mouse moved events for the window
        [m_impl->window setAcceptsMouseMovedEvents:YES];

        if (!m_impl->window) {
            NSLog(@"[DisplayManager] initialize: ERROR - Window is nil");
            return false;
        }

        // DON'T make window visible here - causes hangs
        // Window will be shown later in BaseRunner after everything is initialized
        NSLog(@"[DisplayManager] initialize: Window created (hidden)");

        // DON'T set title - it causes hangs during initialization
        // Center the window now (safe operation)
        [m_impl->window center];
        NSLog(@"[DisplayManager] initialize: Window centered");

        NSLog(@"[DisplayManager] initialize: Creating container view...");
        // Create a container view to hold Metal view and status bar
        m_impl->containerView = [[NSView alloc] initWithFrame:frame];
        [m_impl->containerView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        [m_impl->window setContentView:m_impl->containerView];
        NSLog(@"[DisplayManager] initialize: Container view created");

        NSLog(@"[DisplayManager] initialize: Creating status bar...");
        // Create status bar at bottom (v1 pattern for Metal view coexistence)
        const CGFloat statusBarHeight = 22.0;
        NSRect statusBarFrame = NSMakeRect(0, 0, frame.size.width, statusBarHeight);
        m_impl->statusBar = [[NSView alloc] initWithFrame:statusBarFrame];
        [m_impl->statusBar setAutoresizingMask:NSViewWidthSizable | NSViewMaxYMargin];
        [m_impl->statusBar setWantsLayer:YES];
        m_impl->statusBar.layer.backgroundColor = [[NSColor colorWithWhite:0.2 alpha:1.0] CGColor];

        // Status field (left) - wider to accommodate longer filenames
        NSRect statusLabelFrame = NSMakeRect(8, 2, 650, 18);
        m_impl->statusLabel = [[NSTextField alloc] initWithFrame:statusLabelFrame];
        [m_impl->statusLabel setStringValue:@"● Ready"];
        [m_impl->statusLabel setBezeled:NO];
        [m_impl->statusLabel setDrawsBackground:NO];
        [m_impl->statusLabel setEditable:NO];
        [m_impl->statusLabel setSelectable:NO];
        [m_impl->statusLabel setTextColor:[NSColor whiteColor]];
        [m_impl->statusLabel setFont:[NSFont systemFontOfSize:11]];
        [m_impl->statusLabel setAutoresizingMask:NSViewMaxXMargin];
        [m_impl->statusLabel setLineBreakMode:NSLineBreakByTruncatingTail];
        [[m_impl->statusLabel cell] setUsesSingleLineMode:YES];
        [m_impl->statusBar addSubview:m_impl->statusLabel];
        NSLog(@"[DisplayManager] initialize: Status label added");

        // Cursor position field (center)
        NSRect cursorLabelFrame = NSMakeRect(frame.size.width/2 - 75, 2, 150, 18);
        m_impl->cursorLabel = [[NSTextField alloc] initWithFrame:cursorLabelFrame];
        [m_impl->cursorLabel setStringValue:@"Ln 1, Col 1"];
        [m_impl->cursorLabel setBezeled:NO];
        [m_impl->cursorLabel setDrawsBackground:NO];
        [m_impl->cursorLabel setEditable:NO];
        [m_impl->cursorLabel setSelectable:NO];
        [m_impl->cursorLabel setTextColor:[NSColor whiteColor]];
        [m_impl->cursorLabel setFont:[NSFont systemFontOfSize:11]];
        [m_impl->cursorLabel setAlignment:NSTextAlignmentCenter];
        [m_impl->cursorLabel setAutoresizingMask:NSViewMinXMargin | NSViewMaxXMargin];
        [m_impl->statusBar addSubview:m_impl->cursorLabel];
        NSLog(@"[DisplayManager] initialize: Cursor label added");

        // Script name field (right) - wider to prevent truncation flashing
        NSRect scriptLabelFrame = NSMakeRect(frame.size.width - 408, 2, 400, 18);
        m_impl->scriptLabel = [[NSTextField alloc] initWithFrame:scriptLabelFrame];
        [m_impl->scriptLabel setStringValue:@""];
        [m_impl->scriptLabel setBezeled:NO];
        [m_impl->scriptLabel setDrawsBackground:NO];
        [m_impl->scriptLabel setEditable:NO];
        [m_impl->scriptLabel setSelectable:NO];
        [m_impl->scriptLabel setTextColor:[NSColor whiteColor]];
        [m_impl->scriptLabel setFont:[NSFont systemFontOfSize:11]];
        [m_impl->scriptLabel setAlignment:NSTextAlignmentRight];
        [m_impl->scriptLabel setAutoresizingMask:NSViewMinXMargin];
        [m_impl->scriptLabel setLineBreakMode:NSLineBreakByTruncatingTail];
        [[m_impl->scriptLabel cell] setUsesSingleLineMode:YES];
        [m_impl->statusBar addSubview:m_impl->scriptLabel];
        NSLog(@"[DisplayManager] initialize: Script label added");

        NSLog(@"[DisplayManager] initialize: Creating Metal view...");
        // Create Metal view above status bar (v1 pattern)
        NSRect metalFrame = NSMakeRect(0, statusBarHeight, frame.size.width, frame.size.height - statusBarHeight);
        m_impl->metalView = [[STMetalView alloc] initWithFrame:metalFrame];
        [m_impl->metalView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        NSLog(@"[DisplayManager] initialize: Metal view created");

        NSLog(@"[DisplayManager] initialize: Adding views to container...");
        // Add views to container (status bar first, then Metal view on top)
        [m_impl->containerView addSubview:m_impl->statusBar];
        [m_impl->containerView addSubview:m_impl->metalView];
        NSLog(@"[DisplayManager] initialize: Views added to container");

        NSLog(@"[DisplayManager] initialize: Setting up window delegate...");
        // Set up window delegate
        m_impl->windowDelegate = [[STWindowDelegate alloc] init];
        m_impl->windowDelegate.manager = this;
        [m_impl->window setDelegate:m_impl->windowDelegate];
        NSLog(@"[DisplayManager] initialize: Window delegate set");

        // Enable mouse tracking
        [m_impl->window setAcceptsMouseMovedEvents:YES];
        [m_impl->window makeFirstResponder:m_impl->metalView];

        NSLog(@"[DisplayManager] initialize: Configuring backing scale...");
        // Configure backing scale
        NSScreen* screen = m_impl->window.screen ? m_impl->window.screen : [NSScreen mainScreen];
        CGFloat scale = screen.backingScaleFactor;
        m_impl->metalView.metalLayer.contentsScale = scale;

        CGSize drawableSize = m_impl->metalView.bounds.size;
        drawableSize.width *= scale;
        drawableSize.height *= scale;
        m_impl->metalView.metalLayer.drawableSize = drawableSize;
        NSLog(@"[DisplayManager] initialize: Backing scale configured");

        // Setup CVDisplayLink for hardware-synchronized rendering
        if (m_impl->useCVDisplayLink) {
            CVReturn cvReturn = CVDisplayLinkCreateWithActiveCGDisplays(&m_impl->displayLink);
            if (cvReturn != kCVReturnSuccess) {
                NSLog(@"[DisplayManager] WARNING: Failed to create CVDisplayLink, falling back to manual timing");
                m_impl->useCVDisplayLink = false;
                m_impl->displayLink = nullptr;
            } else {
                // Set callback
                CVDisplayLinkSetOutputCallback(m_impl->displayLink, &DisplayLinkCallback, this);

                // Set to match the screen the window is on
                CGDirectDisplayID displayID = (CGDirectDisplayID)[[[m_impl->window screen]
                                                                   deviceDescription][@"NSScreenNumber"]
                                                                   unsignedIntValue];
                CVDisplayLinkSetCurrentCGDisplay(m_impl->displayLink, displayID);

                NSLog(@"[DisplayManager] CVDisplayLink initialized successfully");
            }
        }

        m_impl->initialized = true;

        // Initialize VideoModeManager with this DisplayManager reference
        if (m_impl->videoModeManager) {
            m_impl->videoModeManager->setDisplayManager(this);
        }

        NSLog(@"[DisplayManager] initialize: COMPLETE - returning true");
        return true;
    }
}

void DisplayManager::setTextGrid(std::shared_ptr<TextGrid> textGrid) {
    m_impl->textGrid = textGrid;
}

void DisplayManager::setRenderer(std::shared_ptr<MetalRenderer> renderer) {
    m_impl->renderer = renderer;

    if (renderer && m_impl->metalView) {
        // Initialize renderer with the Metal layer
        NSRect bounds = m_impl->metalView.bounds;
        NSScreen* screen = m_impl->window.screen ? m_impl->window.screen : [NSScreen mainScreen];
        CGFloat scale = screen.backingScaleFactor;

        uint32_t width = static_cast<uint32_t>(bounds.size.width * scale);
        uint32_t height = static_cast<uint32_t>(bounds.size.height * scale);

        renderer->initialize(m_impl->metalView.metalLayer, width, height);
    }
}

std::shared_ptr<TextGrid> DisplayManager::getTextGrid() const {
    return m_impl->textGrid;
}

std::shared_ptr<MetalRenderer> DisplayManager::getRenderer() const {
    return m_impl->renderer;
}

void DisplayManager::setGraphicsLayer(std::shared_ptr<GraphicsLayer> graphicsLayer) {
    m_impl->graphicsLayer = graphicsLayer;
}

std::shared_ptr<GraphicsLayer> DisplayManager::getGraphicsLayer() const {
    return m_impl->graphicsLayer;
}

void DisplayManager::setFrameCallback(FrameCallback callback) {
    m_impl->frameCallback = callback;
}

void DisplayManager::setResizeCallback(ResizeCallback callback) {
    m_impl->resizeCallback = callback;
}

void DisplayManager::setKeyCallback(KeyCallback callback) {
    m_impl->keyCallback = callback;
}

void DisplayManager::setMouseCallback(MouseCallback callback) {
    m_impl->mouseCallback = callback;
}

void DisplayManager::invokeMouseCallback(int gridX, int gridY, int button, bool pressed) {
    // Safety check: only invoke if ready (prevents race conditions during startup)
    if (!m_impl->readyForCallbacks) {
        return;
    }

    if (m_impl->mouseCallback) {
        m_impl->mouseCallback(gridX, gridY, button, pressed);
    }
}

void DisplayManager::setInputManager(std::shared_ptr<InputManager> inputManager) {
    m_impl->inputManager = inputManager;

    // Configure cell size for grid coordinate conversion
    // IMPORTANT: Must use Metal view bounds in POINTS to match mouse event coordinate space
    if (inputManager && m_impl->textGrid && m_impl->metalView) {
        NSRect metalBounds = m_impl->metalView.bounds;  // In points, not backing pixels

        int cols = m_impl->textGrid->getWidth();
        int rows = m_impl->textGrid->getHeight();
        float cellWidth = metalBounds.size.width / cols;
        float cellHeight = metalBounds.size.height / rows;

        inputManager->setCellSize(cellWidth, cellHeight);
    }
}

std::shared_ptr<InputManager> DisplayManager::getInputManager() const {
    return m_impl->inputManager;
}

void DisplayManager::updateInputManagerCellSize() {
    if (m_impl->inputManager && m_impl->textGrid && m_impl->metalView) {
        NSRect metalBounds = m_impl->metalView.bounds;  // In points, not backing pixels

        int cols = m_impl->textGrid->getWidth();
        int rows = m_impl->textGrid->getHeight();
        float cellWidth = metalBounds.size.width / cols;
        float cellHeight = metalBounds.size.height / rows;

        m_impl->inputManager->setCellSize(cellWidth, cellHeight);
    }
}

void DisplayManager::setSpriteManager(std::shared_ptr<SpriteManager> spriteManager) {
    m_impl->spriteManager = spriteManager;

    // Connect particle system to sprite manager for sprite-based explosions
    if (spriteManager && st_particle_system_is_ready()) {
        st_particle_system_set_sprite_manager(spriteManager.get());
    }
}

std::shared_ptr<SpriteManager> DisplayManager::getSpriteManager() const {
    return m_impl->spriteManager;
}

void DisplayManager::setRectangleManager(std::shared_ptr<RectangleManager> rectangleManager) {
    m_impl->rectangleManager = rectangleManager;
}

std::shared_ptr<RectangleManager> DisplayManager::getRectangleManager() const {
    return m_impl->rectangleManager;
}

void DisplayManager::setCircleManager(std::shared_ptr<CircleManager> circleManager) {
    m_impl->circleManager = circleManager;
}

std::shared_ptr<CircleManager> DisplayManager::getCircleManager() const {
    return m_impl->circleManager;
}

void DisplayManager::setLineManager(std::shared_ptr<LineManager> lineManager) {
    m_impl->lineManager = lineManager;
}

std::shared_ptr<LineManager> DisplayManager::getLineManager() const {
    return m_impl->lineManager;
}

void DisplayManager::setPolygonManager(std::shared_ptr<PolygonManager> polygonManager) {
    m_impl->polygonManager = polygonManager;
}

std::shared_ptr<PolygonManager> DisplayManager::getPolygonManager() const {
    return m_impl->polygonManager;
}

void DisplayManager::setStarManager(std::shared_ptr<StarManager> starManager) {
    m_impl->starManager = starManager;
}

std::shared_ptr<StarManager> DisplayManager::getStarManager() const {
    return m_impl->starManager;
}

std::shared_ptr<LoResPaletteManager> DisplayManager::getLoresPalette() const {
    return m_impl->loresPalette;
}

std::shared_ptr<XResPaletteManager> DisplayManager::getXresPalette() const {
    return m_impl->xresPalette;
}

std::shared_ptr<WResPaletteManager> DisplayManager::getWresPalette() const {
    return m_impl->wresPalette;
}

std::shared_ptr<PResPaletteManager> DisplayManager::getPresPalette() const {
    return m_impl->presPalette;
}

VideoModeManager* DisplayManager::getVideoModeManager() const {
    return m_impl->videoModeManager.get();
}

std::shared_ptr<LoResBuffer> DisplayManager::getLoResBuffer() const {
    return m_impl->loResBuffers[0];
}

void DisplayManager::setLoResMode(bool isLoResMode) {
    m_impl->loResMode = isLoResMode;
    if (isLoResMode) {
        m_impl->uresMode = false;   // Can't be both LORES and URES
        m_impl->xresMode = false;   // Can't be both LORES and XRES
        m_impl->wresMode = false;   // Can't be both LORES and WRES
        m_impl->presMode = false;   // Can't be both LORES and PRES
    }
}

bool DisplayManager::isLoResMode() const {
    return m_impl->loResMode;
}

void DisplayManager::setUResMode(bool isUResMode) {
    m_impl->uresMode = isUResMode;
    if (isUResMode) {
        m_impl->loResMode = false;  // Can't be both URES and LORES
        m_impl->xresMode = false;   // Can't be both URES and XRES
        m_impl->wresMode = false;   // Can't be both URES and WRES
        m_impl->presMode = false;   // Can't be both URES and PRES
    }
}

bool DisplayManager::isUResMode() const {
    return m_impl->uresMode;
}

std::shared_ptr<LoResBuffer> DisplayManager::getLoResBuffer(int bufferID) const {
    if (bufferID >= 0 && bufferID < 8) {
        return m_impl->loResBuffers[bufferID];
    }
    return nullptr;
}

std::shared_ptr<UResBuffer> DisplayManager::getUResBuffer() const {
    return m_impl->uresBuffers[0];
}

std::shared_ptr<UResBuffer> DisplayManager::getUResBuffer(int bufferID) const {
    if (bufferID >= 0 && bufferID < 8) {
        return m_impl->uresBuffers[bufferID];
    }
    return nullptr;
}

std::shared_ptr<XResBuffer> DisplayManager::getXResBuffer() const {
    return m_impl->xresBuffers[0];
}

std::shared_ptr<XResBuffer> DisplayManager::getXResBuffer(int bufferID) const {
    if (bufferID >= 0 && bufferID < 8) {
        return m_impl->xresBuffers[bufferID];
    }
    return nullptr;
}

void DisplayManager::setActiveXResBuffer(int bufferID) {
    if (bufferID >= 0 && bufferID < 8) {
        m_impl->activeXResBufferID = bufferID;
    }
}

int DisplayManager::getActiveXResBuffer() const {
    return m_impl->activeXResBufferID;
}

void DisplayManager::flipXResBuffers() {
    // Swap buffers 0 and 1
    std::swap(m_impl->xresBuffers[0], m_impl->xresBuffers[1]);
}

void DisplayManager::setActiveLoResBuffer(int bufferID) {
    if (bufferID >= 0 && bufferID < 8) {
        m_impl->activeLoResBufferID = bufferID;
    }
}

int DisplayManager::getActiveLoResBuffer() const {
    return m_impl->activeLoResBufferID;
}

void DisplayManager::flipLoResBuffers() {
    // Swap buffers 0 and 1
    std::swap(m_impl->loResBuffers[0], m_impl->loResBuffers[1]);
}

void DisplayManager::setActiveUResBuffer(int bufferID) {
    if (bufferID >= 0 && bufferID < 8) {
        m_impl->activeUResBufferID = bufferID;
    }
}

int DisplayManager::getActiveUResBuffer() const {
    return m_impl->activeUResBufferID;
}

void DisplayManager::flipUResBuffers() {
    // Swap buffers 0 and 1
    std::swap(m_impl->uresBuffers[0], m_impl->uresBuffers[1]);
}

void DisplayManager::setXResMode(bool isXResMode) {
    m_impl->xresMode = isXResMode;
    if (isXResMode) {
        m_impl->loResMode = false;  // Can't be both XRES and LORES
        m_impl->uresMode = false;   // Can't be both XRES and URES
        m_impl->wresMode = false;   // Can't be both XRES and WRES
        m_impl->presMode = false;   // Can't be both XRES and PRES
    }
}

bool DisplayManager::isXResMode() const {
    return m_impl->xresMode;
}

// WRES Mode Management
std::shared_ptr<WResBuffer> DisplayManager::getWResBuffer() const {
    return m_impl->wresBuffers[0];
}

std::shared_ptr<WResBuffer> DisplayManager::getWResBuffer(int bufferID) const {
    if (bufferID >= 0 && bufferID < 8) {
        return m_impl->wresBuffers[bufferID];
    }
    return nullptr;
}

void DisplayManager::setActiveWResBuffer(int bufferID) {
    if (bufferID >= 0 && bufferID < 8) {
        m_impl->activeWResBufferID = bufferID;
    }
}

int DisplayManager::getActiveWResBuffer() const {
    return m_impl->activeWResBufferID;
}

void DisplayManager::flipWResBuffers() {
    // Swap buffers 0 and 1
    std::swap(m_impl->wresBuffers[0], m_impl->wresBuffers[1]);
}

void DisplayManager::setWResMode(bool isWResMode) {
    m_impl->wresMode = isWResMode;
    if (isWResMode) {
        m_impl->loResMode = false;  // Can't be both WRES and LORES
        m_impl->uresMode = false;   // Can't be both WRES and URES
        m_impl->xresMode = false;   // Can't be both WRES and XRES
        m_impl->presMode = false;   // Can't be both WRES and PRES
    }
}

bool DisplayManager::isWResMode() const {
    return m_impl->wresMode;
}

// PRES Mode Management
std::shared_ptr<PResBuffer> DisplayManager::getPResBuffer() const {
    return m_impl->presBuffers[0];
}

std::shared_ptr<PResBuffer> DisplayManager::getPResBuffer(int bufferID) const {
    if (bufferID >= 0 && bufferID < 8) {
        return m_impl->presBuffers[bufferID];
    }
    return nullptr;
}

void DisplayManager::setActivePResBuffer(int bufferID) {
    if (bufferID >= 0 && bufferID < 8) {
        m_impl->activePResBufferID = bufferID;
    }
}

int DisplayManager::getActivePResBuffer() const {
    return m_impl->activePResBufferID;
}

void DisplayManager::flipPResBuffers() {
    // Swap buffers 0 and 1
    std::swap(m_impl->presBuffers[0], m_impl->presBuffers[1]);
}

void DisplayManager::setPResMode(bool isPResMode) {
    m_impl->presMode = isPResMode;
    if (isPResMode) {
        m_impl->loResMode = false;  // Can't be both PRES and LORES
        m_impl->uresMode = false;   // Can't be both PRES and URES
        m_impl->xresMode = false;   // Can't be both PRES and XRES
        m_impl->wresMode = false;   // Can't be both PRES and WRES
    }
}

bool DisplayManager::isPResMode() const {
    return m_impl->presMode;
}

void DisplayManager::setActiveBuffer(int bufferID) {
    if (bufferID == 0 || bufferID == 1) {
        m_impl->activeBufferID = bufferID;
    }
}

int DisplayManager::getActiveBuffer() const {
    return m_impl->activeBufferID;
}

void DisplayManager::flipBuffers() {
    // Swap which buffer is displayed
    m_impl->displayBufferID = (m_impl->displayBufferID == 0) ? 1 : 0;
}

void DisplayManager::setTilemapManager(std::shared_ptr<TilemapManager> tilemapManager) {
    m_impl->tilemapManager = tilemapManager;
}

std::shared_ptr<TilemapManager> DisplayManager::getTilemapManager() const {
    return m_impl->tilemapManager;
}

void DisplayManager::setTilemapRenderer(std::shared_ptr<TilemapRenderer> tilemapRenderer) {
    m_impl->tilemapRenderer = tilemapRenderer;
}

std::shared_ptr<TilemapRenderer> DisplayManager::getTilemapRenderer() const {
    return m_impl->tilemapRenderer;
}

void DisplayManager::updateTiming() {
    auto currentTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> elapsed = currentTime - m_impl->lastFrameTime;
    m_impl->deltaTime = elapsed.count();
    m_impl->lastFrameTime = currentTime;

    m_impl->frameTime = m_impl->deltaTime * 1000.0f; // Convert to milliseconds

    // Update FPS counter
    m_impl->frameCount++;
    m_impl->fpsUpdateTimer += m_impl->deltaTime;

    if (m_impl->fpsUpdateTimer >= 1.0f) {
        m_impl->currentFPS = m_impl->frameCount / m_impl->fpsUpdateTimer;
        m_impl->frameCount = 0;
        m_impl->fpsUpdateTimer = 0.0f;
    }
}

void DisplayManager::handleResize(uint32_t width, uint32_t height) {
    if (m_impl->renderer) {
        m_impl->renderer->setViewportSize(width, height);
    }

    if (m_impl->resizeCallback) {
        m_impl->resizeCallback(width, height);
    }
}

bool DisplayManager::renderFrame() {
    // CRITICAL: Don't render until readyForCallbacks is set (after run() starts)
    // This prevents CVDisplayLink thread from racing with initialization
    if (!m_impl->initialized || !m_impl->renderer || !m_impl->textGrid || !m_impl->readyForCallbacks) {
        static int skipCount = 0;
        if (skipCount < 5) {
            NSLog(@"[DisplayManager] renderFrame skipped: init=%d renderer=%p textGrid=%p ready=%d",
                  m_impl->initialized, m_impl->renderer.get(), m_impl->textGrid.get(), m_impl->readyForCallbacks);
            skipCount++;
        }
        return false;
    }

    static bool firstFrame = true;
    if (firstFrame) {
        NSLog(@"[DisplayManager] First renderFrame executing - rendering enabled");
        firstFrame = false;
    }

    @autoreleasepool {
        // Update timing
        updateTiming();

        // Update particle system with fixed 60Hz timestep (accumulator pattern)
        if (st_particle_system_is_ready()) {
            static float particleTimeAccumulator = 0.0f;
            const float FIXED_TIMESTEP = 1.0f / 60.0f; // 16.67ms

            // Accumulate time
            particleTimeAccumulator += m_impl->deltaTime;

            // Update particles in fixed timesteps
            while (particleTimeAccumulator >= FIXED_TIMESTEP) {
                st_particle_system_update(FIXED_TIMESTEP);
                particleTimeAccumulator -= FIXED_TIMESTEP;
            }
        }

        // Call frame callback
        if (m_impl->frameCallback) {
            m_impl->frameCallback(m_impl->deltaTime);
        }

        // Update tilemap manager if available
        if (m_impl->tilemapManager && m_impl->tilemapManager->isInitialized()) {
            m_impl->tilemapManager->update(m_impl->deltaTime);
        }

        // Render text, graphics, sprites, rectangles, circles, diamonds, stars, and tilemaps (integrated render pass)
        // Wrap in @try/@catch to handle Metal exceptions that can occur with GPU operations
        bool success = false;
        @try {
            success = m_impl->renderer->renderFrame(*m_impl->textGrid,
                                                     m_impl->graphicsLayer.get(),
                                                     m_impl->spriteManager.get(),
                                                     m_impl->rectangleManager.get(),
                                                     m_impl->circleManager.get(),
                                                     m_impl->lineManager.get(),
                                                     m_impl->polygonManager.get(),
                                                     m_impl->starManager.get(),
                                                     m_impl->tilemapManager.get(),
                                                     m_impl->tilemapRenderer.get(),
                                                     m_impl->deltaTime);

            if (!success) {
                NSLog(@"renderFrame: renderer->renderFrame() returned false");
            }
        }
        @catch (NSException *exception) {
            NSLog(@"[DisplayManager] ERROR: Metal rendering exception caught: %@", exception.reason);
            NSLog(@"[DisplayManager] Exception name: %@", exception.name);
            NSLog(@"[DisplayManager] Call stack: %@", exception.callStackSymbols);
            success = false;

            // Continue running - don't crash the app
            // The frame will be skipped but the app stays alive
        }
        @catch (...) {
            NSLog(@"[DisplayManager] ERROR: Unknown C++ exception caught during rendering");
            success = false;
        }

        return success;
    }
}

void DisplayManager::run() {
    if (!m_impl->initialized) {
        return;
    }

    m_impl->running = true;
    m_impl->readyForCallbacks = true;  // NOW it's safe to process mouse events
    m_impl->lastFrameTime = std::chrono::high_resolution_clock::now();

    NSLog(@"DisplayManager::run() - Starting");
    NSLog(@"Window: %@", m_impl->window);
    NSLog(@"Window visible: %d", [m_impl->window isVisible]);

    // Make sure window is visible and app is activated
    [m_impl->window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];

    NSLog(@"After makeKeyAndOrderFront - Window visible: %d", [m_impl->window isVisible]);

    // Start CVDisplayLink if available (handles rendering on separate thread)
    if (m_impl->useCVDisplayLink && m_impl->displayLink) {
        CVReturn cvReturn = CVDisplayLinkStart(m_impl->displayLink);
        if (cvReturn == kCVReturnSuccess) {
            m_impl->displayLinkRunning = true;
            NSLog(@"[DisplayManager] CVDisplayLink started - rendering at display refresh rate");
        } else {
            NSLog(@"[DisplayManager] WARNING: Failed to start CVDisplayLink, falling back to manual loop");
            m_impl->useCVDisplayLink = false;
        }
    }

    // Keep track of frame count for debug
    int frameCount = 0;

    NSLog(@"Entering main loop (CVDisplayLink: %s)", m_impl->useCVDisplayLink ? "enabled" : "disabled");

    // If using CVDisplayLink, just run event loop
    // Otherwise, run traditional render loop
    if (m_impl->useCVDisplayLink) {
        // Event-only loop - CVDisplayLink handles rendering
        while (m_impl->running) {
            @autoreleasepool {
                // Begin input frame
                if (m_impl->inputManager) {
                    m_impl->inputManager->beginFrame();
                }

                // Process events with timeout to allow checking m_impl->running
                NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                   untilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]
                                                      inMode:NSDefaultRunLoopMode
                                                     dequeue:YES];
                if (event) {
                    [NSApp sendEvent:event];
                    [NSApp updateWindows];
                }

                // End input frame
                if (m_impl->inputManager) {
                    m_impl->inputManager->endFrame();
                }

                // Check if window was closed
                if (!m_impl->window || ![m_impl->window isVisible]) {
                    NSLog(@"Window closed or not visible, exiting");
                    m_impl->running = false;
                    break;
                }
            }
        }

        // Stop CVDisplayLink
        if (m_impl->displayLinkRunning) {
            CVDisplayLinkStop(m_impl->displayLink);
            m_impl->displayLinkRunning = false;
            NSLog(@"[DisplayManager] CVDisplayLink stopped");
        }
    } else {
        // Traditional manual render loop
        float targetFrameTime = 1.0f / m_impl->config.targetFPS;

        while (m_impl->running) {
            @autoreleasepool {
                if (frameCount == 0) {
                    NSLog(@"Frame 0: renderer=%p, textGrid=%p", m_impl->renderer.get(), m_impl->textGrid.get());
                }

                // Begin input frame (before processing events)
                if (m_impl->inputManager) {
                    m_impl->inputManager->beginFrame();
                }

                // Process all pending events
                NSEvent* event;
                int eventCount = 0;
                do {
                    event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                               untilDate:nil
                                                  inMode:NSDefaultRunLoopMode
                                                 dequeue:YES];
                    if (event) {
                        [NSApp sendEvent:event];
                        [NSApp updateWindows];
                        eventCount++;
                    }
                } while (event != nil);

                // End input frame (after processing events, before rendering)
                if (m_impl->inputManager) {
                    m_impl->inputManager->endFrame();
                }

                if (frameCount < 3) {
                    NSLog(@"Frame %d: Processed %d events, window visible: %d",
                          frameCount, eventCount, [m_impl->window isVisible]);
                }

                // Check if window was closed (but allow a few frames for initialization)
                if (frameCount > 10 && (!m_impl->window || ![m_impl->window isVisible])) {
                    NSLog(@"Window closed or not visible, exiting");
                    m_impl->running = false;
                    break;
                }

                frameCount++;

                // Render frame
                bool renderSuccess = renderFrame();
                if (frameCount < 3) {
                    NSLog(@"Frame %d: Render success: %d", frameCount - 1, renderSuccess);
                }

                // Frame rate limiting (if not using vsync)
                if (!m_impl->config.vsync) {
                    auto frameEnd = std::chrono::high_resolution_clock::now();
                    std::chrono::duration<float> frameElapsed = frameEnd - m_impl->lastFrameTime;
                    float sleepTime = targetFrameTime - frameElapsed.count();

                    if (sleepTime > 0.0f) {
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(static_cast<int>(sleepTime * 1000.0f))
                        );
                    }
                }
            }
        }
    }
}

void DisplayManager::stop() {
    m_impl->running = false;

    // Stop CVDisplayLink if running
    if (m_impl->displayLinkRunning && m_impl->displayLink) {
        CVDisplayLinkStop(m_impl->displayLink);
        m_impl->displayLinkRunning = false;
        NSLog(@"[DisplayManager] CVDisplayLink stopped in stop()");
    }
}

bool DisplayManager::isRunning() const {
    return m_impl->running;
}

bool DisplayManager::isInitialized() const {
    return m_impl->initialized;
}

bool DisplayManager::isReadyForCallbacks() const {
    return m_impl->readyForCallbacks;
}

void DisplayManager::setReady() {
    m_impl->readyForCallbacks = true;
}

void DisplayManager::setStartupStateMachine(std::shared_ptr<AppStartupStateMachine> stateMachine) {
    m_impl->startupStateMachine = stateMachine;
}

bool DisplayManager::canProcessMouseEvents() const {
    if (m_impl->startupStateMachine) {
        return m_impl->startupStateMachine->canProcessMouseEvents();
    }
    // Fallback if no state machine set
    return m_impl->readyForCallbacks;
}

NSWindowPtr DisplayManager::getWindow() const {
    return m_impl->window;
}

NSView* DisplayManager::getMetalView() const {
    return m_impl->metalView;
}

void* DisplayManager::getMetalLayer() const {
    if (m_impl->metalView) {
        STMetalView* metalView = (STMetalView*)m_impl->metalView;
        return (__bridge void*)metalView.metalLayer;
    }
    return nullptr;
}

void DisplayManager::getWindowSize(uint32_t& width, uint32_t& height) const {
    if (m_impl->window) {
        NSRect frame = [m_impl->window.contentView bounds];
        width = static_cast<uint32_t>(frame.size.width);
        height = static_cast<uint32_t>(frame.size.height);
    }
}

void DisplayManager::setWindowSize(uint32_t width, uint32_t height) {
    if (m_impl->window) {
        NSRect frame = m_impl->window.frame;
        NSRect contentRect = NSMakeRect(0, 0, width, height);
        NSRect newFrame = [m_impl->window frameRectForContentRect:contentRect];
        newFrame.origin = frame.origin;
        [m_impl->window setFrame:newFrame display:YES animate:YES];
    }
}

float DisplayManager::getCurrentFPS() const {
    return m_impl->currentFPS;
}

float DisplayManager::getFrameTime() const {
    return m_impl->frameTime;
}

void DisplayManager::toggleFullscreen() {
    if (m_impl->window) {
        [m_impl->window toggleFullScreen:nil];
    }
}

void DisplayManager::setWindowTitle(const char* title) {
    if (m_impl->window) {
        [m_impl->window setTitle:@(title)];
    }
}

// =============================================================================
// Status Bar Updates (v1 pattern for Metal view coexistence)
// =============================================================================

void DisplayManager::updateStatus(const char* status) {
    if (!m_impl->statusLabel || !status) return;

    @autoreleasepool {
        // Validate the string before converting to NSString
        size_t len = strnlen(status, 1024);  // Limit to reasonable length
        if (len == 0 || len >= 1024) {
            NSLog(@"[DisplayManager] WARNING: Invalid status string length: %zu", len);
            return;
        }

        // Try to create NSString - this validates UTF-8 encoding
        NSString* statusString = [NSString stringWithUTF8String:status];
        if (!statusString) {
            NSLog(@"[DisplayManager] WARNING: Failed to create NSString - invalid UTF-8 encoding");
            return;
        }

        dispatch_async(dispatch_get_main_queue(), ^{
            [m_impl->statusLabel setStringValue:statusString];
        });
    }
}

void DisplayManager::updateCursorPosition(int line, int col) {
    if (!m_impl->cursorLabel) return;

    @autoreleasepool {
        dispatch_async(dispatch_get_main_queue(), ^{
            NSString* cursorString = [NSString stringWithFormat:@"Ln %d, Col %d", line, col];
            [m_impl->cursorLabel setStringValue:cursorString];
        });
    }
}

void DisplayManager::updateScriptName(const char* scriptName) {
    if (!m_impl->scriptLabel) return;

    @autoreleasepool {
        NSString* scriptString = nil;
        if (scriptName && strlen(scriptName) > 0) {
            scriptString = [NSString stringWithUTF8String:scriptName];
        }
        if (!scriptString) {
            scriptString = @"";
        }

        // Only update if the value has changed (prevents flashing)
        if (![scriptString isEqualToString:m_impl->lastScriptName]) {
            m_impl->lastScriptName = scriptString;
            dispatch_async(dispatch_get_main_queue(), ^{
                [m_impl->scriptLabel setStringValue:scriptString];
            });
        }
    }
}

void DisplayManager::setStatusBarVisible(bool visible) {
    if (!m_impl->statusBar) return;

    @autoreleasepool {
        dispatch_async(dispatch_get_main_queue(), ^{
            [m_impl->statusBar setHidden:!visible];

            // Adjust Metal view frame when hiding/showing status bar
            if (m_impl->metalView && m_impl->containerView) {
                NSRect containerBounds = [m_impl->containerView bounds];
                const CGFloat statusBarHeight = 22.0;

                NSRect metalFrame;
                if (visible) {
                    // Status bar visible - Metal view starts above it
                    metalFrame = NSMakeRect(0, statusBarHeight,
                                          containerBounds.size.width,
                                          containerBounds.size.height - statusBarHeight);
                } else {
                    // Status bar hidden - Metal view fills entire container
                    metalFrame = containerBounds;
                }

                [m_impl->metalView setFrame:metalFrame];
            }
        });
    }
}

dispatch_queue_t DisplayManager::getRenderQueue() const {
    return m_impl->renderQueue;
}

} // namespace SuperTerminal
