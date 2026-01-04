//
// TilemapRenderer.mm
// SuperTerminal Framework - Tilemap System
//
// Metal-based GPU-accelerated tilemap renderer implementation
// Copyright © 2024 SuperTerminal. All rights reserved.
//
// IMPLEMENTATION NOTES:
// - Uses v1-style explicit vertex buffer approach (simple, works reliably)
// - Camera position is top-left of viewport (not center)
// - Tiles positioned relative to viewport, converted to NDC in shader
//
// NOT IMPLEMENTED:
// - Zoom (culling uses it, but rendering doesn't scale tiles)
// - Z-order layering (layers render in order added, no sorting)
// - Tile rotation and flipping (flags ignored)
// - Tile animations (animation system exists but not used)
//

#import "TilemapRenderer.h"
#import "TilemapLayer.h"
#import "Tilemap.h"
#import "Tileset.h"
#import "TilesetIndexed.h"
#import "PaletteBank.h"
#import "TilemapEx.h"
#import "Camera.h"
#import "TileData.h"
#import "TileDataEx.h"
#import <Metal/Metal.h>
#import <simd/simd.h>
#import <vector>
#import <unordered_map>
#import <chrono>

namespace SuperTerminal {

// =============================================================================
// Implementation Structure
// =============================================================================

struct TilemapRenderer::Impl {
    // Metal resources
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> commandQueue = nil;
    id<MTLRenderPipelineState> pipelineState = nil;
    id<MTLRenderPipelineState> debugPipelineState = nil;
    id<MTLSamplerState> samplerState = nil;

    // Indexed rendering pipeline states
    id<MTLRenderPipelineState> pipelineStateIndexed = nil;
    id<MTLRenderPipelineState> debugPipelineStateIndexed = nil;
    id<MTLSamplerState> samplerStateIndexed = nil;  // Nearest-neighbor for index lookup

    // Buffers
    id<MTLBuffer> vertexBuffer = nil;
    id<MTLBuffer> uniformBuffer = nil;
    std::vector<TileInstance> instanceData;
    std::vector<TileInstanceIndexed> instanceDataIndexed;
    TilemapUniforms uniforms;

    // Rendering state
    id<MTLRenderCommandEncoder> currentEncoder = nil;
    CAMetalLayer* metalLayer = nil;
    uint32_t viewportWidth = 0;
    uint32_t viewportHeight = 0;

    // Configuration
    TilemapRenderConfig config;
    bool initialized = false;

    // Statistics
    TilemapRenderStats stats;
    std::chrono::high_resolution_clock::time_point frameStartTime;

    // Cached resources
    std::unordered_map<const Tileset*, id<MTLTexture>> tilesetCache;
    std::unordered_map<const TilesetIndexed*, id<MTLTexture>> tilesetIndexedCache;

    Impl() = default;
    ~Impl() {
        cleanup();
    }

    void cleanup() {
        tilesetCache.clear();
        tilesetIndexedCache.clear();
        vertexBuffer = nil;
        uniformBuffer = nil;
        samplerState = nil;
        samplerStateIndexed = nil;
        pipelineState = nil;
        debugPipelineState = nil;
        pipelineStateIndexed = nil;
        debugPipelineStateIndexed = nil;
        commandQueue = nil;
        device = nil;
        metalLayer = nil;
        initialized = false;
    }
};

// =============================================================================
// Construction
// =============================================================================

TilemapRenderer::TilemapRenderer(MTLDevicePtr device)
    : m_impl(std::make_unique<Impl>())
{
    if (device) {
        m_impl->device = device;
    } else {
        m_impl->device = MTLCreateSystemDefaultDevice();
    }
}

TilemapRenderer::~TilemapRenderer() {
    shutdown();
}

TilemapRenderer::TilemapRenderer(TilemapRenderer&&) noexcept = default;
TilemapRenderer& TilemapRenderer::operator=(TilemapRenderer&&) noexcept = default;

// =============================================================================
// Initialization
// =============================================================================

bool TilemapRenderer::initialize(CAMetalLayer* layer, uint32_t viewportWidth, uint32_t viewportHeight) {
    if (!m_impl->device) {
        NSLog(@"[TilemapRenderer] ERROR: No Metal device available");
        return false;
    }

    m_impl->metalLayer = layer;

    m_impl->viewportWidth = viewportWidth;
    m_impl->viewportHeight = viewportHeight;

    // Create command queue
    m_impl->commandQueue = [m_impl->device newCommandQueue];
    if (!m_impl->commandQueue) {
        NSLog(@"[TilemapRenderer] ERROR: Failed to create command queue");
        return false;
    }

    // Create pipeline states
    createPipelineState();

    // Create buffers
    createBuffers();

    // Create sampler state
    createSamplerState();

    // Initialize uniforms
    m_impl->uniforms.viewportSize[0] = static_cast<float>(viewportWidth);
    m_impl->uniforms.viewportSize[1] = static_cast<float>(viewportHeight);

    m_impl->initialized = true;
    NSLog(@"[TilemapRenderer] Initialized successfully (%dx%d)", viewportWidth, viewportHeight);

    return true;
}

bool TilemapRenderer::isInitialized() const {
    return m_impl->initialized;
}

void TilemapRenderer::shutdown() {
    if (m_impl) {
        m_impl->cleanup();
    }
}

// =============================================================================
// Configuration
// =============================================================================

void TilemapRenderer::setConfig(const TilemapRenderConfig& config) {
    m_impl->config = config;
}

const TilemapRenderConfig& TilemapRenderer::getConfig() const {
    return m_impl->config;
}

void TilemapRenderer::setViewportSize(uint32_t width, uint32_t height) {
    m_impl->viewportWidth = width;
    m_impl->viewportHeight = height;
    m_impl->uniforms.viewportSize[0] = static_cast<float>(width);
    m_impl->uniforms.viewportSize[1] = static_cast<float>(height);
}

void TilemapRenderer::getViewportSize(uint32_t& width, uint32_t& height) const {
    width = m_impl->viewportWidth;
    height = m_impl->viewportHeight;
}

// =============================================================================
// Rendering
// =============================================================================

bool TilemapRenderer::beginFrame(MTLRenderCommandEncoderPtr commandEncoder) {
    if (!m_impl->initialized) {
        return false;
    }

    m_impl->currentEncoder = commandEncoder;
    m_impl->stats.reset();
    m_impl->frameStartTime = std::chrono::high_resolution_clock::now();

    return true;
}

bool TilemapRenderer::renderLayer(const TilemapLayer& layer, const Camera& camera, float time) {
    if (!m_impl->currentEncoder || !validateLayer(layer)) {
        return false;
    }

    // Get tilemap and tileset
    const Tilemap* tilemap = layer.getTilemapPtr();
    const Tileset* tileset = layer.getTilesetPtr();

    if (!tilemap || !tileset || !tileset->isValid()) {
        return false; // Nothing to render
    }

    // Check visibility
    if (!layer.isVisible() || layer.getOpacity() < 0.01f) {
        NSLog(@"[TilemapRenderer] Layer invisible or transparent");
        return true; // Layer is invisible, but not an error
    }

    // Build instance buffer for visible tiles
    buildInstanceBuffer(layer, camera, time);

    if (m_impl->instanceData.empty()) {
        NSLog(@"[TilemapRenderer] No instances created - empty instanceData");
        return true; // No visible tiles, but not an error
    }

    NSLog(@"[TilemapRenderer] renderLayer: About to render %zu instances", m_impl->instanceData.size());

    // Update uniforms
    updateUniforms(camera, layer);
    m_impl->uniforms.time = time;
    m_impl->uniforms.opacity = layer.getOpacity();

    // Upload uniforms
    memcpy([m_impl->uniformBuffer contents], &m_impl->uniforms, sizeof(TilemapUniforms));

    // Note: Vertex buffer is built during render call (v1 style)

    // Get or cache tileset texture
    id<MTLTexture> texture = nil;
    auto it = m_impl->tilesetCache.find(tileset);
    if (it != m_impl->tilesetCache.end()) {
        texture = it->second;
    } else {
        // Get texture from tileset (assumes it's already a Metal texture)
        texture = tileset->getTexture();
        if (texture) {
            m_impl->tilesetCache[tileset] = texture;
        }
    }

    if (!texture) {
        NSLog(@"[TilemapRenderer] WARNING: No texture for tileset");
        return false;
    }

    // Set pipeline state
    id<MTLRenderPipelineState> pipeline = m_impl->pipelineState;
    if (!pipeline) {
        NSLog(@"[TilemapRenderer] ERROR: No pipeline state!");
        return false;
    }

    [m_impl->currentEncoder setRenderPipelineState:pipeline];

    // Build explicit vertex buffer (v1 style)
    size_t instanceCount = m_impl->instanceData.size();
    size_t vertexCount = instanceCount * 6; // 6 vertices per tile quad

    // Allocate or resize vertex buffer if needed
    size_t vertexBufferSize = vertexCount * sizeof(TileVertex);
    if (!m_impl->vertexBuffer || [m_impl->vertexBuffer length] < vertexBufferSize) {
        m_impl->vertexBuffer = [m_impl->device newBufferWithLength:vertexBufferSize
                                                            options:MTLResourceStorageModeShared];
        NSLog(@"[TilemapRenderer] Allocated vertex buffer: %zu bytes for %zu vertices (%zu tiles)",
              vertexBufferSize, vertexCount, instanceCount);
    }

    // Build vertices from instances
    TileVertex* vertices = (TileVertex*)[m_impl->vertexBuffer contents];
    size_t vertexIndex = 0;

    for (size_t i = 0; i < instanceCount; i++) {
        const TileInstance& inst = m_impl->instanceData[i];
        float x = inst.position[0];
        float y = inst.position[1];
        float tileWidth = m_impl->uniforms.tileSize[0];
        float tileHeight = m_impl->uniforms.tileSize[1];

        float uMin = inst.texCoordBase[0];
        float vMin = inst.texCoordBase[1];
        float uMax = uMin + inst.texCoordSize[0];
        float vMax = vMin + inst.texCoordSize[1];

        // Triangle 1
        vertices[vertexIndex++] = {
            .position = {x, y},
            .texCoord = {uMin, vMin},
            .tileId = 0,
            .flags = 0
        };
        vertices[vertexIndex++] = {
            .position = {x + tileWidth, y},
            .texCoord = {uMax, vMin},
            .tileId = 0,
            .flags = 0
        };
        vertices[vertexIndex++] = {
            .position = {x, y + tileHeight},
            .texCoord = {uMin, vMax},
            .tileId = 0,
            .flags = 0
        };

        // Triangle 2
        vertices[vertexIndex++] = {
            .position = {x + tileWidth, y},
            .texCoord = {uMax, vMin},
            .tileId = 0,
            .flags = 0
        };
        vertices[vertexIndex++] = {
            .position = {x + tileWidth, y + tileHeight},
            .texCoord = {uMax, vMax},
            .tileId = 0,
            .flags = 0
        };
        vertices[vertexIndex++] = {
            .position = {x, y + tileHeight},
            .texCoord = {uMin, vMax},
            .tileId = 0,
            .flags = 0
        };
    }

    // Bind resources
    [m_impl->currentEncoder setVertexBuffer:m_impl->vertexBuffer offset:0 atIndex:0];
    [m_impl->currentEncoder setVertexBuffer:m_impl->uniformBuffer offset:0 atIndex:1];

    // Set viewport size buffer
    simd_float2 viewportSize = simd_make_float2(m_impl->viewportWidth, m_impl->viewportHeight);
    [m_impl->currentEncoder setVertexBytes:&viewportSize length:sizeof(simd_float2) atIndex:2];

    [m_impl->currentEncoder setFragmentTexture:texture atIndex:0];
    [m_impl->currentEncoder setFragmentSamplerState:m_impl->samplerState atIndex:0];

    // Draw vertices
    NSLog(@"[TilemapRenderer] Drawing %zu vertices (%zu tiles)", vertexCount, instanceCount);
    [m_impl->currentEncoder drawPrimitives:MTLPrimitiveTypeTriangle
                               vertexStart:0
                               vertexCount:vertexCount];

    // Update stats
    m_impl->stats.layersRendered++;
    m_impl->stats.tilesRendered += static_cast<uint32_t>(instanceCount);
    m_impl->stats.drawCalls++;

    return true;
}

void TilemapRenderer::endFrame() {
    m_impl->currentEncoder = nil;

    // Calculate frame time
    auto frameEndTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        frameEndTime - m_impl->frameStartTime);
    m_impl->stats.frameTime = duration.count() / 1000.0f; // Convert to milliseconds
}

const TilemapRenderStats& TilemapRenderer::getStats() const {
    return m_impl->stats;
}

// =============================================================================
// Resource Management
// =============================================================================

bool TilemapRenderer::preloadTileset(const Tileset& tileset) {
    if (!tileset.isValid()) {
        return false;
    }

    id<MTLTexture> texture = tileset.getTexture();
    if (texture) {
        m_impl->tilesetCache[&tileset] = texture;
        return true;
    }

    return false;
}

void TilemapRenderer::unloadTileset(const Tileset& tileset) {
    m_impl->tilesetCache.erase(&tileset);
}

void TilemapRenderer::clearCache() {
    m_impl->tilesetCache.clear();
}

// =============================================================================
// Debug
// =============================================================================

void TilemapRenderer::setDebugOverlay(bool enabled) {
    m_impl->config.enableDebugOverlay = enabled;
}

bool TilemapRenderer::getDebugOverlay() const {
    return m_impl->config.enableDebugOverlay;
}

void TilemapRenderer::printStats() const {
    const auto& stats = m_impl->stats;
    NSLog(@"[TilemapRenderer Stats]");
    NSLog(@"  Layers: %u", stats.layersRendered);
    NSLog(@"  Tiles: %u (culled: %u)", stats.tilesRendered, stats.tilesCulled);
    NSLog(@"  Draw calls: %u", stats.drawCalls);
    NSLog(@"  Frame time: %.2f ms", stats.frameTime);
}

// =============================================================================
// Helper Methods
// =============================================================================

void TilemapRenderer::buildInstanceBuffer(const TilemapLayer& layer, const Camera& camera, float time) {
    m_impl->instanceData.clear();

    const Tilemap* tilemap = layer.getTilemapPtr();
    const Tileset* tileset = layer.getTilesetPtr();

    if (!tilemap || !tileset) {
        return;
    }

    int32_t mapWidth = tilemap->getWidth();
    int32_t mapHeight = tilemap->getHeight();
    int32_t tileWidth = tilemap->getTileWidth();
    int32_t tileHeight = tilemap->getTileHeight();

    // Camera x,y is top-left of viewport (v1-style scroll offset)
    float camX = camera.getX();
    float camY = camera.getY();
    float zoom = camera.getZoom();  // Used for culling only (rendering doesn't scale tiles)

    float parallaxX = layer.getParallaxX();
    float parallaxY = layer.getParallaxY();

    float width = static_cast<float>(m_impl->viewportWidth);
    float height = static_cast<float>(m_impl->viewportHeight);

    // Viewport bounds (camera is top-left corner)
    float viewportLeft = camX * parallaxX;
    float viewportTop = camY * parallaxY;
    float viewportRight = viewportLeft + (width / zoom);
    float viewportBottom = viewportTop + (height / zoom);

    // Calculate which tiles are visible
    int32_t startCol = std::max(0, (int32_t)std::floor(viewportLeft / tileWidth));
    int32_t endCol = std::min(mapWidth, (int32_t)std::ceil(viewportRight / tileWidth) + 1);
    int32_t startRow = std::max(0, (int32_t)std::floor(viewportTop / tileHeight));
    int32_t endRow = std::min(mapHeight, (int32_t)std::ceil(viewportBottom / tileHeight) + 1);

    NSLog(@"[TilemapRenderer] Culling: viewport=%.0fx%.0f zoom=%.2f", width, height, zoom);
    NSLog(@"[TilemapRenderer] Culling: bounds L=%.1f R=%.1f T=%.1f B=%.1f", viewportLeft, viewportRight, viewportTop, viewportBottom);
    NSLog(@"[TilemapRenderer] Culling: tiles col[%d-%d] row[%d-%d] = %dx%d tiles", startCol, endCol, startRow, endRow, (endCol-startCol), (endRow-startRow));

    // Reserve space for instances
    int32_t visibleTiles = (endCol - startCol) * (endRow - startRow);
    m_impl->instanceData.reserve(visibleTiles);

    // Build instance data for each visible tile
    int emptyTileCount = 0;
    int renderedTileCount = 0;
    for (int32_t row = startRow; row < endRow; row++) {
        for (int32_t col = startCol; col < endCol; col++) {
            TileData tile = tilemap->getTile(col, row);
            uint16_t tileID = tile.getTileID();

            if (tileID == 0) {
                emptyTileCount++;
                continue; // Empty tile
            }

            renderedTileCount++;
            if (renderedTileCount <= 5 || renderedTileCount > (visibleTiles - 5)) {
                NSLog(@"[TilemapRenderer] Tile at (%d,%d) = ID %d", col, row, tileID);
            }

            // Get animated tile ID if applicable
            if (tileset->hasAnimation(tileID)) {
                tileID = tileset->getAnimatedTile(tileID, time);
            }

            // Get texture coordinates
            TexCoords texCoords = tileset->getTexCoords(tileID);

            // Create instance
            TileInstance instance;

            // Position relative to viewport top-left (v1-style)
            float worldX = col * tileWidth;
            float worldY = row * tileHeight;
            instance.position[0] = worldX - viewportLeft;
            instance.position[1] = worldY - viewportTop;

            // Texture coordinates
            instance.texCoordBase[0] = texCoords.u;
            instance.texCoordBase[1] = texCoords.v;
            instance.texCoordSize[0] = texCoords.width;
            instance.texCoordSize[1] = texCoords.height;

            // Tint color (white = no tint)
            instance.tintColor[0] = 1.0f;
            instance.tintColor[1] = 1.0f;
            instance.tintColor[2] = 1.0f;
            instance.tintColor[3] = 1.0f;

            // Flags (flip, rotation)
            instance.flags = 0;
            if (tile.getFlipX()) instance.flags |= 0x02;
            if (tile.getFlipY()) instance.flags |= 0x04;

            uint8_t rotation = tile.getRotation();
            if (rotation == 1) instance.flags |= 0x08;      // 90°
            else if (rotation == 2) instance.flags |= 0x10; // 180°
            else if (rotation == 3) instance.flags |= 0x20; // 270°

            m_impl->instanceData.push_back(instance);
        }
    }

    // Update culling stats
    int32_t totalTiles = mapWidth * mapHeight;
    int32_t renderedTiles = static_cast<int32_t>(m_impl->instanceData.size());
    m_impl->stats.tilesCulled += (totalTiles - renderedTiles);

    NSLog(@"[TilemapRenderer] Visible region: %d tiles, Empty: %d, Rendered: %d", visibleTiles, emptyTileCount, renderedTiles);
}

void TilemapRenderer::updateUniforms(const Camera& camera, const TilemapLayer& layer) {
    // Note: Projection matrix not needed - shader converts directly to NDC
    // Camera x,y is top-left of viewport, tiles are positioned relative to it

    // Set tile size
    const Tilemap* tilemap = layer.getTilemapPtr();
    if (tilemap) {
        m_impl->uniforms.tileSize[0] = static_cast<float>(tilemap->getTileWidth());
        m_impl->uniforms.tileSize[1] = static_cast<float>(tilemap->getTileHeight());
    }
}

void TilemapRenderer::createPipelineState() {
    NSError* error = nil;

    // Inline Metal shader source
    NSString* shaderSource = @R"(
        #include <metal_stdlib>
        using namespace metal;

        struct TileVertexIn {
            float2 position [[attribute(0)]];
            float2 texCoord [[attribute(1)]];
            uint16_t tileId [[attribute(2)]];
            uint16_t flags [[attribute(3)]];
        };

        struct TileVertexOut {
            float4 position [[position]];
            float2 texCoord;
        };

        struct TilemapUniforms {
            float4x4 viewProjectionMatrix;
            float2 viewportSize;
            float2 tileSize;
            float time;
            float opacity;
            uint flags;
            uint padding;
        };

        vertex TileVertexOut tilemapVertexShader(
            TileVertexIn in [[stage_in]],
            constant TilemapUniforms& uniforms [[buffer(1)]],
            constant float2& viewportSize [[buffer(2)]]
        ) {
            TileVertexOut out;

            // Convert viewport-relative position to NDC (normalized device coordinates)
            // Position is already relative to viewport top-left (0,0)
            float2 screenPos = in.position;

            // Map to NDC: (0,0) -> (-1,1), (width,height) -> (1,-1)
            out.position.x = (screenPos.x / viewportSize.x) * 2.0 - 1.0;
            out.position.y = 1.0 - (screenPos.y / viewportSize.y) * 2.0;
            out.position.z = 0.0;
            out.position.w = 1.0;

            out.texCoord = in.texCoord;

            return out;
        }

        fragment float4 tilemapFragmentShader(
            TileVertexOut in [[stage_in]],
            texture2d<float> tilesetTexture [[texture(0)]],
            sampler textureSampler [[sampler(0)]]
        ) {
            float4 color = tilesetTexture.sample(textureSampler, in.texCoord);

            if (color.a < 0.01) {
                discard_fragment();
            }

            return color;
        }

        fragment float4 tilemapFragmentShaderDebug(
            TileVertexOut in [[stage_in]],
            texture2d<float> tilesetTexture [[texture(0)]],
            sampler textureSampler [[sampler(0)]],
            constant TilemapUniforms& uniforms [[buffer(0)]]
        ) {
            float4 texColor = tilesetTexture.sample(textureSampler, in.texCoord);
            float4 finalColor = texColor;
            float2 uv = in.texCoord;
            float2 gridUV = fract(uv * uniforms.tileSize);
            float gridLine = step(0.95, max(gridUV.x, gridUV.y));
            finalColor = mix(finalColor, float4(1.0, 1.0, 0.0, 1.0), gridLine * 0.3);
            return finalColor;
        }
    )";

    // Compile shader library from source
    id<MTLLibrary> library = [m_impl->device newLibraryWithSource:shaderSource options:nil error:&error];
    if (!library) {
        NSLog(@"[TilemapRenderer] ERROR: Failed to compile Metal shaders: %@", error);
        return;
    }

    // Get shader functions
    id<MTLFunction> vertexFunction = [library newFunctionWithName:@"tilemapVertexShader"];
    id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"tilemapFragmentShader"];
    id<MTLFunction> debugFragmentFunction = [library newFunctionWithName:@"tilemapFragmentShaderDebug"];

    if (!vertexFunction || !fragmentFunction) {
        NSLog(@"[TilemapRenderer] ERROR: Failed to load shader functions");
        return;
    }

    NSLog(@"[TilemapRenderer] Shaders compiled successfully");

    // Create pipeline descriptor
    MTLRenderPipelineDescriptor* pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDescriptor.vertexFunction = vertexFunction;
    pipelineDescriptor.fragmentFunction = fragmentFunction;
    pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

    // Set up vertex descriptor
    MTLVertexDescriptor* vertexDescriptor = [[MTLVertexDescriptor alloc] init];
    vertexDescriptor.attributes[0].format = MTLVertexFormatFloat2;
    vertexDescriptor.attributes[0].offset = 0;
    vertexDescriptor.attributes[0].bufferIndex = 0;
    vertexDescriptor.attributes[1].format = MTLVertexFormatFloat2;
    vertexDescriptor.attributes[1].offset = sizeof(simd_float2);
    vertexDescriptor.attributes[1].bufferIndex = 0;
    vertexDescriptor.attributes[2].format = MTLVertexFormatUShort;
    vertexDescriptor.attributes[2].offset = sizeof(simd_float2) * 2;
    vertexDescriptor.attributes[2].bufferIndex = 0;
    vertexDescriptor.attributes[3].format = MTLVertexFormatUShort;
    vertexDescriptor.attributes[3].offset = sizeof(simd_float2) * 2 + sizeof(uint16_t);
    vertexDescriptor.attributes[3].bufferIndex = 0;
    vertexDescriptor.layouts[0].stride = sizeof(TileVertex);
    vertexDescriptor.layouts[0].stepRate = 1;
    vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

    pipelineDescriptor.vertexDescriptor = vertexDescriptor;

    // Enable alpha blending
    pipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
    pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
    pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

    // Create normal pipeline state
    m_impl->pipelineState = [m_impl->device newRenderPipelineStateWithDescriptor:pipelineDescriptor
                                                                           error:&error];
    if (!m_impl->pipelineState) {
        NSLog(@"[TilemapRenderer] ERROR: Failed to create pipeline state: %@", error);
        return;
    }

    // Create debug pipeline state
    if (debugFragmentFunction) {
        pipelineDescriptor.fragmentFunction = debugFragmentFunction;
        m_impl->debugPipelineState = [m_impl->device newRenderPipelineStateWithDescriptor:pipelineDescriptor
                                                                                    error:&error];
        if (!m_impl->debugPipelineState) {
            NSLog(@"[TilemapRenderer] WARNING: Failed to create debug pipeline state: %@", error);
        }
    }

    NSLog(@"[TilemapRenderer] Pipeline states created successfully");
}

void TilemapRenderer::createBuffers() {
    // Create uniform buffer (one per frame, shared mode for CPU writes)
    m_impl->uniformBuffer = [m_impl->device newBufferWithLength:sizeof(TilemapUniforms)
                                                         options:MTLResourceStorageModeShared];

    // Vertex buffer is created dynamically during render (v1 style)
    // Initial allocation for reasonable number of tiles
    size_t initialVertexCount = 1024 * 6; // 1024 tiles
    m_impl->vertexBuffer = [m_impl->device newBufferWithLength:initialVertexCount * sizeof(TileVertex)
                                                        options:MTLResourceStorageModeShared];
}

void TilemapRenderer::createSamplerState() {
    MTLSamplerDescriptor* samplerDescriptor = [[MTLSamplerDescriptor alloc] init];
    samplerDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
    samplerDescriptor.magFilter = MTLSamplerMinMagFilterLinear;
    samplerDescriptor.mipFilter = m_impl->config.enableMipmaps ?
                                   MTLSamplerMipFilterLinear : MTLSamplerMipFilterNotMipmapped;
    samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.normalizedCoordinates = YES;

    m_impl->samplerState = [m_impl->device newSamplerStateWithDescriptor:samplerDescriptor];

    NSLog(@"[TilemapRenderer] Sampler state created successfully");
}

bool TilemapRenderer::validateLayer(const TilemapLayer& layer) const {
    if (!layer.getTilemap()) {
        NSLog(@"[TilemapRenderer] WARNING: Layer has no tilemap");
        return false;
    }

    if (!layer.getTileset()) {
        NSLog(@"[TilemapRenderer] WARNING: Layer has no tileset");
        return false;
    }

    return true;
}

// =============================================================================
// Indexed Rendering Pipeline
// =============================================================================

void TilemapRenderer::createPipelineStateIndexed() {
    NSError* error = nil;

    // Load shader library from framework bundle (or inline if needed)
    id<MTLLibrary> library = [m_impl->device newDefaultLibrary];
    if (!library) {
        NSLog(@"[TilemapRenderer] ERROR: Failed to load Metal library for indexed rendering");
        return;
    }

    // Get shader functions for indexed rendering
    id<MTLFunction> vertexFunction = [library newFunctionWithName:@"tilemapVertexShaderIndexed"];
    id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"tilemapFragmentShaderIndexed"];
    id<MTLFunction> debugFragmentFunction = [library newFunctionWithName:@"tilemapFragmentShaderIndexedDebug"];

    if (!vertexFunction || !fragmentFunction) {
        NSLog(@"[TilemapRenderer] ERROR: Failed to load indexed shader functions");
        return;
    }

    // Create pipeline descriptor
    MTLRenderPipelineDescriptor* pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDescriptor.vertexFunction = vertexFunction;
    pipelineDescriptor.fragmentFunction = fragmentFunction;
    pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

    // Enable alpha blending
    pipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
    pipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    pipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

    // Create vertex descriptor for indexed tiles
    MTLVertexDescriptor* vertexDescriptor = [[MTLVertexDescriptor alloc] init];

    // Position (float2)
    vertexDescriptor.attributes[0].format = MTLVertexFormatFloat2;
    vertexDescriptor.attributes[0].offset = 0;
    vertexDescriptor.attributes[0].bufferIndex = 0;

    // TexCoord (float2)
    vertexDescriptor.attributes[1].format = MTLVertexFormatFloat2;
    vertexDescriptor.attributes[1].offset = 8;
    vertexDescriptor.attributes[1].bufferIndex = 0;

    // Layout
    vertexDescriptor.layouts[0].stride = sizeof(float) * 4; // position + texCoord
    vertexDescriptor.layouts[0].stepRate = 1;
    vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

    pipelineDescriptor.vertexDescriptor = vertexDescriptor;

    // Create pipeline state
    m_impl->pipelineStateIndexed = [m_impl->device newRenderPipelineStateWithDescriptor:pipelineDescriptor
                                                                                   error:&error];
    if (!m_impl->pipelineStateIndexed) {
        NSLog(@"[TilemapRenderer] ERROR: Failed to create indexed pipeline state: %@", error);
        return;
    }

    // Create debug pipeline state
    if (debugFragmentFunction) {
        pipelineDescriptor.fragmentFunction = debugFragmentFunction;
        m_impl->debugPipelineStateIndexed = [m_impl->device newRenderPipelineStateWithDescriptor:pipelineDescriptor
                                                                                            error:&error];
        if (!m_impl->debugPipelineStateIndexed) {
            NSLog(@"[TilemapRenderer] WARNING: Failed to create indexed debug pipeline state: %@", error);
        }
    }

    // Create nearest-neighbor sampler for index texture
    MTLSamplerDescriptor* samplerDescriptor = [[MTLSamplerDescriptor alloc] init];
    samplerDescriptor.minFilter = MTLSamplerMinMagFilterNearest;
    samplerDescriptor.magFilter = MTLSamplerMinMagFilterNearest;
    samplerDescriptor.mipFilter = MTLSamplerMipFilterNotMipmapped;
    samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.normalizedCoordinates = YES;

    m_impl->samplerStateIndexed = [m_impl->device newSamplerStateWithDescriptor:samplerDescriptor];

    NSLog(@"[TilemapRenderer] Indexed pipeline states created successfully");
}

bool TilemapRenderer::preloadTilesetIndexed(const TilesetIndexed& tileset) {
    if (!m_impl->initialized) {
        return false;
    }

    // Get texture from tileset (should already be a Metal texture)
    id<MTLTexture> texture = tileset.getTexture();
    if (!texture) {
        NSLog(@"[TilemapRenderer] WARNING: Indexed tileset has no texture");
        return false;
    }

    // Cache the texture
    m_impl->tilesetIndexedCache[&tileset] = texture;

    NSLog(@"[TilemapRenderer] Preloaded indexed tileset: %dx%d, %d tiles",
          (int)tileset.getTileWidth(), (int)tileset.getTileHeight(), (int)tileset.getTileCount());

    return true;
}

void TilemapRenderer::unloadTilesetIndexed(const TilesetIndexed& tileset) {
    auto it = m_impl->tilesetIndexedCache.find(&tileset);
    if (it != m_impl->tilesetIndexedCache.end()) {
        m_impl->tilesetIndexedCache.erase(it);
    }
}

void TilemapRenderer::buildInstanceBufferIndexed(const TilemapLayer& layer, const Camera& camera, float time) {
    m_impl->instanceDataIndexed.clear();

    const Tilemap* tilemap = layer.getTilemapPtr();
    if (!tilemap) {
        return;
    }

    // Get camera bounds
    float camX = camera.getX();
    float camY = camera.getY();
    float camW = static_cast<float>(m_impl->viewportWidth);
    float camH = static_cast<float>(m_impl->viewportHeight);

    // Get tile size
    const Tileset* tileset = layer.getTilesetPtr();
    if (!tileset) {
        return;
    }
    uint32_t tileWidth = tileset->getTileWidth();
    uint32_t tileHeight = tileset->getTileHeight();

    if (tileWidth == 0 || tileHeight == 0) {
        return;
    }

    // Calculate visible tile range
    int32_t startX = static_cast<int32_t>(camX / tileWidth);
    int32_t startY = static_cast<int32_t>(camY / tileHeight);
    int32_t endX = static_cast<int32_t>((camX + camW) / tileWidth) + 1;
    int32_t endY = static_cast<int32_t>((camY + camH) / tileHeight) + 1;

    // Clamp to tilemap bounds
    startX = std::max(0, startX);
    startY = std::max(0, startY);
    endX = std::min(static_cast<int32_t>(tilemap->getWidth()), endX);
    endY = std::min(static_cast<int32_t>(tilemap->getHeight()), endY);

    // Reserve space
    uint32_t maxTiles = (endX - startX) * (endY - startY);
    m_impl->instanceDataIndexed.reserve(maxTiles);

    // Try to cast to TilemapEx for extended tile data
    const TilemapEx* tilemapEx = nullptr;
    // Try to cast to TilemapEx (note: Tilemap needs to be polymorphic for this to work)
    // For now, we'll just use the base Tilemap API

    // Build instances for visible tiles
    for (int32_t y = startY; y < endY; y++) {
        for (int32_t x = startX; x < endX; x++) {
            // Get tile data (use TileDataEx if available, otherwise convert from TileData)
            TileDataEx tileData;
            if (tilemapEx) {
                tileData = tilemapEx->getTileEx(x, y);
            } else {
                // Convert legacy TileData to TileDataEx
                TileData legacyTile = tilemap->getTile(x, y);
                if (legacyTile.getTileID() == 0) {
                    continue; // Empty tile
                }
                tileData = TileDataEx::fromTileData(legacyTile);
            }

            if (tileData.isEmpty()) {
                continue;
            }

            TileInstanceIndexed instance;

            // World position (relative to camera)
            instance.position[0] = x * tileWidth - camX;
            instance.position[1] = y * tileHeight - camY;

            // Texture coordinates (will be set by tileset)
            // For now, use simple grid layout
            uint32_t tileId = tileData.getTileID();
            uint32_t tilesPerRow = 16; // Assume 16 tiles per row
            uint32_t tileX = tileId % tilesPerRow;
            uint32_t tileY = tileId / tilesPerRow;

            float uvWidth = 1.0f / tilesPerRow;
            float uvHeight = uvWidth; // Assume square atlas

            instance.texCoordBase[0] = tileX * uvWidth;
            instance.texCoordBase[1] = tileY * uvHeight;
            instance.texCoordSize[0] = uvWidth;
            instance.texCoordSize[1] = uvHeight;

            // Tint color (white = no tint)
            instance.tintColor[0] = 1.0f;
            instance.tintColor[1] = 1.0f;
            instance.tintColor[2] = 1.0f;
            instance.tintColor[3] = 1.0f;

            // Flags (flip, rotate)
            instance.flags = 0;
            if (tileData.getFlipX()) instance.flags |= 0x02;
            if (tileData.getFlipY()) instance.flags |= 0x04;

            // Palette index
            instance.paletteIndex = tileData.getPaletteIndex();

            m_impl->instanceDataIndexed.push_back(instance);
        }
    }

    m_impl->stats.tilesRendered += static_cast<uint32_t>(m_impl->instanceDataIndexed.size());
    m_impl->stats.tilesCulled += maxTiles - static_cast<uint32_t>(m_impl->instanceDataIndexed.size());
}

bool TilemapRenderer::renderLayerIndexed(const TilemapLayer& layer,
                                         const TilesetIndexed& tileset,
                                         const PaletteBank& paletteBank,
                                         const Camera& camera,
                                         float time) {
    if (!m_impl->currentEncoder || !validateLayer(layer)) {
        return false;
    }

    // Check if indexed pipeline is ready
    if (!m_impl->pipelineStateIndexed) {
        createPipelineStateIndexed();
        if (!m_impl->pipelineStateIndexed) {
            NSLog(@"[TilemapRenderer] ERROR: Indexed pipeline not available");
            return false;
        }
    }

    // Check visibility
    if (!layer.isVisible() || layer.getOpacity() < 0.01f) {
        return true;
    }

    // Build instance buffer
    buildInstanceBufferIndexed(layer, camera, time);

    if (m_impl->instanceDataIndexed.empty()) {
        return true; // No visible tiles
    }

    // Update uniforms
    updateUniforms(camera, layer);
    m_impl->uniforms.time = time;
    m_impl->uniforms.opacity = layer.getOpacity();
    memcpy([m_impl->uniformBuffer contents], &m_impl->uniforms, sizeof(TilemapUniforms));

    // Get or cache textures
    id<MTLTexture> indexTexture = nil;
    auto it = m_impl->tilesetIndexedCache.find(&tileset);
    if (it != m_impl->tilesetIndexedCache.end()) {
        indexTexture = it->second;
    } else {
        indexTexture = tileset.getTexture();
        if (indexTexture) {
            m_impl->tilesetIndexedCache[&tileset] = indexTexture;
        }
    }

    id<MTLTexture> paletteTexture = paletteBank.getTexture();

    if (!indexTexture || !paletteTexture) {
        NSLog(@"[TilemapRenderer] ERROR: Missing textures for indexed rendering");
        return false;
    }

    // Set pipeline state
    id<MTLRenderPipelineState> pipeline = m_impl->config.enableDebugOverlay ?
        m_impl->debugPipelineStateIndexed : m_impl->pipelineStateIndexed;

    [m_impl->currentEncoder setRenderPipelineState:pipeline];

    // Bind uniforms
    [m_impl->currentEncoder setVertexBuffer:m_impl->uniformBuffer offset:0 atIndex:0];
    [m_impl->currentEncoder setFragmentBuffer:m_impl->uniformBuffer offset:0 atIndex:0];

    // Bind instance buffer
    size_t instanceBufferSize = m_impl->instanceDataIndexed.size() * sizeof(TileInstanceIndexed);
    id<MTLBuffer> instanceBuffer = [m_impl->device newBufferWithBytes:m_impl->instanceDataIndexed.data()
                                                                length:instanceBufferSize
                                                               options:MTLResourceStorageModeShared];
    [m_impl->currentEncoder setVertexBuffer:instanceBuffer offset:0 atIndex:1];

    // Bind textures
    [m_impl->currentEncoder setFragmentTexture:indexTexture atIndex:0];
    [m_impl->currentEncoder setFragmentTexture:paletteTexture atIndex:1];

    // Bind samplers
    [m_impl->currentEncoder setFragmentSamplerState:m_impl->samplerStateIndexed atIndex:0];

    // Create and bind quad vertex buffer (4 vertices for instanced quads)
    struct QuadVertex {
        float position[2];
        float texCoord[2];
    };

    QuadVertex quadVertices[4] = {
        {{0.0f, 0.0f}, {0.0f, 0.0f}},  // Top-left
        {{1.0f, 0.0f}, {1.0f, 0.0f}},  // Top-right
        {{0.0f, 1.0f}, {0.0f, 1.0f}},  // Bottom-left
        {{1.0f, 1.0f}, {1.0f, 1.0f}}   // Bottom-right
    };

    id<MTLBuffer> quadBuffer = [m_impl->device newBufferWithBytes:quadVertices
                                                            length:sizeof(quadVertices)
                                                           options:MTLResourceStorageModeShared];
    [m_impl->currentEncoder setVertexBuffer:quadBuffer offset:0 atIndex:0];

    // Draw instanced quads (triangle strip)
    [m_impl->currentEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
                               vertexStart:0
                               vertexCount:4
                             instanceCount:m_impl->instanceDataIndexed.size()];

    m_impl->stats.drawCalls++;
    m_impl->stats.layersRendered++;

    NSLog(@"[TilemapRenderer] Rendered indexed layer: %zu tiles", m_impl->instanceDataIndexed.size());

    return true;
}

} // namespace SuperTerminal
