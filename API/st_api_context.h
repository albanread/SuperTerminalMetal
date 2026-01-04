//
// st_api_context.h
// SuperTerminal v2 - C API Context Manager
//
// Manages the global state and framework component references for the C API
//
// THREAD SAFETY:
// - All public methods are thread-safe via recursive mutex
// - Context is a singleton - safe to access from multiple threads
// - Component setters should only be called during initialization (main thread)
// - Component getters are thread-safe and can be called from any thread
// - Resource handle methods are thread-safe (registerSprite, registerSound, etc.)
// - Error handling methods are thread-safe but last error is global state
//   (concurrent errors from different threads will overwrite each other)
//

#ifndef ST_API_CONTEXT_H
#define ST_API_CONTEXT_H

#include <memory>
#include <string>
#include <mutex>
#include <unordered_map>
#include <functional>
#include <atomic>
#include <condition_variable>

// Forward declarations of framework components
namespace SuperTerminal {
    class TextGrid;
    class GraphicsLayer;
    class SpriteManager;
    class InputManager;
    class AudioManager;
    class DisplayManager;
    class AssetManager;
    class TilemapManager;
    class TextDisplayManager;
    class CartManager;
    class LoResPaletteManager;
    class XResPaletteManager;
    class WResPaletteManager;
    class PResPaletteManager;
}

// Full include needed for unique_ptr<SimpleLineEditor> in LineInputState
#ifndef VOICE_ONLY
#include "../Input/SimpleLineEditor.h"
#endif

// Forward declaration for CoreGraphics
#ifdef __OBJC__
    #import <CoreGraphics/CoreGraphics.h>
#else
    typedef struct CGContext* CGContextRef;
#endif

namespace STApi {

// =============================================================================
// API Context - Singleton that holds framework component references
// =============================================================================
//
// Thread Safety Model:
// - Singleton access (instance()) is thread-safe
// - All methods acquire recursive mutex before accessing state
// - Component registration (setXxx) should be done once at startup
// - Component access (xxx()) is safe from any thread after initialization
// - Frame/time updates are called from render thread only
// - Resource registration can be called from any thread
//
class Context {
public:
    static Context& instance();
    
    // Disable copy/move
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&&) = delete;
    Context& operator=(Context&&) = delete;
    
    // Component accessors (thread-safe, can be called from any thread)
    SuperTerminal::TextGrid* textGrid() const { return m_textGrid.get(); }
    SuperTerminal::GraphicsLayer* graphics() const { return m_graphics.get(); }
    SuperTerminal::SpriteManager* sprites() const { return m_sprites.get(); }
    SuperTerminal::InputManager* input() const { return m_input.get(); }
    SuperTerminal::AudioManager* audio() const { return m_audio.get(); }
    SuperTerminal::DisplayManager* display() const { return m_display.get(); }
    SuperTerminal::AssetManager* assets() const { return m_assets.get(); }
    SuperTerminal::TilemapManager* tilemap() const { return m_tilemap.get(); }
    SuperTerminal::TextDisplayManager* textDisplay() const { return m_textDisplay.get(); }
    SuperTerminal::CartManager* getCartManager() const { return m_cartManager.get(); }
    
    // Helper accessors for palette managers (via DisplayManager)
    SuperTerminal::LoResPaletteManager* loresPalette() const;
    SuperTerminal::XResPaletteManager* xresPalette() const;
    SuperTerminal::WResPaletteManager* wresPalette() const;
    SuperTerminal::PResPaletteManager* presPalette() const;
    
    // Component setters (thread-safe, but should only be called once during initialization)
    void setTextGrid(std::shared_ptr<SuperTerminal::TextGrid> grid) { m_textGrid = grid; }
    void setGraphics(std::shared_ptr<SuperTerminal::GraphicsLayer> gfx) { m_graphics = gfx; }
    void setSprites(std::shared_ptr<SuperTerminal::SpriteManager> spr) { m_sprites = spr; }
    void setInput(std::shared_ptr<SuperTerminal::InputManager> inp) { m_input = inp; }
    void setAudio(std::shared_ptr<SuperTerminal::AudioManager> aud) { m_audio = aud; }
    void setDisplay(std::shared_ptr<SuperTerminal::DisplayManager> disp) { m_display = disp; }
    void setAssets(std::shared_ptr<SuperTerminal::AssetManager> assetMgr) { m_assets = assetMgr; }
    void setTilemap(std::shared_ptr<SuperTerminal::TilemapManager> tilemapMgr) { m_tilemap = tilemapMgr; }
    void setTextDisplay(std::shared_ptr<SuperTerminal::TextDisplayManager> textDisplayMgr) { m_textDisplay = textDisplayMgr; }
    void setCartManager(std::shared_ptr<SuperTerminal::CartManager> cartMgr) { m_cartManager = cartMgr; }
    
    // Error handling (thread-safe, but error state is shared across threads)
    // WARNING: Concurrent errors from different threads will overwrite each other
    void setLastError(const std::string& error);
    const char* getLastError() const;
    void clearError();
    
    // Frame tracking (thread-safe, typically called from render thread)
    void incrementFrame();
    uint64_t frameCount() const { return m_frameCount; }
    void updateTime(double time, double delta);
    double time() const { return m_time; }
    double deltaTime() const { return m_deltaTime; }
    
    // Text cursor position (thread-safe)
    void setTextCursor(int x, int y);
    void getTextCursor(int* x, int* y) const;
    int getTextCursorX() const { return m_textCursorX; }
    int getTextCursorY() const { return m_textCursorY; }
    
    // Frame wait system (script thread requests wake-up, render thread grants it)
    void requestFrameWait(int frameCount);  // Script thread: block until N frames elapse
    void processFrameWaits();               // Render thread: check and wake waiting threads
    void interruptFrameWaits();             // Interrupt all pending frame waits (for script termination)
    
    // Script cancellation system (for WAIT_MS and other cancellable operations)
    void setScriptShouldStop(bool shouldStop) { m_scriptShouldStop = shouldStop; }
    bool shouldStopScript() const { return m_scriptShouldStop; }
    
    // Line input mode system (script thread requests input, render thread drives editor)
    void requestLineInput(int x, int y, const std::string& prompt, int fgColor = -1, int bgColor = -1);
    void updateLineInput();                 // Render thread: drive SimpleLineEditor
    std::string getLineInputResult();       // Script thread: get result after completion
    bool isLineInputActive() const;         // Check if line input is in progress
    
    // Resource tracking (thread-safe, can be called from any thread)
    int32_t registerSprite(int spriteId);
    void unregisterSprite(int32_t handle);
    int getSpriteId(int32_t handle) const;
    
    int32_t registerSound(const std::string& name);
    void unregisterSound(int32_t handle);
    std::string getSoundName(int32_t handle) const;
    
    int32_t registerAsset(const std::string& name);
    void unregisterAsset(int32_t handle);
    std::string getAssetName(int32_t handle) const;
    
    // Sprite drawing context (for DrawIntoSprite/EndDrawIntoSprite)
    bool isDrawingIntoSprite() const { return m_spriteDrawContext != nullptr; }
    CGContextRef getSpriteDrawContext() const { return m_spriteDrawContext; }
    int getSpriteDrawWidth() const { return m_spriteDrawWidth; }
    int getSpriteDrawHeight() const { return m_spriteDrawHeight; }
    int getSpriteDrawId() const { return m_spriteDrawId; }
    void* getSpriteDrawBitmapData() const { return m_spriteDrawBitmapData; }
    
    void beginSpriteDrawing(int spriteId, int width, int height, CGContextRef context, void* bitmapData);
    void endSpriteDrawing();
    
    // File drawing context (for DrawToFile/EndDrawToFile)
    bool isDrawingToFile() const { return m_fileDrawContext != nullptr; }
    CGContextRef getFileDrawContext() const { return m_fileDrawContext; }
    int getFileDrawWidth() const { return m_fileDrawWidth; }
    int getFileDrawHeight() const { return m_fileDrawHeight; }
    void* getFileDrawBitmapData() const { return m_fileDrawBitmapData; }
    const char* getFileDrawFilename() const { return m_fileDrawFilename.c_str(); }
    
    void beginFileDrawing(const char* filename, int width, int height, CGContextRef context, void* bitmapData);
    void endFileDrawing();
    
    // Tileset drawing context (for DrawToTileset/DrawTile/EndDrawToTileset)
    bool isDrawingToTileset() const { return m_tilesetDrawContext != nullptr; }
    CGContextRef getTilesetDrawContext() const { return m_tilesetDrawContext; }
    int getTilesetDrawWidth() const { return m_tilesetDrawWidth; }
    int getTilesetDrawHeight() const { return m_tilesetDrawHeight; }
    int getTilesetDrawTileWidth() const { return m_tilesetDrawTileWidth; }
    int getTilesetDrawTileHeight() const { return m_tilesetDrawTileHeight; }
    int getTilesetDrawColumns() const { return m_tilesetDrawColumns; }
    int getTilesetDrawRows() const { return m_tilesetDrawRows; }
    int getTilesetDrawCurrentTile() const { return m_tilesetDrawCurrentTile; }
    int getTilesetDrawId() const { return m_tilesetDrawId; }
    void* getTilesetDrawBitmapData() const { return m_tilesetDrawBitmapData; }
    
    void beginTilesetDrawing(int tilesetId, int tileWidth, int tileHeight, int columns, int rows,
                             int atlasWidth, int atlasHeight, CGContextRef context, void* bitmapData);
    void setTilesetDrawCurrentTile(int tileIndex);
    void endTilesetDrawing();
    
    // Thread safety
    std::recursive_mutex& mutex() { return m_mutex; }
    
private:
    Context();
    ~Context();
    
    // Framework components (shared ownership)
    std::shared_ptr<SuperTerminal::TextGrid> m_textGrid;
    std::shared_ptr<SuperTerminal::GraphicsLayer> m_graphics;
    std::shared_ptr<SuperTerminal::SpriteManager> m_sprites;
    std::shared_ptr<SuperTerminal::InputManager> m_input;
    std::shared_ptr<SuperTerminal::AudioManager> m_audio;
    std::shared_ptr<SuperTerminal::DisplayManager> m_display;
    std::shared_ptr<SuperTerminal::AssetManager> m_assets;
    std::shared_ptr<SuperTerminal::TilemapManager> m_tilemap;
    std::shared_ptr<SuperTerminal::TextDisplayManager> m_textDisplay;
    std::shared_ptr<SuperTerminal::CartManager> m_cartManager;
    
    // Error state
    std::string m_lastError;
    
    // Frame/time tracking
    std::atomic<uint64_t> m_frameCount;
    double m_time;
    double m_deltaTime;
    
    // Text cursor position (for PRINT command)
    std::atomic<int> m_textCursorX;
    std::atomic<int> m_textCursorY;
    
    // Frame wait queue system
    struct FrameWaitRequest {
        uint64_t targetFrame;
        std::condition_variable cv;
        std::mutex mutex;
        bool ready;
        bool interrupted;
        
        FrameWaitRequest(uint64_t target) 
            : targetFrame(target), ready(false), interrupted(false) {}
    };
    std::mutex m_frameWaitMutex;
    std::vector<std::shared_ptr<FrameWaitRequest>> m_frameWaitQueue;
    
    // Line input mode system (render-thread driven editor)
    struct LineInputState {
        bool active;
        int x, y;
        int fgColor, bgColor;
        std::string prompt;
        std::string result;
        bool ready;
#ifndef VOICE_ONLY
        std::unique_ptr<SuperTerminal::SimpleLineEditor> editor;
#endif
        mutable std::mutex mutex;
        std::condition_variable cv;
        
        LineInputState() 
            : active(false), x(0), y(0), fgColor(-1), bgColor(-1), ready(false) {}
    };
    LineInputState m_lineInputState;
    
    // Script cancellation flag
    std::atomic<bool> m_scriptShouldStop;

    // Resource handle mappings
    std::unordered_map<int32_t, int> m_spriteHandles;  // API handle -> sprite ID
    std::unordered_map<int32_t, std::string> m_soundHandles;  // API handle -> sound name
    std::unordered_map<int32_t, std::string> m_assetHandles;  // API handle -> asset name
    int32_t m_nextSpriteHandle;
    int32_t m_nextSoundHandle;
    int32_t m_nextAssetHandle;
    
    // Sprite drawing state (for DrawIntoSprite/EndDrawIntoSprite)
    CGContextRef m_spriteDrawContext;
    void* m_spriteDrawBitmapData;
    int m_spriteDrawWidth;
    int m_spriteDrawHeight;
    int m_spriteDrawId;
    
    // File drawing state (for DrawToFile/EndDrawToFile)
    CGContextRef m_fileDrawContext;
    void* m_fileDrawBitmapData;
    int m_fileDrawWidth;
    int m_fileDrawHeight;
    std::string m_fileDrawFilename;
    
    // Tileset drawing state (for DrawToTileset/DrawTile/EndDrawToTileset)
    CGContextRef m_tilesetDrawContext;
    void* m_tilesetDrawBitmapData;
    int m_tilesetDrawWidth;           // Total atlas width
    int m_tilesetDrawHeight;          // Total atlas height
    int m_tilesetDrawTileWidth;       // Individual tile width
    int m_tilesetDrawTileHeight;      // Individual tile height
    int m_tilesetDrawColumns;         // Number of columns
    int m_tilesetDrawRows;            // Number of rows
    int m_tilesetDrawCurrentTile;     // Currently selected tile index
    int m_tilesetDrawId;              // Tileset ID
    
    // Thread safety
    mutable std::recursive_mutex m_mutex;
};

// =============================================================================
// Helper Macros
// =============================================================================
//
// Thread Safety: These macros acquire the context mutex automatically
//

// Context access (thread-safe singleton)
#define ST_CONTEXT STApi::Context::instance()

// Lock acquisition (recursive mutex - safe to lock multiple times in same thread)
#define ST_LOCK std::lock_guard<std::recursive_mutex> lock(ST_CONTEXT.mutex())

// Error handling helper (thread-safe, but global error state)
#define ST_SET_ERROR(msg) ST_CONTEXT.setLastError(msg)
#define ST_CLEAR_ERROR() ST_CONTEXT.clearError()

// Null check helpers (thread-safe with automatic locking)
#define ST_CHECK_PTR(ptr, name) \
    if (!(ptr)) { \
        ST_SET_ERROR(name " not initialized"); \
        return; \
    }

#define ST_CHECK_PTR_RET(ptr, name, ret) \
    if (!(ptr)) { \
        ST_SET_ERROR(name " not initialized"); \
        return ret; \
    }

} // namespace STApi

#endif // ST_API_CONTEXT_H