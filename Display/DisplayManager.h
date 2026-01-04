//
// DisplayManager.h
// SuperTerminal v2
//
// Manages window creation, event loop, and render timing for macOS.
// Coordinates between the MetalRenderer, TextGrid, and system events.
//
// THREAD SAFETY:
// - All public methods are thread-safe unless marked otherwise
// - initialize() should be called once from the main thread
// - renderFrame() must be called from the render thread (via CVDisplayLink)
// - Component setters (setTextGrid, setRenderer, etc.) should be called
//   during initialization before starting the render loop
// - Window management methods (getWindow, etc.) are thread-safe
//

#ifndef SUPERTERMINAL_DISPLAY_MANAGER_H
#define SUPERTERMINAL_DISPLAY_MANAGER_H

#include <cstdint>
#include <memory>
#include <functional>
#include <dispatch/dispatch.h>

// Forward declarations
namespace SuperTerminal {
    class TextGrid;
    class MetalRenderer;
    class GraphicsLayer;
    class InputManager;
    class SpriteManager;
    class RectangleManager;
    class CircleManager;
    class LineManager;
    class PolygonManager;
    class StarManager;
    class LoResPaletteManager;
    class XResPaletteManager;
    class WResPaletteManager;
    class PResPaletteManager;
    class LoResBuffer;
    class UResBuffer;
    class XResBuffer;
    class WResBuffer;
    class PResBuffer;
    class TilemapManager;
    class TilemapRenderer;
    class AppStartupStateMachine;
    class VideoModeManager;
}

#ifdef __OBJC__
    @class NSWindow;
    @class NSView;
    @class CAMetalLayer;
    typedef NSWindow* NSWindowPtr;
    typedef NSView* NSViewPtr;
#else
    typedef void* NSWindowPtr;
    typedef void* NSViewPtr;
    typedef void* CAMetalLayer;
#endif

namespace SuperTerminal {

/// Display configuration options
struct DisplayConfig {
    uint32_t windowWidth;        // Initial window width
    uint32_t windowHeight;       // Initial window height
    uint32_t cellWidth;          // Cell width in pixels
    uint32_t cellHeight;         // Cell height in pixels
    bool fullscreen;             // Start in fullscreen mode
    bool vsync;                  // Enable vertical sync
    float targetFPS;             // Target frames per second (if not vsync)
    const char* windowTitle;     // Window title
    
    DisplayConfig()
        : windowWidth(800)
        , windowHeight(600)
        , cellWidth(8)
        , cellHeight(16)
        , fullscreen(false)
        , vsync(true)
        , targetFPS(60.0f)
        , windowTitle("SuperTerminal v2")
    {}
};

/// Frame callback types
using FrameCallback = std::function<void(float deltaTime)>;
using ResizeCallback = std::function<void(uint32_t width, uint32_t height)>;
using KeyCallback = std::function<void(uint32_t keyCode, bool pressed)>;
using MouseCallback = std::function<void(int gridX, int gridY, int button, bool pressed)>;

/// DisplayManager: Window and rendering loop management
///
/// Responsibilities:
/// - Create and manage NSWindow with Metal layer
/// - Run render loop at target framerate
/// - Handle window resize and fullscreen events
/// - Coordinate rendering between TextGrid and MetalRenderer
/// - Provide frame timing and vsync
///
/// Thread Safety:
/// - Initialization methods (initialize) must be called from main thread
/// - Component registration (setXxx) should be done during initialization
/// - Rendering (renderFrame) is called from CVDisplayLink callback (render thread)
/// - Window accessors (getWindow) are thread-safe
/// - Internal state is protected by mutexes where necessary
class DisplayManager {
public:
    /// Create a new DisplayManager
    DisplayManager();
    
    /// Destructor
    ~DisplayManager();
    
    // Disable copy, allow move
    DisplayManager(const DisplayManager&) = delete;
    DisplayManager& operator=(const DisplayManager&) = delete;
    DisplayManager(DisplayManager&&) noexcept;
    DisplayManager& operator=(DisplayManager&&) noexcept;
    
    /// Initialize the display system
    /// @param config Display configuration
    /// @return true if initialization succeeded
    /// @note Thread Safety: Must be called from main thread
    bool initialize(const DisplayConfig& config);
    
    /// Set the TextGrid to render
    /// @param textGrid Shared pointer to TextGrid
    /// @note Thread Safety: Should be called during initialization before render loop starts
    void setTextGrid(std::shared_ptr<TextGrid> textGrid);
    
    /// Set the MetalRenderer to use
    /// @param renderer Shared pointer to MetalRenderer
    /// @note Thread Safety: Should be called during initialization before render loop starts
    void setRenderer(std::shared_ptr<MetalRenderer> renderer);
    
    /// Set the GraphicsLayer to render
    /// @param graphicsLayer Shared pointer to GraphicsLayer
    /// @note Thread Safety: Should be called during initialization before render loop starts
    void setGraphicsLayer(std::shared_ptr<GraphicsLayer> graphicsLayer);
    
    /// Get the TextGrid
    /// @note Thread Safety: Safe to call from any thread after initialization
    std::shared_ptr<TextGrid> getTextGrid() const;
    
    /// Get the GraphicsLayer
    /// @note Thread Safety: Safe to call from any thread after initialization
    std::shared_ptr<GraphicsLayer> getGraphicsLayer() const;
    
    /// Get the MetalRenderer
    /// @note Thread Safety: Safe to call from any thread after initialization
    std::shared_ptr<MetalRenderer> getRenderer() const;
    
    /// Set the InputManager to use
    /// @param inputManager Shared pointer to InputManager
    /// @note Thread Safety: Should be called during initialization before render loop starts
    void setInputManager(std::shared_ptr<InputManager> inputManager);
    
    /// Update InputManager cell size based on current Metal view bounds
    /// @note Should be called after window resize or when view bounds change
    void updateInputManagerCellSize();
    
    /// Get the InputManager
    /// @note Thread Safety: Safe to call from any thread after initialization
    std::shared_ptr<InputManager> getInputManager() const;
    
    /// Set the SpriteManager to use
    /// @param spriteManager Shared pointer to SpriteManager
    /// @note Thread Safety: Should be called during initialization before render loop starts
    void setSpriteManager(std::shared_ptr<SpriteManager> spriteManager);
    
    /// Get the SpriteManager
    /// @note Thread Safety: Safe to call from any thread after initialization
    std::shared_ptr<SpriteManager> getSpriteManager() const;
    
    /// Set the RectangleManager to use
    /// @param rectangleManager Shared pointer to RectangleManager
    /// @note Thread Safety: Should be called during initialization before render loop starts
    void setRectangleManager(std::shared_ptr<RectangleManager> rectangleManager);
    
    /// Get the RectangleManager
    /// @note Thread Safety: Safe to call from any thread after initialization
    std::shared_ptr<RectangleManager> getRectangleManager() const;
    
    /// Set the CircleManager to use
    /// @param circleManager Shared pointer to CircleManager
    /// @note Thread Safety: Should be called during initialization before render loop starts
    void setCircleManager(std::shared_ptr<CircleManager> circleManager);
    
    /// Get the CircleManager
    /// @note Thread Safety: Safe to call from any thread after initialization
    std::shared_ptr<CircleManager> getCircleManager() const;
    
    /// Set the LineManager to use
    /// @param lineManager Shared pointer to LineManager
    /// @note Thread Safety: Should be called during initialization before render loop starts
    void setLineManager(std::shared_ptr<LineManager> lineManager);
    
    /// Get the LineManager
    /// @note Thread Safety: Safe to call from any thread after initialization
    std::shared_ptr<LineManager> getLineManager() const;
    
    /// Set the StarManager to use
    /// @param starManager Shared pointer to StarManager
    /// @note Thread Safety: Should be called during initialization before render loop starts
    void setStarManager(std::shared_ptr<StarManager> starManager);
    
    /// Get the StarManager
    /// @note Thread Safety: Safe to call from any thread after initialization
    std::shared_ptr<StarManager> getStarManager() const;
    
    /// Set the PolygonManager to use
    /// @param polygonManager Shared pointer to PolygonManager
    /// @note Thread Safety: Should be called during initialization before render loop starts
    void setPolygonManager(std::shared_ptr<PolygonManager> polygonManager);
    
    /// Get the PolygonManager
    /// @note Thread Safety: Safe to call from any thread after initialization
    std::shared_ptr<PolygonManager> getPolygonManager() const;
    
    /// Get the LoResPaletteManager
    /// @note Thread Safety: Safe to call from any thread after initialization
    std::shared_ptr<LoResPaletteManager> getLoresPalette() const;
    
    /// Get the XResPaletteManager
    /// @note Thread Safety: Safe to call from any thread after initialization
    std::shared_ptr<XResPaletteManager> getXresPalette() const;
    
    /// Get the WResPaletteManager
    /// @note Thread Safety: Safe to call from any thread after initialization
    std::shared_ptr<WResPaletteManager> getWresPalette() const;
    
    /// Get the PResPaletteManager
    /// @note Thread Safety: Safe to call from any thread after initialization
    std::shared_ptr<PResPaletteManager> getPresPalette() const;
    
    /// Get the VideoModeManager
    /// @note Thread Safety: Safe to call from any thread after initialization
    VideoModeManager* getVideoModeManager() const;
    
    /// Get the LoResBuffer (front buffer - buffer 0)
    /// @note Thread Safety: Safe to call from any thread after initialization
    std::shared_ptr<LoResBuffer> getLoResBuffer() const;
    
    /// Get a specific buffer by ID (0 = front, 1 = back, 2-7 = atlas/scratch)
    /// @param bufferID Buffer ID (0-7)
    /// @return Shared pointer to buffer, or nullptr if invalid ID
    /// @note Thread Safety: Safe to call from any thread after initialization
    std::shared_ptr<LoResBuffer> getLoResBuffer(int bufferID) const;
    
    /// Select active LORES buffer for drawing operations
    /// @param bufferID Buffer ID (0-7)
    /// @note Thread Safety: Safe to call from any thread
    void setActiveLoResBuffer(int bufferID);
    
    /// Get the active LORES buffer ID
    /// @return Current active buffer ID (0-7)
    /// @note Thread Safety: Safe to call from any thread
    int getActiveLoResBuffer() const;
    
    /// Flip LORES front and back buffers (swap 0 and 1)
    /// @note Thread Safety: Safe to call from any thread
    void flipLoResBuffers();
    
    /// Get the UResBuffer (front buffer - buffer 0)
    /// @note Thread Safety: Safe to call from any thread after initialization
    std::shared_ptr<UResBuffer> getUResBuffer() const;
    
    /// Get a specific URES buffer by ID (0 = front, 1 = back, 2-7 = atlas/scratch)
    /// @param bufferID Buffer ID (0-7)
    /// @return Shared pointer to buffer, or nullptr if invalid ID
    /// @note Thread Safety: Safe to call from any thread after initialization
    std::shared_ptr<UResBuffer> getUResBuffer(int bufferID) const;
    
    /// Select active URES buffer for drawing operations
    /// @param bufferID Buffer ID (0-7)
    /// @note Thread Safety: Safe to call from any thread
    void setActiveUResBuffer(int bufferID);
    
    /// Get the active URES buffer ID
    /// @return Current active buffer ID (0-7)
    /// @note Thread Safety: Safe to call from any thread
    int getActiveUResBuffer() const;
    
    /// Flip URES front and back buffers (swap 0 and 1)
    /// @note Thread Safety: Safe to call from any thread
    void flipUResBuffers();
    
    /// Get the XResBuffer (front buffer - buffer 0)
    /// @note Thread Safety: Safe to call from any thread after initialization
    std::shared_ptr<XResBuffer> getXResBuffer() const;
    
    /// Get a specific XRES buffer by ID (0-7)
    /// @param bufferID Buffer ID (0 = front, 1 = back, 2-7 = atlas/scratch)
    /// @return Shared pointer to buffer, or nullptr if invalid ID
    /// @note Thread Safety: Safe to call from any thread after initialization
    std::shared_ptr<XResBuffer> getXResBuffer(int bufferID) const;
    
    /// Select active XRES buffer for drawing operations
    /// @param bufferID Buffer ID (0-7)
    /// @note Thread Safety: Safe to call from any thread
    void setActiveXResBuffer(int bufferID);
    
    /// Get the active XRES buffer ID
    /// @return Current active buffer ID (0-7)
    /// @note Thread Safety: Safe to call from any thread
    int getActiveXResBuffer() const;
    
    /// Flip XRES front and back buffers (swap 0 and 1)
    /// @note Thread Safety: Safe to call from any thread
    void flipXResBuffers();
    
    /// Select active buffer for drawing operations
    /// @param bufferID Buffer ID (0 or 1)
    /// @note Thread Safety: Safe to call from any thread
    void setActiveBuffer(int bufferID);
    
    /// Get current active buffer ID
    /// @return Active buffer ID (0 or 1)
    /// @note Thread Safety: Safe to call from any thread
    int getActiveBuffer() const;
    
    /// Swap front and back buffers (instant flip)
    /// @note Thread Safety: Safe to call from any thread
    void flipBuffers();
    
    /// Set display mode (TEXT or LORES)
    /// @param isLoResMode true for LORES mode, false for TEXT mode
    /// @note Thread Safety: Safe to call from any thread
    void setLoResMode(bool isLoResMode);
    
    /// Check if in LORES mode
    /// @return true if LORES mode is active, false if TEXT mode
    /// @note Thread Safety: Safe to call from any thread
    bool isLoResMode() const;
    
    /// Set display mode to URES (Ultra Resolution 1280×720 direct color)
    /// @param isUResMode true for URES mode, false otherwise
    /// @note Thread Safety: Safe to call from any thread
    /// @note Setting URES mode automatically disables LORES mode
    void setUResMode(bool isUResMode);
    
    /// Check if in URES mode
    /// @return true if URES mode is active, false otherwise
    /// @note Thread Safety: Safe to call from any thread
    bool isUResMode() const;
    
    /// Set display mode to XRES (Mode X 320×240 with 256-color palette)
    /// @param isXResMode true for XRES mode, false otherwise
    /// @note Thread Safety: Safe to call from any thread
    /// @note Setting XRES mode automatically disables LORES and URES modes
    void setXResMode(bool isXResMode);
    
    /// Check if in XRES mode
    /// @return true if XRES mode is active, false otherwise
    /// @note Thread Safety: Safe to call from any thread
    bool isXResMode() const;
    
    /// Set display mode to WRES (Wide Resolution 432×240 with 256-color palette)
    /// @param isWResMode true for WRES mode, false otherwise
    /// @note Thread Safety: Safe to call from any thread
    /// @note Setting WRES mode automatically disables LORES, URES, and XRES modes
    void setWResMode(bool isWResMode);
    
    /// Check if in WRES mode
    /// @return true if WRES mode is active, false otherwise
    /// @note Thread Safety: Safe to call from any thread
    bool isWResMode() const;
    
    /// Get the WResBuffer (front buffer - buffer 0)
    /// @note Thread Safety: Safe to call from any thread after initialization
    std::shared_ptr<WResBuffer> getWResBuffer() const;
    
    /// Get a specific WRES buffer by ID (0-7)
    /// @param bufferID Buffer ID (0-7, where 0=front, 1=back, 2-7=atlas)
    /// @return Shared pointer to buffer, or nullptr if invalid ID
    /// @note Thread Safety: Safe to call from any thread after initialization
    std::shared_ptr<WResBuffer> getWResBuffer(int bufferID) const;
    
    /// Select active WRES buffer for drawing operations
    /// @param bufferID Buffer ID (0-7)
    /// @note Thread Safety: Safe to call from any thread
    void setActiveWResBuffer(int bufferID);
    
    /// Get currently active WRES buffer ID
    /// @return Active buffer ID (0-7)
    /// @note Thread Safety: Safe to call from any thread
    int getActiveWResBuffer() const;
    
    /// Flip WRES front and back buffers (swap buffers 0 and 1)
    /// @note Thread Safety: Safe to call from any thread
    void flipWResBuffers();
    
    /// Set display mode to PRES (Premium Resolution 1280×720 with 256-color palette)
    /// @param isPResMode true for PRES mode, false otherwise
    /// @note Thread Safety: Safe to call from any thread
    /// @note Setting PRES mode automatically disables LORES, URES, XRES, and WRES modes
    void setPResMode(bool isPResMode);
    
    /// Check if in PRES mode
    /// @return true if PRES mode is active, false otherwise
    /// @note Thread Safety: Safe to call from any thread
    bool isPResMode() const;
    
    /// Get the PResBuffer (front buffer - buffer 0)
    /// @note Thread Safety: Safe to call from any thread after initialization
    std::shared_ptr<PResBuffer> getPResBuffer() const;
    
    /// Get a specific PRES buffer by ID (0-7)
    /// @param bufferID Buffer ID (0-7, where 0=front, 1=back, 2-7=atlas)
    /// @return Shared pointer to buffer, or nullptr if invalid ID
    /// @note Thread Safety: Safe to call from any thread after initialization
    std::shared_ptr<PResBuffer> getPResBuffer(int bufferID) const;
    
    /// Select active PRES buffer for drawing operations
    /// @param bufferID Buffer ID (0-7)
    /// @note Thread Safety: Safe to call from any thread
    void setActivePResBuffer(int bufferID);
    
    /// Get currently active PRES buffer ID
    /// @return Active buffer ID (0-7)
    /// @note Thread Safety: Safe to call from any thread
    int getActivePResBuffer() const;
    
    /// Flip PRES front and back buffers (swap buffers 0 and 1)
    /// @note Thread Safety: Safe to call from any thread
    void flipPResBuffers();
    
    /// Set the TilemapManager to use
    /// @param tilemapManager Shared pointer to TilemapManager
    /// @note Thread Safety: Should be called during initialization before render loop starts
    void setTilemapManager(std::shared_ptr<TilemapManager> tilemapManager);
    
    /// Get the TilemapManager
    /// @note Thread Safety: Safe to call from any thread after initialization
    std::shared_ptr<TilemapManager> getTilemapManager() const;
    
    /// Set the TilemapRenderer to use
    /// @param tilemapRenderer Shared pointer to TilemapRenderer
    /// @note Thread Safety: Should be called during initialization before render loop starts
    void setTilemapRenderer(std::shared_ptr<TilemapRenderer> tilemapRenderer);
    
    /// Get the TilemapRenderer
    /// @note Thread Safety: Safe to call from any thread after initialization
    std::shared_ptr<TilemapRenderer> getTilemapRenderer() const;
    
    /// Set frame callback (called each frame before render)
    /// @param callback Function to call each frame
    /// @note Thread Safety: Should be called during initialization
    void setFrameCallback(FrameCallback callback);
    
    /// Set resize callback (called when window resizes)
    /// @param callback Function to call on resize
    /// @note Thread Safety: Should be called during initialization
    void setResizeCallback(ResizeCallback callback);
    
    /// Set key event callback
    /// @param callback Function to call on key events
    /// @note Thread Safety: Should be called during initialization
    void setKeyCallback(KeyCallback callback);
    
    /// Set mouse event callback
    /// @param callback Function to call on mouse events (gridX, gridY, button, pressed)
    /// @note Thread Safety: Should be called during initialization
    void setMouseCallback(MouseCallback callback);
    
    /// Invoke mouse callback (for internal use by event handlers)
    /// @param gridX Grid X coordinate
    /// @param gridY Grid Y coordinate
    /// @param button Mouse button index
    /// @param pressed True if pressed, false if released
    /// @note Thread Safety: Safe to call from main thread
    void invokeMouseCallback(int gridX, int gridY, int button, bool pressed);
    
    /// Start the render loop (blocking)
    /// This will run until the window is closed
    /// @note Thread Safety: Must be called from main thread
    void run();
    
    /// Stop the render loop
    /// @note Thread Safety: Safe to call from any thread
    void stop();
    
    /// Render a single frame (non-blocking, for manual control)
    /// @return true if render succeeded
    /// @note Thread Safety: Must be called from render thread (CVDisplayLink callback)
    bool renderFrame();
    
    /// Check if display is running
    /// @note Thread Safety: Safe to call from any thread
    bool isRunning() const;
    
    /// Check if display is initialized
    /// @note Thread Safety: Safe to call from any thread
    bool isInitialized() const;
    
    /// Check if display is ready for callbacks (render loop has started)
    /// @note Thread Safety: Safe to call from any thread
    bool isReadyForCallbacks() const;
    
    /// Mark display as ready for rendering (for use with external render loops like CVDisplayLink)
    /// @note Thread Safety: Should be called after all initialization is complete
    void setReady();
    
    /// Set the startup state machine (for checking initialization state)
    /// @note Thread Safety: Should be called during initialization
    void setStartupStateMachine(std::shared_ptr<AppStartupStateMachine> stateMachine);
    
    /// Check if mouse events can be processed (queries state machine)
    /// @note Thread Safety: Safe to call from any thread
    bool canProcessMouseEvents() const;
    
    /// Get the native window handle
    /// @note Thread Safety: Safe to call from any thread
    NSWindowPtr getWindow() const;
    
    /// Get the Metal view (for coordinate conversion)
    /// @note Thread Safety: Safe to call from any thread
    NSViewPtr getMetalView() const;
    
    /// Get the CAMetalLayer directly
    /// @note Thread Safety: Safe to call from any thread
    void* getMetalLayer() const;
    
    /// Get current window size
    /// @note Thread Safety: Safe to call from any thread
    void getWindowSize(uint32_t& width, uint32_t& height) const;
    
    /// Set window size
    /// @note Thread Safety: Should be called from main thread
    void setWindowSize(uint32_t width, uint32_t height);
    
    /// Get current FPS
    /// @note Thread Safety: Safe to call from any thread
    float getCurrentFPS() const;
    
    /// Get frame time (milliseconds)
    /// @note Thread Safety: Safe to call from any thread
    float getFrameTime() const;
    
    /// Toggle fullscreen mode
    /// @note Thread Safety: Should be called from main thread
    void toggleFullscreen();
    
    /// Set window title
    /// @note Thread Safety: Safe to call from any thread
    void setWindowTitle(const char* title);
    
    // =========================================================================
    // Status Bar Updates (v1 pattern for Metal view coexistence)
    // =========================================================================
    
    /// Update status bar status text (left side)
    /// @param status Status text to display (e.g., "● Running", "● Stopped")
    /// @note Thread Safety: Safe to call from any thread
    void updateStatus(const char* status);
    
    /// Update status bar cursor position (center)
    /// @param line Current line number (1-based)
    /// @param col Current column number (1-based)
    /// @note Thread Safety: Safe to call from any thread
    void updateCursorPosition(int line, int col);
    
    /// Update status bar script name (right side)
    /// @param scriptName Name of current script
    /// @note Thread Safety: Safe to call from any thread
    void updateScriptName(const char* scriptName);
    
    /// Show or hide the status bar
    /// @param visible true to show, false to hide
    /// @note Thread Safety: Should be called from main thread
    void setStatusBarVisible(bool visible);
    
    /// Get render queue for CVDisplayLink callback
    /// @return Dispatch queue for rendering
    dispatch_queue_t getRenderQueue() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    
    /// Update frame timing
    void updateTiming();
    
    /// Handle window resize internally
    void handleResize(uint32_t width, uint32_t height);
};

} // namespace SuperTerminal

#endif // SUPERTERMINAL_DISPLAY_MANAGER_H