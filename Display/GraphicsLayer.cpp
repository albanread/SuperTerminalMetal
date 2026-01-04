//
// GraphicsLayer.cpp
// SuperTerminal v2
//
// Graphics layer implementation with thread-safe command queue.
// Floating graphics layer, cpu drawing, 16 million, native format.

#include "GraphicsLayer.h"

namespace SuperTerminal {

// Pimpl implementation with single command buffer (retained mode)
struct GraphicsLayer::Impl {
    std::vector<GraphicsCommand> commands;
    mutable std::mutex mutex;

    void addCommand(const GraphicsCommand& cmd) {
        std::lock_guard<std::mutex> lock(mutex);
        commands.push_back(cmd);
    }

    void clearCommands() {
        std::lock_guard<std::mutex> lock(mutex);
        commands.clear();
    }

    std::vector<GraphicsCommand> getCommands() const {
        std::lock_guard<std::mutex> lock(mutex);
        return commands;
    }

    size_t getCount() const {
        std::lock_guard<std::mutex> lock(mutex);
        return commands.size();
    }
};

GraphicsLayer::GraphicsLayer()
    : m_impl(std::make_unique<Impl>())
    , m_dirty(true)  // Start dirty to ensure first render
    , m_visible(true)
    , m_lastRenderFrame(0)
{
}

GraphicsLayer::~GraphicsLayer() = default;

GraphicsLayer::GraphicsLayer(GraphicsLayer&& other) noexcept
    : m_impl(std::move(other.m_impl))
    , m_dirty(other.m_dirty)
    , m_visible(other.m_visible)
    , m_lastRenderFrame(other.m_lastRenderFrame)
{
}

GraphicsLayer& GraphicsLayer::operator=(GraphicsLayer&& other) noexcept {
    if (this != &other) {
        m_impl = std::move(other.m_impl);
        m_dirty = other.m_dirty;
        m_visible = other.m_visible;
        m_lastRenderFrame = other.m_lastRenderFrame;
    }
    return *this;
}

void GraphicsLayer::clear() {
    // Clear commands and add a full-screen transparent rectangle
    m_impl->clearCommands();

    // Draw full-screen transparent black rectangle (0, 0, 0, 0)
    // Use large dimensions to ensure full coverage
    fillRect(0, 0, 10000, 10000, 0.0f, 0.0f, 0.0f, 0.0f);

    markDirty();
}

void GraphicsLayer::swapBuffers() {
    // No-op: Double buffering removed, kept for API compatibility
}

void GraphicsLayer::drawRect(float x, float y, float width, float height,
                             float r, float g, float b, float a, float lineWidth) {
    GraphicsCommand cmd;
    cmd.type = DrawCommand::DrawRect;
    cmd.x = x;
    cmd.y = y;
    cmd.width = width;
    cmd.height = height;
    cmd.r = r;
    cmd.g = g;
    cmd.b = b;
    cmd.a = a;
    cmd.lineWidth = lineWidth;
    m_impl->addCommand(cmd);
    markDirty();
}

void GraphicsLayer::fillRect(float x, float y, float width, float height,
                             float r, float g, float b, float a) {
    GraphicsCommand cmd;
    cmd.type = DrawCommand::FillRect;
    cmd.x = x;
    cmd.y = y;
    cmd.width = width;
    cmd.height = height;
    cmd.r = r;
    cmd.g = g;
    cmd.b = b;
    cmd.a = a;
    m_impl->addCommand(cmd);
    markDirty();
}

void GraphicsLayer::drawCircle(float x, float y, float radius,
                               float r, float g, float b, float a, float lineWidth) {
    GraphicsCommand cmd;
    cmd.type = DrawCommand::DrawCircle;
    cmd.x = x;
    cmd.y = y;
    cmd.radius = radius;
    cmd.r = r;
    cmd.g = g;
    cmd.b = b;
    cmd.a = a;
    cmd.lineWidth = lineWidth;
    m_impl->addCommand(cmd);
    markDirty();
}

void GraphicsLayer::fillCircle(float x, float y, float radius,
                               float r, float g, float b, float a) {
    GraphicsCommand cmd;
    cmd.type = DrawCommand::FillCircle;
    cmd.x = x;
    cmd.y = y;
    cmd.radius = radius;
    cmd.r = r;
    cmd.g = g;
    cmd.b = b;
    cmd.a = a;
    m_impl->addCommand(cmd);
    markDirty();
}

void GraphicsLayer::drawLine(float x1, float y1, float x2, float y2,
                             float r, float g, float b, float a, float lineWidth) {
    GraphicsCommand cmd;
    cmd.type = DrawCommand::DrawLine;
    cmd.x = x1;
    cmd.y = y1;
    cmd.x2 = x2;
    cmd.y2 = y2;
    cmd.r = r;
    cmd.g = g;
    cmd.b = b;
    cmd.a = a;
    cmd.lineWidth = lineWidth;
    m_impl->addCommand(cmd);
    markDirty();
}

void GraphicsLayer::drawPixel(float x, float y, float r, float g, float b, float a) {
    GraphicsCommand cmd;
    cmd.type = DrawCommand::DrawPixel;
    cmd.x = x;
    cmd.y = y;
    cmd.r = r;
    cmd.g = g;
    cmd.b = b;
    cmd.a = a;
    m_impl->addCommand(cmd);
    markDirty();
}

std::vector<GraphicsCommand> GraphicsLayer::getCommands() const {
    return m_impl->getCommands();
}

std::vector<GraphicsCommand> GraphicsLayer::getBackBufferCommands() const {
    // Compatibility: return same buffer
    return m_impl->getCommands();
}

size_t GraphicsLayer::getCommandCount() const {
    return m_impl->getCount();
}

size_t GraphicsLayer::getBackBufferCommandCount() const {
    // Compatibility: return same count
    return m_impl->getCount();
}

bool GraphicsLayer::isEmpty() const {
    return m_impl->getCount() == 0;
}

bool GraphicsLayer::isBackBufferEmpty() const {
    // Compatibility: return same result
    return m_impl->getCount() == 0;
}

// =============================================================================
// Dirty Tracking & Visibility
// =============================================================================

bool GraphicsLayer::isDirty() const {
    std::lock_guard<std::mutex> lock(m_dirtyMutex);
    return m_dirty;
}

void GraphicsLayer::markDirty() {
    std::lock_guard<std::mutex> lock(m_dirtyMutex);
    m_dirty = true;
}

void GraphicsLayer::clearDirty() {
    std::lock_guard<std::mutex> lock(m_dirtyMutex);
    m_dirty = false;
}

bool GraphicsLayer::isVisible() const {
    std::lock_guard<std::mutex> lock(m_dirtyMutex);
    return m_visible;
}

void GraphicsLayer::setVisible(bool visible) {
    std::lock_guard<std::mutex> lock(m_dirtyMutex);
    if (m_visible != visible) {
        m_visible = visible;
        if (visible) {
            m_dirty = true;  // Mark dirty when becoming visible
        }
    }
}

uint64_t GraphicsLayer::getLastRenderFrame() const {
    std::lock_guard<std::mutex> lock(m_dirtyMutex);
    return m_lastRenderFrame;
}

void GraphicsLayer::setLastRenderFrame(uint64_t frame) {
    std::lock_guard<std::mutex> lock(m_dirtyMutex);
    m_lastRenderFrame = frame;
}

} // namespace SuperTerminal
