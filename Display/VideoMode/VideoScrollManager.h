//
// VideoScrollManager.h
// SuperTerminal Framework
//
// Hardware-accelerated scrolling and viewport management for video modes
// Manages scroll layers, coordinate transforms, and GPU-based rendering offsets
//
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#ifndef SUPERTERMINAL_VIDEO_SCROLL_MANAGER_H
#define SUPERTERMINAL_VIDEO_SCROLL_MANAGER_H

#include "../../API/st_api_video_scroll.h"
#include "../VideoMode.h"
#include <memory>
#include <vector>
#include <mutex>

namespace SuperTerminal {

// Forward declarations
class MetalRenderer;
class DisplayManager;

/// Internal scroll layer data
struct ScrollLayer {
    STScrollLayerID id;
    STScrollLayerConfig config;
    
    // Camera state
    float targetX;
    float targetY;
    float smoothness;
    bool hasTarget;
    
    // Camera bounds
    float boundsX;
    float boundsY;
    float boundsWidth;
    float boundsHeight;
    bool hasBounds;
    
    // Shake effect
    float shakeOffsetX;
    float shakeOffsetY;
    float shakeMagnitude;
    float shakeDuration;
    float shakeTimeRemaining;
    
    // Constructors
    ScrollLayer();
    explicit ScrollLayer(STScrollLayerID layerID);
    
    // Get effective scroll position (with shake applied)
    void getEffectiveOffset(float& outX, float& outY) const;
    
    // Apply camera bounds to position
    void applyBounds(float& x, float& y, float viewportW, float viewportH) const;
};

/// VideoScrollManager: Hardware-accelerated scrolling system
///
/// Responsibilities:
/// - Manage scroll layers for current video mode
/// - Apply GPU-based viewport transforms during rendering
/// - Handle camera follow, shake effects, and smooth scrolling
/// - Coordinate transformation between screen and world space
/// - Parallax layer management
///
/// Thread Safety:
/// - All public methods are thread-safe
/// - Internal state protected by mutex
class VideoScrollManager {
public:
    /// Constructor
    VideoScrollManager();
    
    /// Destructor
    ~VideoScrollManager();
    
    // Disable copy, allow move
    VideoScrollManager(const VideoScrollManager&) = delete;
    VideoScrollManager& operator=(const VideoScrollManager&) = delete;
    VideoScrollManager(VideoScrollManager&&) noexcept = default;
    VideoScrollManager& operator=(VideoScrollManager&&) noexcept = default;
    
    // =========================================================================
    // INITIALIZATION
    // =========================================================================
    
    /// Initialize scroll system for current video mode
    /// @param mode Current video mode
    /// @param displayManager Pointer to DisplayManager
    /// @return true if successful
    bool initialize(VideoMode mode, DisplayManager* displayManager);
    
    /// Shutdown scroll system
    void shutdown();
    
    /// Check if initialized
    bool isInitialized() const;
    
    /// Set the MetalRenderer (for GPU operations)
    void setRenderer(std::shared_ptr<MetalRenderer> renderer);
    
    // =========================================================================
    // LAYER MANAGEMENT
    // =========================================================================
    
    /// Create a new scroll layer
    STScrollLayerID createLayer(const STScrollLayerConfig* config);
    
    /// Destroy a layer
    void destroyLayer(STScrollLayerID layerID);
    
    /// Destroy all layers
    void destroyAllLayers();
    
    /// Get number of active layers
    int32_t getLayerCount() const;
    
    /// Check if layer is valid
    bool isValidLayer(STScrollLayerID layerID) const;
    
    /// Get layer by ID
    ScrollLayer* getLayer(STScrollLayerID layerID);
    const ScrollLayer* getLayer(STScrollLayerID layerID) const;
    
    /// Get all layers (sorted by depth)
    std::vector<ScrollLayer*> getLayersSorted();
    
    // =========================================================================
    // LAYER CONFIGURATION
    // =========================================================================
    
    void setLayerBuffer(STScrollLayerID layerID, int32_t bufferID);
    int32_t getLayerBuffer(STScrollLayerID layerID) const;
    
    void setLayerOffset(STScrollLayerID layerID, float x, float y);
    void getLayerOffset(STScrollLayerID layerID, float* x, float* y) const;
    void moveLayer(STScrollLayerID layerID, float dx, float dy);
    
    void setLayerViewport(STScrollLayerID layerID, float x, float y, float w, float h);
    void setLayerSource(STScrollLayerID layerID, float x, float y, float w, float h);
    
    void setLayerScale(STScrollLayerID layerID, float scaleX, float scaleY);
    void getLayerScale(STScrollLayerID layerID, float* scaleX, float* scaleY) const;
    
    void setLayerRotation(STScrollLayerID layerID, float degrees);
    float getLayerRotation(STScrollLayerID layerID) const;
    
    void setLayerDepth(STScrollLayerID layerID, int32_t depth);
    int32_t getLayerDepth(STScrollLayerID layerID) const;
    
    void setLayerBlendMode(STScrollLayerID layerID, STScrollBlendMode mode);
    STScrollBlendMode getLayerBlendMode(STScrollLayerID layerID) const;
    
    void setLayerWrapMode(STScrollLayerID layerID, STScrollWrapMode mode);
    STScrollWrapMode getLayerWrapMode(STScrollLayerID layerID) const;
    
    void setLayerOpacity(STScrollLayerID layerID, float opacity);
    float getLayerOpacity(STScrollLayerID layerID) const;
    
    void setLayerEnabled(STScrollLayerID layerID, bool enabled);
    bool isLayerEnabled(STScrollLayerID layerID) const;
    
    bool getLayerConfig(STScrollLayerID layerID, STScrollLayerConfig* config) const;
    void setLayerConfig(STScrollLayerID layerID, const STScrollLayerConfig* config);
    
    // =========================================================================
    // GLOBAL/SIMPLE SCROLLING (operates on layer 0)
    // =========================================================================
    
    void setGlobalScroll(float x, float y);
    void getGlobalScroll(float* x, float* y) const;
    void moveGlobalScroll(float dx, float dy);
    void resetGlobalScroll();
    
    void setGlobalWrapMode(STScrollWrapMode mode);
    STScrollWrapMode getGlobalWrapMode() const;
    
    // =========================================================================
    // CAMERA UTILITIES
    // =========================================================================
    
    void cameraFollow(STScrollLayerID layerID, float targetX, float targetY, float smoothness);
    void cameraSetBounds(STScrollLayerID layerID, float x, float y, float w, float h);
    void cameraClearBounds(STScrollLayerID layerID);
    void cameraShake(STScrollLayerID layerID, float magnitude, float duration);
    void cameraCenterOn(STScrollLayerID layerID, float worldX, float worldY);
    
    // =========================================================================
    // PARALLAX HELPERS
    // =========================================================================
    
    STScrollLayerID parallaxCreate(int32_t numLayers, const int32_t* bufferIDs, const float* speedFactors);
    void parallaxUpdate(STScrollLayerID firstLayerID, int32_t numLayers, float dx, float dy);
    
    // =========================================================================
    // COORDINATE TRANSFORMATION
    // =========================================================================
    
    void screenToWorld(STScrollLayerID layerID, float screenX, float screenY, float* worldX, float* worldY) const;
    void worldToScreen(STScrollLayerID layerID, float worldX, float worldY, float* screenX, float* screenY) const;
    
    // =========================================================================
    // SYSTEM UPDATE
    // =========================================================================
    
    /// Update scroll system (camera smoothing, shake, etc.)
    /// @param dt Delta time in seconds
    void update(float dt);
    
    /// Get statistics
    void getStats(int32_t* layerCount, size_t* gpuMemoryBytes) const;
    
    // =========================================================================
    // RENDERING INTEGRATION
    // =========================================================================
    
    /// Get GPU uniforms for a layer (used by Metal renderer)
    /// @param layerID Layer ID
    /// @param outUniforms Output uniform buffer data
    /// @return true if layer is valid and enabled
    bool getLayerGPUUniforms(STScrollLayerID layerID, void* outUniforms) const;
    
    /// Apply scroll transforms to all layers during rendering
    /// Called by DisplayManager/MetalRenderer before video mode rendering
    void applyScrollTransforms();
    
private:
    // =========================================================================
    // INTERNAL STATE
    // =========================================================================
    
    bool m_initialized;
    VideoMode m_videoMode;
    DisplayManager* m_displayManager;
    std::shared_ptr<MetalRenderer> m_renderer;
    
    // Layer storage
    std::vector<ScrollLayer> m_layers;
    STScrollLayerID m_nextLayerID;
    
    // Global layer (always exists at index 0)
    static constexpr STScrollLayerID GLOBAL_LAYER_ID = 0;
    
    // Video mode dimensions
    int m_modeWidth;
    int m_modeHeight;
    
    // Thread safety
    mutable std::mutex m_mutex;
    
    // =========================================================================
    // INTERNAL HELPERS
    // =========================================================================
    
    /// Ensure global layer exists
    void ensureGlobalLayer();
    
    /// Find layer index by ID
    int findLayerIndex(STScrollLayerID layerID) const;
    
    /// Allocate new layer ID
    STScrollLayerID allocateLayerID();
    
    /// Update camera follow for a layer
    void updateCameraFollow(ScrollLayer& layer, float dt);
    
    /// Update camera shake for a layer
    void updateCameraShake(ScrollLayer& layer, float dt);
    
    /// Get video mode dimensions
    void getModeDimensions(int& width, int& height) const;
    
    /// Clamp value to range
    static float clamp(float value, float min, float max);
    
    /// Linear interpolation
    static float lerp(float a, float b, float t);
};

} // namespace SuperTerminal

#endif // SUPERTERMINAL_VIDEO_SCROLL_MANAGER_H