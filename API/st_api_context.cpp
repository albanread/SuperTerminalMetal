//
// st_api_context.cpp
// SuperTerminal v2 - C API Context Manager Implementation
//

#include "st_api_context.h"
#ifndef VOICE_ONLY
#include "../Display/TextGrid.h"
#include "../Display/GraphicsLayer.h"
#include "../Display/SpriteManager.h"
#include "../Display/DisplayManager.h"
#include "../Display/LoResPaletteManager.h"
#include "../Display/XResPaletteManager.h"
#include "../Display/WResPaletteManager.h"
#include "../Display/PResPaletteManager.h"
#include "../Input/InputManager.h"
#include "../Input/SimpleLineEditor.h"
#endif
#include "../Audio/AudioManager.h"

namespace STApi {

// =============================================================================
// Singleton Instance
// =============================================================================

Context& Context::instance() {
    static Context instance;
    return instance;
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

Context::Context()
    : m_frameCount(0)
    , m_time(0.0)
    , m_deltaTime(0.0)
    , m_textCursorX(0)
    , m_textCursorY(0)
    , m_nextSpriteHandle(1)
    , m_nextSoundHandle(1)
    , m_nextAssetHandle(1)
    , m_spriteDrawContext(nullptr)
    , m_spriteDrawBitmapData(nullptr)
    , m_spriteDrawWidth(0)
    , m_spriteDrawHeight(0)
    , m_spriteDrawId(0)
    , m_fileDrawContext(nullptr)
    , m_fileDrawBitmapData(nullptr)
    , m_fileDrawWidth(0)
    , m_fileDrawHeight(0)
    , m_tilesetDrawContext(nullptr)
    , m_tilesetDrawBitmapData(nullptr)
    , m_tilesetDrawWidth(0)
    , m_tilesetDrawHeight(0)
    , m_tilesetDrawTileWidth(0)
    , m_tilesetDrawTileHeight(0)
    , m_tilesetDrawColumns(0)
    , m_tilesetDrawRows(0)
    , m_tilesetDrawCurrentTile(-1)
    , m_scriptShouldStop(false)
    , m_tilesetDrawId(0)
{
}

Context::~Context() {
    // Components will be automatically destroyed via shared_ptr
}

// =============================================================================
// Component Accessors
// =============================================================================

#ifndef VOICE_ONLY
SuperTerminal::LoResPaletteManager* Context::loresPalette() const {
    return display() ? display()->getLoresPalette().get() : nullptr;
}

SuperTerminal::XResPaletteManager* Context::xresPalette() const {
    return display() ? display()->getXresPalette().get() : nullptr;
}

SuperTerminal::WResPaletteManager* Context::wresPalette() const {
    return display() ? display()->getWresPalette().get() : nullptr;
}

SuperTerminal::PResPaletteManager* Context::presPalette() const {
    return display() ? display()->getPresPalette().get() : nullptr;
}
#endif

// =============================================================================
// Error Handling
// =============================================================================

void Context::setLastError(const std::string& error) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_lastError = error;
}

const char* Context::getLastError() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_lastError.empty() ? nullptr : m_lastError.c_str();
}

void Context::clearError() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_lastError.clear();
}

// =============================================================================
// Frame Tracking
// =============================================================================

void Context::incrementFrame() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_frameCount++;
}

void Context::updateTime(double time, double delta) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_time = time;
    m_deltaTime = delta;
}

void Context::requestFrameWait(int frameCount) {
    if (frameCount <= 0) return;
    
    // Create wait request with target frame
    uint64_t currentFrame = m_frameCount.load();
    uint64_t targetFrame = currentFrame + frameCount;
    auto request = std::make_shared<FrameWaitRequest>(targetFrame);
    
    // Add to wait queue
    {
        std::lock_guard<std::mutex> lock(m_frameWaitMutex);
        m_frameWaitQueue.push_back(request);
    }
    
    // Block until render thread wakes us or we're interrupted
    std::unique_lock<std::mutex> lock(request->mutex);
    request->cv.wait(lock, [request]() { return request->ready || request->interrupted; });
}

void Context::processFrameWaits() {
    std::lock_guard<std::mutex> lock(m_frameWaitMutex);
    
    uint64_t currentFrame = m_frameCount.load();
    
    // Check all pending wait requests
    auto it = m_frameWaitQueue.begin();
    while (it != m_frameWaitQueue.end()) {
        auto& request = *it;
        
        // Has this request's target frame been reached?
        if (currentFrame >= request->targetFrame) {
            // Wake up the waiting thread
            {
                std::lock_guard<std::mutex> reqLock(request->mutex);
                request->ready = true;
            }
            request->cv.notify_one();
            
            // Remove from queue
            it = m_frameWaitQueue.erase(it);
        } else {
            ++it;
        }
    }
}

void Context::interruptFrameWaits() {
    std::lock_guard<std::mutex> lock(m_frameWaitMutex);
    
    // Wake up all waiting threads and mark them as interrupted
    for (auto& request : m_frameWaitQueue) {
        {
            std::lock_guard<std::mutex> reqLock(request->mutex);
            request->interrupted = true;
        }
        request->cv.notify_one();
    }
    
    // Clear the queue
    m_frameWaitQueue.clear();
}

// =============================================================================
// Resource Tracking - Sprites
// =============================================================================

int32_t Context::registerSprite(int spriteId) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    int32_t handle = m_nextSpriteHandle++;
    m_spriteHandles[handle] = spriteId;
    return handle;
}

void Context::unregisterSprite(int32_t handle) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_spriteHandles.erase(handle);
}

int Context::getSpriteId(int32_t handle) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_spriteHandles.find(handle);
    return (it != m_spriteHandles.end()) ? it->second : -1;
}

// =============================================================================
// Resource Tracking - Sounds
// =============================================================================

int32_t Context::registerSound(const std::string& name) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    int32_t handle = m_nextSoundHandle++;
    m_soundHandles[handle] = name;
    return handle;
}

void Context::unregisterSound(int32_t handle) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_soundHandles.erase(handle);
}

std::string Context::getSoundName(int32_t handle) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_soundHandles.find(handle);
    return (it != m_soundHandles.end()) ? it->second : "";
}

// =============================================================================
// Resource Tracking - Assets
// =============================================================================

int32_t Context::registerAsset(const std::string& name) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    int32_t handle = m_nextAssetHandle++;
    m_assetHandles[handle] = name;
    return handle;
}

void Context::unregisterAsset(int32_t handle) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_assetHandles.erase(handle);
}

std::string Context::getAssetName(int32_t handle) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto it = m_assetHandles.find(handle);
    return (it != m_assetHandles.end()) ? it->second : "";
}

// =============================================================================
// Sprite Drawing Context Management
// =============================================================================

void Context::beginSpriteDrawing(int spriteId, int width, int height, CGContextRef context, void* bitmapData) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_spriteDrawId = spriteId;
    m_spriteDrawWidth = width;
    m_spriteDrawHeight = height;
    m_spriteDrawContext = context;
    m_spriteDrawBitmapData = bitmapData;
}

void Context::endSpriteDrawing() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_spriteDrawContext = nullptr;
    m_spriteDrawBitmapData = nullptr;
    m_spriteDrawWidth = 0;
    m_spriteDrawHeight = 0;
    m_spriteDrawId = 0;
}

void Context::beginFileDrawing(const char* filename, int width, int height, CGContextRef context, void* bitmapData) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_fileDrawFilename = filename;
    m_fileDrawWidth = width;
    m_fileDrawHeight = height;
    m_fileDrawContext = context;
    m_fileDrawBitmapData = bitmapData;
}

void Context::endFileDrawing() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_fileDrawContext = nullptr;
    m_fileDrawBitmapData = nullptr;
    m_fileDrawWidth = 0;
    m_fileDrawHeight = 0;
    m_fileDrawFilename.clear();
}

void Context::beginTilesetDrawing(int tilesetId, int tileWidth, int tileHeight, int columns, int rows,
                                   int atlasWidth, int atlasHeight, CGContextRef context, void* bitmapData) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_tilesetDrawId = tilesetId;
    m_tilesetDrawTileWidth = tileWidth;
    m_tilesetDrawTileHeight = tileHeight;
    m_tilesetDrawColumns = columns;
    m_tilesetDrawRows = rows;
    m_tilesetDrawWidth = atlasWidth;
    m_tilesetDrawHeight = atlasHeight;
    m_tilesetDrawContext = context;
    m_tilesetDrawBitmapData = bitmapData;
    m_tilesetDrawCurrentTile = -1;
}

void Context::setTilesetDrawCurrentTile(int tileIndex) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_tilesetDrawCurrentTile = tileIndex;
}

void Context::endTilesetDrawing() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_tilesetDrawContext = nullptr;
    m_tilesetDrawBitmapData = nullptr;
    m_tilesetDrawWidth = 0;
    m_tilesetDrawHeight = 0;
    m_tilesetDrawTileWidth = 0;
    m_tilesetDrawTileHeight = 0;
    m_tilesetDrawColumns = 0;
    m_tilesetDrawRows = 0;
    m_tilesetDrawCurrentTile = -1;
    m_tilesetDrawId = 0;
}

// =============================================================================
// Line Input Mode System
// =============================================================================

void Context::requestLineInput(int x, int y, const std::string& prompt, int fgColor, int bgColor) {
#ifndef VOICE_ONLY
    std::unique_lock<std::mutex> lock(m_lineInputState.mutex);
    
    // Set up line input request
    m_lineInputState.active = true;
    m_lineInputState.x = x;
    m_lineInputState.y = y;
    m_lineInputState.prompt = prompt;
    m_lineInputState.fgColor = fgColor;
    m_lineInputState.bgColor = bgColor;
    m_lineInputState.ready = false;
    m_lineInputState.result.clear();
    
    // Create the editor (will be driven by render thread)
    m_lineInputState.editor = std::make_unique<SuperTerminal::SimpleLineEditor>(x, y, prompt);
    
    // Wait for render thread to complete the input
    m_lineInputState.cv.wait(lock, [this]() { return m_lineInputState.ready; });
#endif
}

void Context::updateLineInput() {
#ifndef VOICE_ONLY
    // This runs on the render thread
    std::unique_lock<std::mutex> lock(m_lineInputState.mutex);
    
    if (!m_lineInputState.active || !m_lineInputState.editor) {
        return;
    }
    
    // Get input manager and text grid
    auto inputMgr = input();
    auto textGrid = this->textGrid();
    
    if (!inputMgr || !textGrid) {
        // Clean up and signal completion with empty result
        m_lineInputState.result.clear();
        m_lineInputState.ready = true;
        m_lineInputState.active = false;
        m_lineInputState.editor.reset();
        m_lineInputState.cv.notify_one();
        return;
    }
    
    // Update editor with input events (processes key presses)
    bool complete = m_lineInputState.editor->update(inputMgr);
    
    // Render editor to screen
    m_lineInputState.editor->render(textGrid);
    
    // If input is complete, store result and signal waiting thread
    if (complete || m_lineInputState.editor->isComplete()) {
        m_lineInputState.result = m_lineInputState.editor->getResult();
        m_lineInputState.ready = true;
        m_lineInputState.active = false;
        m_lineInputState.editor.reset();
        m_lineInputState.cv.notify_one();
    }
#endif
}

std::string Context::getLineInputResult() {
#ifndef VOICE_ONLY
    std::lock_guard<std::mutex> lock(m_lineInputState.mutex);
    return m_lineInputState.result;
#else
    return "";
#endif
}

bool Context::isLineInputActive() const {
#ifndef VOICE_ONLY
    std::lock_guard<std::mutex> lock(m_lineInputState.mutex);
    return m_lineInputState.active;
#else
    return false;
#endif
}

} // namespace STApi