//
// MetalRenderer.mm
// SuperTerminal v2
//
// Metal-based text and graphics renderer implementation.
// Most metal shaders live here

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <AppKit/AppKit.h>
#import <simd/simd.h>
#include "MetalRenderer.h"
#include "../Display/TextGrid.h"
#include "../Display/GraphicsLayer.h"
#include "../Display/SpriteManager.h"
#include "../Display/RectangleManager.h"
#include "../Display/CircleManager.h"
#include "../Display/LineManager.h"
#include "../Display/PolygonManager.h"
#include "../Display/StarManager.h"
#include "FontAtlas.h"
#include "../Display/TextDisplayManager.h"
#include "../Display/LoResPaletteManager.h"
#include "../Display/XResPaletteManager.h"
#include "../Display/WResPaletteManager.h"
#include "../Display/PResPaletteManager.h"
#include "../Display/DisplayManager.h"
#include "../Display/LoResBuffer.h"
#include "../Display/UResBuffer.h"
#include "../Display/XResBuffer.h"
#include "../Display/WResBuffer.h"
#include "../Display/PResBuffer.h"
#include "../API/st_api_context.h"
#include "../Debug/Logger.h"
#include "../Particles/ParticleSystem.h"
#include "../Tilemap/TilemapManager.h"
#include "../Tilemap/TilemapRenderer.h"
#include "../Tilemap/TilemapLayer.h"
#include "../Tilemap/Camera.h"
#include <vector>
#include <cmath>

// Forward declare GraphicsRenderer
namespace SuperTerminal {
    class GraphicsRenderer;
}

// Include the implementation
#include "../Display/GraphicsRenderer.mm"

namespace SuperTerminal {

// Pimpl implementation to hide Objective-C++ details
struct MetalRenderer::Impl {
    id<MTLDevice> device;
    id<MTLCommandQueue> commandQueue;
    id<MTLRenderPipelineState> pipelineState;
    id<MTLRenderPipelineState> sextantPipelineState;
    id<MTLRenderPipelineState> primitivesPipelineState;
    id<MTLRenderPipelineState> particlePipelineState;
    id<MTLRenderPipelineState> transformedTextPipelineState;
    id<MTLRenderPipelineState> textEffectsPipelineState;
    id<MTLBuffer> vertexBuffer;
    id<MTLBuffer> sextantVertexBuffer;
    id<MTLBuffer> uniformBuffer;
    id<MTLBuffer> particleInstanceBuffer;
    id<MTLBuffer> loresPaletteBuffer;
    id<MTLTexture> loresTexture;  // Legacy single texture (kept for compatibility)
    id<MTLTexture> loresTextures[8];  // [0]=front, [1]=back, [2-7]=scratch/atlas (for GPU blitter)
    id<MTLRenderPipelineState> loresPipelineState;
    id<MTLTexture> uresTexture;
    id<MTLRenderPipelineState> uresPipelineState;

    // URES: 8 GPU-resident textures (for GPU blitter support)
    id<MTLTexture> uresTextures[8];  // [0]=front, [1]=back, [2-7]=scratch/atlas

    // XRES: 8 GPU-resident textures (Amiga-style blitter support)
    id<MTLTexture> xresTextures[8];  // [0]=front, [1]=back, [2-7]=scratch/atlas
    id<MTLBuffer> xresPaletteBuffer;
    id<MTLRenderPipelineState> xresPipelineState;

    // WRES: 8 GPU-resident textures (Amiga-style blitter support)
    id<MTLTexture> wresTextures[8];  // [0]=front, [1]=back, [2-7]=scratch/atlas
    id<MTLBuffer> wresPaletteBuffer;
    id<MTLRenderPipelineState> wresPipelineState;

    // PRES: 8 GPU-resident textures (Premium Resolution 1280×720, 256-color palette)
    id<MTLTexture> presTextures[8];  // [0]=front, [1]=back, [2-7]=scratch/atlas
    id<MTLBuffer> presPaletteBuffer;
    id<MTLRenderPipelineState> presPipelineState;

    // GPU Blitter resources
    id<MTLComputePipelineState> blitterCopyPipeline;
    id<MTLComputePipelineState> blitterTransparentPipeline;
    id<MTLComputePipelineState> blitterMintermPipeline;
    id<MTLComputePipelineState> blitterClearPipeline;
    id<MTLSamplerState> samplerState;

    // GPU Primitive resources
    id<MTLComputePipelineState> primitiveRectPipeline;
    id<MTLComputePipelineState> primitiveCirclePipeline;
    id<MTLComputePipelineState> primitiveLinePipeline;
    id<MTLComputePipelineState> primitiveCircleAAPipeline;
    id<MTLComputePipelineState> primitiveLineAAPipeline;

    // URES GPU Primitive resources
    id<MTLComputePipelineState> uresPrimitiveRectPipeline;
    id<MTLComputePipelineState> uresPrimitiveCirclePipeline;
    id<MTLComputePipelineState> uresPrimitiveLinePipeline;
    id<MTLComputePipelineState> uresPrimitiveCircleAAPipeline;
    id<MTLComputePipelineState> uresPrimitiveLineAAPipeline;

    // URES GPU Gradient Primitive resources
    id<MTLComputePipelineState> uresPrimitiveRectGradientPipeline;
    id<MTLComputePipelineState> uresPrimitiveCircleGradientPipeline;
    id<MTLComputePipelineState> uresPrimitiveCircleGradientAAPipeline;

    // URES GPU Blitter resources
    id<MTLComputePipelineState> uresBlitterCopyPipeline;
    id<MTLComputePipelineState> uresBlitterTransparentPipeline;
    id<MTLComputePipelineState> uresBlitterAlphaCompositePipeline;
    id<MTLComputePipelineState> uresBlitterClearPipeline;

    // GPU Blitter batching state
    id<MTLCommandBuffer> batchCommandBuffer;
    id<MTLComputeCommandEncoder> batchEncoder;
    bool inBlitBatch;
    CAMetalLayer* metalLayer;

    // Frame synchronization
    dispatch_semaphore_t inflightSemaphore;
    static constexpr int MAX_FRAMES_IN_FLIGHT = 3;

    std::shared_ptr<FontAtlas> fontAtlas;
    std::shared_ptr<TextDisplayManager> textDisplayManager;
    std::unique_ptr<GraphicsRenderer> graphicsRenderer;
    RenderConfig config;

    uint32_t viewportWidth;
    uint32_t viewportHeight;
    uint32_t cellWidth;
    uint32_t cellHeight;

    std::vector<TextVertex> vertices;
    std::vector<TextVertex> sextantVertices;
    TextUniforms uniforms;

    float elapsedTime;
    float cursorPhase;

    bool initialized;

    Impl()
        : device(nil)
        , commandQueue(nil)
        , pipelineState(nil)
        , sextantPipelineState(nil)
        , primitivesPipelineState(nil)
        , particlePipelineState(nil)
        , transformedTextPipelineState(nil)
        , textEffectsPipelineState(nil)
        , vertexBuffer(nil)
        , sextantVertexBuffer(nil)
        , uniformBuffer(nil)
        , particleInstanceBuffer(nil)
        , loresPaletteBuffer(nil)
        , loresTexture(nil)
        , loresPipelineState(nil)
        , uresTexture(nil)
        , uresPipelineState(nil)
        , xresPaletteBuffer(nil)
        , xresPipelineState(nil)
        , wresPaletteBuffer(nil)
        , wresPipelineState(nil)
        , blitterCopyPipeline(nil)
        , blitterTransparentPipeline(nil)
        , blitterMintermPipeline(nil)
        , blitterClearPipeline(nil)
        , samplerState(nil)
        , primitiveRectPipeline(nil)
        , primitiveCirclePipeline(nil)
        , primitiveLinePipeline(nil)
        , primitiveCircleAAPipeline(nil)
        , primitiveLineAAPipeline(nil)
        , uresPrimitiveRectPipeline(nil)
        , uresPrimitiveCirclePipeline(nil)
        , uresPrimitiveLinePipeline(nil)
        , uresPrimitiveCircleAAPipeline(nil)
        , uresPrimitiveLineAAPipeline(nil)
        , uresPrimitiveRectGradientPipeline(nil)
        , uresPrimitiveCircleGradientPipeline(nil)
        , uresPrimitiveCircleGradientAAPipeline(nil)
        , uresBlitterCopyPipeline(nil)
        , uresBlitterTransparentPipeline(nil)
        , uresBlitterAlphaCompositePipeline(nil)
        , uresBlitterClearPipeline(nil)
        , batchCommandBuffer(nil)
        , batchEncoder(nil)
        , inBlitBatch(false)
        , metalLayer(nil)
        , viewportWidth(800)
        , viewportHeight(600)
        , cellWidth(8)
        , cellHeight(16)
        , elapsedTime(0.0f)
        , cursorPhase(0.0f)
        , initialized(false)
    {
        std::memset(&uniforms, 0, sizeof(uniforms));
    }

    ~Impl() {
        // ARC will handle cleanup
        device = nil;
        commandQueue = nil;
        pipelineState = nil;
        primitivesPipelineState = nil;
        vertexBuffer = nil;
        uniformBuffer = nil;
        samplerState = nil;
        metalLayer = nil;
        graphicsRenderer.reset();
    }
};

MetalRenderer::MetalRenderer(MTLDevicePtr device)
    : m_impl(std::make_unique<Impl>())
{
    if (device) {
        m_impl->device = device;
    } else {
        m_impl->device = MTLCreateSystemDefaultDevice();
    }

    if (m_impl->device) {
        m_impl->commandQueue = [m_impl->device newCommandQueue];
    }

    // Initialize frame synchronization semaphore
    m_impl->inflightSemaphore = dispatch_semaphore_create(Impl::MAX_FRAMES_IN_FLIGHT);
}

MetalRenderer::~MetalRenderer() {
    // Release semaphore if it exists
    if (m_impl && m_impl->inflightSemaphore) {
        // Wait for all frames to complete before destroying
        for (int i = 0; i < Impl::MAX_FRAMES_IN_FLIGHT; i++) {
            dispatch_semaphore_wait(m_impl->inflightSemaphore, DISPATCH_TIME_FOREVER);
        }
        // Release will happen automatically with ARC
    }
}

MetalRenderer::MetalRenderer(MetalRenderer&&) noexcept = default;
MetalRenderer& MetalRenderer::operator=(MetalRenderer&&) noexcept = default;

bool MetalRenderer::initialize(CAMetalLayer* layer, uint32_t width, uint32_t height) {
    if (!m_impl->device || !m_impl->commandQueue) {
        return false;
    }

    m_impl->metalLayer = layer;
    m_impl->viewportWidth = width;
    m_impl->viewportHeight = height;

    // Configure metal layer
    m_impl->metalLayer = layer;
    m_impl->metalLayer.device = m_impl->device;
    m_impl->metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    m_impl->metalLayer.framebufferOnly = YES;

    // Create graphics renderer at fixed 1920x1080 resolution
    // This provides consistent coordinate space and saves memory
    const uint32_t GRAPHICS_WIDTH = 1920;
    const uint32_t GRAPHICS_HEIGHT = 1080;
    CGFloat contentScale = 1.0;  // Fixed scale for consistent rendering
    NSLog(@"[MetalRenderer] Creating GraphicsRenderer at fixed %dx%d resolution", GRAPHICS_WIDTH, GRAPHICS_HEIGHT);
    m_impl->graphicsRenderer = std::make_unique<GraphicsRenderer>(m_impl->device, GRAPHICS_WIDTH, GRAPHICS_HEIGHT, contentScale);

    // Create render pipeline
    NSError* error = nil;

    // Load shader library - compile from source
    NSString* shaderSource = @R"(
        #include <metal_stdlib>
        using namespace metal;

        struct TextVertex {
            float2 position;
            float2 texCoord;
            float4 color;
        };

        struct VertexOut {
            float4 position [[position]];
            float2 texCoord;
            float4 color;
        };

        struct Uniforms {
            float4x4 projectionMatrix;
            float2 viewportSize;
            float2 cellSize;
            float time;
            float cursorBlink;
            uint renderMode;
            uint loresHeight;  // LORES height in pixels (75, 150, or 300)
        };

        vertex VertexOut text_vertex(
            uint vertexID [[vertex_id]],
            const device TextVertex* vertices [[buffer(0)]],
            constant Uniforms& uniforms [[buffer(1)]]
        ) {
            VertexOut out;
            TextVertex vert = vertices[vertexID];

            // Apply projection
            float4 projected = uniforms.projectionMatrix * float4(vert.position, 0.0, 1.0);
            out.position = projected;
            out.texCoord = vert.texCoord;
            out.color = vert.color;

            return out;
        }

        // Transformed text vertex shader - uses fixed 1920x1080 coordinate system
        vertex VertexOut transformedText_vertex(
            uint vertexID [[vertex_id]],
            const device TextVertex* vertices [[buffer(0)]],
            constant Uniforms& uniforms [[buffer(1)]]
        ) {
            VertexOut out;
            TextVertex vert = vertices[vertexID];

            // Convert from fixed 1920x1080 coordinate system to actual viewport coordinates
            // This ensures text appears at the same logical position regardless of window size
            float2 fixedResolution = float2(1920.0, 1080.0);
            float2 scaledPosition = vert.position * (uniforms.viewportSize / fixedResolution);

            // Apply projection with scaled coordinates
            float4 projected = uniforms.projectionMatrix * float4(scaledPosition, 0.0, 1.0);
            out.position = projected;
            out.texCoord = vert.texCoord;
            out.color = vert.color;

            return out;
        }

        fragment float4 text_fragment(
            VertexOut in [[stage_in]],
            texture2d<float> fontTexture [[texture(0)]],
            sampler fontSampler [[sampler(0)]]
        ) {
            // If texcoord is negative, this is a background quad - no texture sampling
            if (in.texCoord.x < 0.0) {
                return in.color;
            }

            // Otherwise, sample the font texture for text
            float alpha = fontTexture.sample(fontSampler, in.texCoord).r;
            return float4(in.color.rgb, in.color.a * alpha);
        }

        // LORES palette structure (300 rows × 16 colors - supports LORES/MIDRES/HIRES)
        struct LoResPalette300 {
            float4 colors[300][16];
        };

        // Sextant rendering shader with per-row palette support
        // Uses 75-palette structure but maps 32 text rows to first 32 palette rows
        fragment float4 sextant_fragment(
            VertexOut in [[stage_in]],
            texture2d<float> fontTexture [[texture(0)]],
            sampler fontSampler [[sampler(0)]],
            constant LoResPalette300& palette [[buffer(2)]]
        ) {
            // Calculate which text row we're in
            // Each cell is cellHeight pixels tall (16 pixels by default)
            // Position comes from in.position which is in pixel coordinates
            float cellHeight = 16.0;  // TODO: Pass this as uniform if needed
            int text_row = int(in.position.y / cellHeight);
            text_row = clamp(text_row, 0, 31);

            // The color in the vertex already contains the palette index (0-15)
            // Extract the color index from the red channel (we packed it there)
            int color_index = int(in.color.r * 15.0 + 0.5);
            color_index = clamp(color_index, 0, 15);

            // Look up color from per-row palette (using first 32 rows for sextant mode)
            float4 color = palette.colors[text_row][color_index];

            return color;
        }

        // Graphics layer compositing structures (used by LORES and graphics shaders)
        struct GraphicsVertex {
            float2 position;
            float2 texCoord;
        };

        struct GraphicsVertexOut {
            float4 position [[position]];
            float2 texCoord;
        };

        // LORES vertex shader for fullscreen quad
        vertex GraphicsVertexOut lores_vertex(
            uint vertexID [[vertex_id]],
            const device GraphicsVertex* vertices [[buffer(0)]]
        ) {
            GraphicsVertexOut out;
            GraphicsVertex vert = vertices[vertexID];
            out.position = float4(vert.position, 0.0, 1.0);
            out.texCoord = vert.texCoord;
            return out;
        }

        // LORES fragment shader with per-pixel-row palette lookup (dynamic resolution)
        // Now supports per-pixel alpha: lower 4 bits = color index, upper 4 bits = alpha (0-15)
        // Color index 0 is treated as transparent to allow underlying layers to show through
        fragment float4 lores_fragment(
            GraphicsVertexOut in [[stage_in]],
            texture2d<uint, access::sample> loresIndexTexture [[texture(0)]],
            constant LoResPalette300& palette [[buffer(2)]],
            constant Uniforms& uniforms [[buffer(3)]]
        ) {
            // Sample pixel data (8 bits: upper 4 = alpha, lower 4 = color index)
            // Use nearest sampling to avoid interpolation
            constexpr sampler nearestSampler(coord::normalized, filter::nearest);
            uint pixelData = loresIndexTexture.sample(nearestSampler, in.texCoord).r;

            // Extract color index (lower 4 bits)
            uint colorIndex = pixelData & 0x0Fu;
            colorIndex = clamp(colorIndex, 0u, 15u);

            // Color index 0 is transparent - allows tilemap and other layers to show through
            if (colorIndex == 0u) {
                return float4(0.0, 0.0, 0.0, 0.0);
            }

            // Extract alpha (upper 4 bits), scale to 0.0-1.0
            uint alphaValue = (pixelData >> 4u) & 0x0Fu;
            float alpha = float(alphaValue) / 15.0;

            // Calculate pixel row dynamically based on current resolution
            // in.texCoord.y ranges from 0 (top) to 1 (bottom)
            int pixelRow = int(in.texCoord.y * float(uniforms.loresHeight));
            pixelRow = clamp(pixelRow, 0, int(uniforms.loresHeight) - 1);

            // Look up color from per-row palette
            float4 color = palette.colors[pixelRow][colorIndex];

            // Apply per-pixel alpha
            color.a *= alpha;

            return color;
        }

        // URES vertex shader for fullscreen quad (same as LORES)
        vertex GraphicsVertexOut ures_vertex(
            uint vertexID [[vertex_id]],
            const device GraphicsVertex* vertices [[buffer(0)]]
        ) {
            GraphicsVertexOut out;
            GraphicsVertex vert = vertices[vertexID];
            out.position = float4(vert.position, 0.0, 1.0);
            out.texCoord = vert.texCoord;
            return out;
        }

        // URES fragment shader - direct ARGB4444 color rendering (no palette)
        // Pixel format: 16-bit ARGB4444
        // Bits [15-12]: Alpha (0-15)
        // Bits [11-8]:  Red (0-15)
        // Bits [7-4]:   Green (0-15)
        // Bits [3-0]:   Blue (0-15)
        fragment float4 ures_fragment(
            GraphicsVertexOut in [[stage_in]],
            texture2d<uint, access::sample> uresTexture [[texture(0)]]
        ) {
            // Sample 16-bit pixel data
            // Use nearest sampling to avoid interpolation
            constexpr sampler nearestSampler(coord::normalized, filter::nearest);
            uint pixelData = uresTexture.sample(nearestSampler, in.texCoord).r;

            // Extract ARGB4444 components (4 bits each)
            uint a = (pixelData >> 12u) & 0x0Fu;
            uint r = (pixelData >> 8u) & 0x0Fu;
            uint g = (pixelData >> 4u) & 0x0Fu;
            uint b = pixelData & 0x0Fu;

            // Convert 4-bit (0-15) to normalized float (0.0-1.0)
            float4 color;
            color.r = float(r) / 15.0;
            color.g = float(g) / 15.0;
            color.b = float(b) / 15.0;
            color.a = float(a) / 15.0;

            return color;
        }

        // XRES palette structure (hybrid: per-row 0-15 + global 16-255)
        struct XResPalette {
            float4 perRowColors[240][16];   // 240 rows × 16 colors for indices 0-15
            float4 globalColors[240];        // 240 global colors for indices 16-255
        };

        // WRES palette structure (hybrid: per-row 0-15 + global 16-255, same as XRES but 432 width)
        struct WResPalette {
            float4 perRowColors[240][16];   // 240 rows × 16 colors for indices 0-15
            float4 globalColors[240];        // 240 global colors for indices 16-255
        };

        // XRES vertex shader for fullscreen quad (same as LORES/URES)
        vertex GraphicsVertexOut xres_vertex(
            uint vertexID [[vertex_id]],
            const device GraphicsVertex* vertices [[buffer(0)]]
        ) {
            GraphicsVertexOut out;
            GraphicsVertex vert = vertices[vertexID];
            out.position = float4(vert.position, 0.0, 1.0);
            out.texCoord = vert.texCoord;
            return out;
        }

        // XRES fragment shader with hybrid palette (per-row 0-15, global 16-255)
        // Resolution: 320×240, 8-bit palette indices (0-255)
        // Color index 0 is treated as transparent to allow underlying layers to show through
        fragment float4 xres_fragment(
            GraphicsVertexOut in [[stage_in]],
            texture2d<uint, access::sample> xresIndexTexture [[texture(0)]],
            constant XResPalette& palette [[buffer(2)]]
        ) {
            // Sample 8-bit palette index
            // Use nearest sampling to avoid interpolation
            constexpr sampler nearestSampler(coord::normalized, filter::nearest);
            uint colorIndex = xresIndexTexture.sample(nearestSampler, in.texCoord).r;

            // Clamp to valid range
            colorIndex = clamp(colorIndex, 0u, 255u);

            // Color index 0 is transparent - allows tilemap and other layers to show through
            if (colorIndex == 0u) {
                return float4(0.0, 0.0, 0.0, 0.0);
            }

            // Calculate pixel row (0-239)
            int pixelRow = int(in.texCoord.y * 240.0);
            pixelRow = clamp(pixelRow, 0, 239);

            float4 color;

            // Hybrid palette lookup
            if (colorIndex < 16u) {
                // Per-row palette for indices 0-15
                color = palette.perRowColors[pixelRow][colorIndex];
            } else {
                // Global palette for indices 16-255
                uint globalIndex = colorIndex - 16u;
                globalIndex = clamp(globalIndex, 0u, 239u);
                color = palette.globalColors[globalIndex];
            }

            return color;
        }

        // WRES vertex shader for fullscreen quad (same as XRES/LORES/URES)
        vertex GraphicsVertexOut wres_vertex(
            uint vertexID [[vertex_id]],
            const device GraphicsVertex* vertices [[buffer(0)]]
        ) {
            GraphicsVertexOut out;
            GraphicsVertex vert = vertices[vertexID];
            out.position = float4(vert.position, 0.0, 1.0);
            out.texCoord = vert.texCoord;
            return out;
        }

        // WRES fragment shader with hybrid palette (per-row 0-15, global 16-255)
        // Resolution: 432×240, 8-bit palette indices (0-255)
        // Color index 0 is treated as transparent to allow underlying layers to show through
        fragment float4 wres_fragment(
            GraphicsVertexOut in [[stage_in]],
            texture2d<uint, access::sample> wresIndexTexture [[texture(0)]],
            constant WResPalette& palette [[buffer(2)]]
        ) {
            // Sample 8-bit palette index
            // Use nearest sampling to avoid interpolation
            constexpr sampler nearestSampler(coord::normalized, filter::nearest);
            uint colorIndex = wresIndexTexture.sample(nearestSampler, in.texCoord).r;

            // Clamp to valid range
            colorIndex = clamp(colorIndex, 0u, 255u);

            // Color index 0 is transparent - allows tilemap and other layers to show through
            if (colorIndex == 0u) {
                return float4(0.0, 0.0, 0.0, 0.0);
            }

            // Calculate pixel row (0-239)
            int pixelRow = int(in.texCoord.y * 240.0);
            pixelRow = clamp(pixelRow, 0, 239);

            float4 color;

            // Hybrid palette lookup
            if (colorIndex < 16u) {
                // Per-row palette for indices 0-15
                color = palette.perRowColors[pixelRow][colorIndex];
            } else {
                // Global palette for indices 16-255
                uint globalIndex = colorIndex - 16u;
                globalIndex = clamp(globalIndex, 0u, 239u);
                color = palette.globalColors[globalIndex];
            }

            return color;
        }

        // PRES vertex shader for fullscreen quad (same as XRES/WRES/LORES/URES)
        vertex GraphicsVertexOut pres_vertex(
            uint vertexID [[vertex_id]],
            const device GraphicsVertex* vertices [[buffer(0)]]
        ) {
            GraphicsVertexOut out;
            GraphicsVertex vert = vertices[vertexID];
            out.position = float4(vert.position, 0.0, 1.0);
            out.texCoord = vert.texCoord;
            return out;
        }

        // PRES fragment shader with hybrid palette (per-row 0-15, global 16-255)
        // Resolution: 1280×720, 8-bit palette indices (0-255)
        // Color index 0 is treated as transparent to allow underlying layers to show through
        fragment float4 pres_fragment(
            GraphicsVertexOut in [[stage_in]],
            texture2d<uint, access::sample> presIndexTexture [[texture(0)]],
            constant float4* palette [[buffer(2)]]
        ) {
            // Sample 8-bit palette index
            constexpr sampler nearestSampler(coord::normalized, filter::nearest);
            uint colorIndex = presIndexTexture.sample(nearestSampler, in.texCoord).r;

            // Clamp to valid range
            colorIndex = clamp(colorIndex, 0u, 255u);

            // Color index 0 is transparent - allows tilemap and other layers to show through
            if (colorIndex == 0u) {
                return float4(0.0, 0.0, 0.0, 0.0);
            }

            // Calculate pixel row (0-719)
            int pixelRow = int(in.texCoord.y * 720.0);
            pixelRow = clamp(pixelRow, 0, 719);

            float4 color;

            // Hybrid palette lookup
            if (colorIndex < 16u) {
                // Per-row palette for indices 0-15
                // Layout: palette[row * 16 + index]
                int paletteIndex = pixelRow * 16 + int(colorIndex);
                color = palette[paletteIndex];
            } else {
                // Global palette for indices 16-255
                // Layout: palette[11520 + (index - 16)]
                // 11520 = 720 rows * 16 colors
                int paletteIndex = 11520 + int(colorIndex) - 16;
                color = palette[paletteIndex];
            }

            return color;
        }

        // Primitive rendering shaders
        struct PrimitiveVertex {
            float2 position;
            float4 color;
        };

        vertex VertexOut primitive_vertex(
            uint vertexID [[vertex_id]],
            const device PrimitiveVertex* vertices [[buffer(0)]],
            constant Uniforms& uniforms [[buffer(1)]]
        ) {
            VertexOut out;
            PrimitiveVertex vert = vertices[vertexID];

            float4 projected = uniforms.projectionMatrix * float4(vert.position, 0.0, 1.0);
            out.position = projected;
            out.color = vert.color;
            out.texCoord = float2(0.0, 0.0);

            return out;
        }

        fragment float4 primitive_fragment(
            VertexOut in [[stage_in]]
        ) {
            return in.color;
        }

        // Graphics layer compositing shaders (for RGBA texture)
        vertex GraphicsVertexOut graphics_vertex(
            uint vertexID [[vertex_id]],
            const device GraphicsVertex* vertices [[buffer(0)]]
        ) {
            GraphicsVertexOut out;
            GraphicsVertex vert = vertices[vertexID];

            out.position = float4(vert.position, 0.0, 1.0);
            out.texCoord = vert.texCoord;

            return out;
        }

        fragment float4 graphics_fragment(
            GraphicsVertexOut in [[stage_in]],
            texture2d<float> graphicsTexture [[texture(0)]],
            sampler textureSampler [[sampler(0)]]
        ) {
            // Sample the RGBA texture directly (not alpha-only like text shader)
            float4 color = graphicsTexture.sample(textureSampler, in.texCoord);
            return color;
        }

        // Particle system shaders - instanced quad rendering with sprite textures
        struct ParticleVertex {
            float2 position;
            float2 texCoord;
        };

        struct ParticleInstanceData {
            float2 position;
            float2 texCoordMin;
            float2 texCoordMax;
            float4 color;
            float scale;
            float rotation;
            float alpha;
        };

        struct ParticleVertexOut {
            float4 position [[position]];
            float2 texCoord;
            float4 color;
            float alpha;
        };

        vertex ParticleVertexOut particleVertexShader(
            const device ParticleVertex* vertices [[buffer(0)]],
            constant Uniforms& uniforms [[buffer(1)]],
            const device ParticleInstanceData* instances [[buffer(2)]],
            uint vertexID [[vertex_id]],
            uint instanceID [[instance_id]]
        ) {
            ParticleVertexOut out;
            ParticleVertex vert = vertices[vertexID];
            ParticleInstanceData instance = instances[instanceID];

            // Apply rotation
            float c = cos(instance.rotation);
            float s = sin(instance.rotation);
            float2 rotated = float2(
                vert.position.x * c - vert.position.y * s,
                vert.position.x * s + vert.position.y * c
            );

            // Apply scale and position
            float2 scaled = rotated * instance.scale;
            float2 worldPos = instance.position + scaled;

            // Transform to clip space
            out.position = uniforms.projectionMatrix * float4(worldPos, 0.0, 1.0);

            // Interpolate texture coordinates based on fragment position in sprite
            out.texCoord = mix(instance.texCoordMin, instance.texCoordMax, vert.texCoord);

            // Pass color and alpha
            out.color = instance.color;
            out.alpha = instance.alpha;

            return out;
        }

        fragment float4 particleFragmentShader(
            ParticleVertexOut in [[stage_in]],
            texture2d<float> spriteTexture [[texture(0)]],
            sampler spriteSampler [[sampler(0)]]
        ) {
            // Sample the sprite texture at the fragment's texture coordinates
            float4 texColor = spriteTexture.sample(spriteSampler, in.texCoord);

            // Apply particle color tint and alpha
            float4 finalColor = texColor * in.color;
            finalColor.a *= in.alpha;

            // Discard fully transparent fragments
            if (finalColor.a < 0.01) {
                discard_fragment();
            }

            return finalColor;
        }

        // =====================================
        // TEXT EFFECTS SHADERS FOR DISPLAYTEXT
        // =====================================

        struct TextEffectVertex {
            float2 position;
            float2 texCoord;
            float4 color;
            float4 effectColor;
            float effectIntensity;
            float effectSize;
            float animationTime;
            uint effectType;
        };

        struct TextEffectVertexOut {
            float4 position [[position]];
            float2 texCoord;
            float4 color;
            float4 effectColor;
            float effectIntensity;
            float effectSize;
            float animationTime;
            uint effectType;
            float2 screenPosition;
        };

        vertex TextEffectVertexOut textEffect_vertex(
            uint vertexID [[vertex_id]],
            const device TextEffectVertex* vertices [[buffer(0)]],
            constant Uniforms& uniforms [[buffer(1)]]
        ) {
            TextEffectVertexOut out;
            TextEffectVertex vert = vertices[vertexID];

            // Convert from fixed 1920x1080 coordinate system to actual viewport coordinates
            // This ensures text appears at the same logical position regardless of window size
            float2 fixedResolution = float2(1920.0, 1080.0);
            float2 scaledPosition = vert.position * (uniforms.viewportSize / fixedResolution);

            // Apply projection with scaled coordinates
            float4 projected = uniforms.projectionMatrix * float4(scaledPosition, 0.0, 1.0);
            out.position = projected;
            out.texCoord = vert.texCoord;
            out.color = vert.color;
            out.effectColor = vert.effectColor;
            out.effectIntensity = vert.effectIntensity;
            out.effectSize = vert.effectSize;
            out.animationTime = vert.animationTime;
            out.effectType = vert.effectType;
            out.screenPosition = scaledPosition / uniforms.viewportSize;

            return out;
        }

        // Helper functions for text effects
        float4 sampleWithOffset(texture2d<float> fontTexture, sampler fontSampler, float2 texCoord, float2 offset) {
            return fontTexture.sample(fontSampler, texCoord + offset);
        }

        float4 createOutline(texture2d<float> fontTexture, sampler fontSampler, float2 texCoord, float4 outlineColor, float outlineWidth) {
            float2 texelSize = 1.0 / float2(512.0, 512.0); // Assuming 512x512 atlas
            float outlineAlpha = 0.0;

            // Sample in 8 directions for outline
            for (int x = -1; x <= 1; x++) {
                for (int y = -1; y <= 1; y++) {
                    if (x == 0 && y == 0) continue;
                    float2 offset = float2(x, y) * texelSize * outlineWidth;
                    outlineAlpha = max(outlineAlpha, fontTexture.sample(fontSampler, texCoord + offset).r);
                }
            }

            return float4(outlineColor.rgb, outlineColor.a * outlineAlpha);
        }

        float4 createGlow(texture2d<float> fontTexture, sampler fontSampler, float2 texCoord, float4 glowColor, float glowSize, float intensity) {
            float2 texelSize = 1.0 / float2(512.0, 512.0);
            float glowAlpha = 0.0;
            float samples = 0.0;

            // Sample in a circle for glow
            for (int x = -3; x <= 3; x++) {
                for (int y = -3; y <= 3; y++) {
                    float distance = length(float2(x, y));
                    if (distance <= glowSize) {
                        float2 offset = float2(x, y) * texelSize;
                        float weight = 1.0 - (distance / glowSize);
                        glowAlpha += fontTexture.sample(fontSampler, texCoord + offset).r * weight;
                        samples += weight;
                    }
                }
            }

            if (samples > 0.0) {
                glowAlpha /= samples;
            }

            return float4(glowColor.rgb, glowColor.a * glowAlpha * intensity);
        }

        fragment float4 textEffect_fragment(
            TextEffectVertexOut in [[stage_in]],
            texture2d<float> fontTexture [[texture(0)]],
            sampler fontSampler [[sampler(0)]],
            constant Uniforms& uniforms [[buffer(1)]]
        ) {
            // If texcoord is negative, this is a background quad
            if (in.texCoord.x < 0.0) {
                return in.color;
            }

            // Sample the main text
            float textAlpha = fontTexture.sample(fontSampler, in.texCoord).r;
            float4 finalColor = float4(in.color.rgb, in.color.a * textAlpha);

            // Apply effects based on effect type
            switch (in.effectType) {
                case 1: { // DROP_SHADOW
                    float2 shadowOffset = float2(in.effectSize, in.effectSize) / float2(512.0, 512.0);
                    float shadowAlpha = fontTexture.sample(fontSampler, in.texCoord - shadowOffset).r;
                    float4 shadowColor = float4(in.effectColor.rgb, in.effectColor.a * shadowAlpha * in.effectIntensity);
                    // Composite: shadow behind text
                    finalColor = mix(shadowColor, finalColor, finalColor.a);
                    break;
                }
                case 2: { // OUTLINE
                    float4 outlineColor = createOutline(fontTexture, fontSampler, in.texCoord, in.effectColor, in.effectSize);
                    outlineColor.a *= in.effectIntensity;
                    // Composite: outline behind text
                    finalColor = mix(outlineColor, finalColor, finalColor.a);
                    break;
                }
                case 3: { // GLOW
                    float4 glowColor = createGlow(fontTexture, fontSampler, in.texCoord, in.effectColor, in.effectSize, in.effectIntensity);
                    // Additive blend for glow
                    finalColor.rgb += glowColor.rgb * glowColor.a;
                    break;
                }
                case 4: { // GRADIENT
                    // Simple vertical gradient effect
                    float gradientFactor = in.screenPosition.y;
                    float4 gradientColor = mix(in.effectColor, in.color, gradientFactor);
                    finalColor = float4(gradientColor.rgb, in.color.a * textAlpha);
                    break;
                }
                case 5: { // WAVE
                    // Animated wave distortion (color shift only for now)
                    float wave = sin(in.screenPosition.x * 10.0 + uniforms.time * 5.0) * 0.5 + 0.5;
                    float4 waveColor = mix(in.color, in.effectColor, wave * in.effectIntensity);
                    finalColor = float4(waveColor.rgb, in.color.a * textAlpha);
                    break;
                }
                case 6: { // NEON
                    // Neon glow effect - combines outline and glow
                    float4 outlineColor = createOutline(fontTexture, fontSampler, in.texCoord, in.effectColor, 1.0);
                    float4 glowColor = createGlow(fontTexture, fontSampler, in.texCoord, in.effectColor, in.effectSize, in.effectIntensity);

                    // Composite: glow + outline + text
                    finalColor.rgb += glowColor.rgb * glowColor.a;
                    finalColor = mix(outlineColor, finalColor, finalColor.a);
                    break;
                }
                default:
                    // No effect - just return normal text
                    break;
            }

            return finalColor;
        }

        // ============================================================================
        // GPU BLITTER COMPUTE SHADERS (Amiga-style GPU-to-GPU blits)
        // ============================================================================

        // Simple GPU-to-GPU copy blit
        // Copies a rectangular region from source to destination texture
        kernel void blitter_copy(
            texture2d<uint, access::read> source [[texture(0)]],
            texture2d<uint, access::write> destination [[texture(1)]],
            constant int4& sourceRect [[buffer(0)]],      // x, y, width, height
            constant int2& destPos [[buffer(1)]],         // x, y
            uint2 gid [[thread_position_in_grid]]
        ) {
            // Check bounds
            if (gid.x >= uint(sourceRect.z) || gid.y >= uint(sourceRect.w)) {
                return;
            }

            // Calculate source and destination coordinates
            uint2 srcCoord = uint2(sourceRect.x, sourceRect.y) + gid;
            uint2 dstCoord = uint2(destPos.x, destPos.y) + gid;

            // Read from source and write to destination
            uint pixel = source.read(srcCoord).r;
            destination.write(uint4(pixel, 0, 0, 0), dstCoord);
        }

        // Transparent blit: copy pixels, skipping a transparent color
        kernel void blitter_transparent(
            texture2d<uint, access::read> source [[texture(0)]],
            texture2d<uint, access::write> destination [[texture(1)]],
            constant int4& sourceRect [[buffer(0)]],      // x, y, width, height
            constant int2& destPos [[buffer(1)]],         // x, y
            constant uint& transparentColor [[buffer(2)]], // Color to skip (0-255)
            uint2 gid [[thread_position_in_grid]]
        ) {
            // Check bounds
            if (gid.x >= uint(sourceRect.z) || gid.y >= uint(sourceRect.w)) {
                return;
            }

            // Calculate source and destination coordinates
            uint2 srcCoord = uint2(sourceRect.x, sourceRect.y) + gid;
            uint2 dstCoord = uint2(destPos.x, destPos.y) + gid;

            // Read from source
            uint pixel = source.read(srcCoord).r;

            // Skip transparent pixels
            if (pixel == transparentColor) {
                return;
            }

            // Write to destination
            destination.write(uint4(pixel, 0, 0, 0), dstCoord);
        }

        // Minterm blit: Amiga-style 3-channel (A/B/C) logical operations
        // A = source, B = destination (read before write), C = mask
        // minterm is an 8-bit value specifying output for each ABC combination
        kernel void blitter_minterm(
            texture2d<uint, access::read> source [[texture(0)]],      // Channel A
            texture2d<uint, access::read_write> destination [[texture(1)]], // Channel B
            texture2d<uint, access::read> mask [[texture(2)]],        // Channel C (optional)
            constant int4& sourceRect [[buffer(0)]],      // x, y, width, height
            constant int2& destPos [[buffer(1)]],         // x, y
            constant uint& minterm [[buffer(2)]],         // 8-bit minterm value
            constant bool& useMask [[buffer(3)]],         // Enable mask channel
            uint2 gid [[thread_position_in_grid]]
        ) {
            // Check bounds
            if (gid.x >= uint(sourceRect.z) || gid.y >= uint(sourceRect.w)) {
                return;
            }

            // Calculate coordinates
            uint2 srcCoord = uint2(sourceRect.x, sourceRect.y) + gid;
            uint2 dstCoord = uint2(destPos.x, destPos.y) + gid;

            // Read channels
            uint A = source.read(srcCoord).r;
            uint B = destination.read(dstCoord).r;
            uint C = useMask ? mask.read(srcCoord).r : 255;

            // For indexed color, treat non-zero as 1, zero as 0 for logic
            bool a = (A != 0);
            bool b = (B != 0);
            bool c = (C != 0);

            // Build truth table index (bit 2=A, bit 1=B, bit 0=C)
            uint index = (a ? 4u : 0u) | (b ? 2u : 0u) | (c ? 1u : 0u);

            // Check minterm bit for this combination
            bool result = (minterm & (1u << index)) != 0;

            // Write result: if true, write A; if false, keep B
            if (result) {
                destination.write(uint4(A, 0, 0, 0), dstCoord);
            }
            // If false, B is already there (read_write), so do nothing
        }

        // GPU clear: fill entire texture with a single color
        // Fast GPU-resident clear without CPU involvement
        kernel void blitter_clear(
            texture2d<uint, access::write> destination [[texture(0)]],
            constant uint& clearColor [[buffer(0)]],     // Color value (0-255)
            uint2 gid [[thread_position_in_grid]]
        ) {
            // Write clear color to every pixel
            destination.write(uint4(clearColor, 0, 0, 0), gid);
        }

        // ============================================================================
        // GPU PRIMITIVE DRAWING OPERATIONS
        // ============================================================================

        // Fill a rectangle with a solid color
        kernel void primitive_rect_fill(
            texture2d<uint, access::write> destination [[texture(0)]],
            constant int4& rect [[buffer(0)]],           // x, y, width, height
            constant uint& fillColor [[buffer(1)]],      // Color index (0-255)
            uint2 gid [[thread_position_in_grid]]
        ) {
            // Check if thread is within rectangle bounds
            if (gid.x >= uint(rect.z) || gid.y >= uint(rect.w)) {
                return;
            }

            // Calculate absolute position
            uint2 pos = uint2(rect.x, rect.y) + gid;

            // Write color to destination
            destination.write(uint4(fillColor, 0, 0, 0), pos);
        }

        // Fill a circle with a solid color using distance test
        kernel void primitive_circle_fill(
            texture2d<uint, access::write> destination [[texture(0)]],
            constant int2& center [[buffer(0)]],         // cx, cy
            constant int& radius [[buffer(1)]],          // radius
            constant uint& fillColor [[buffer(2)]],      // Color index (0-255)
            uint2 gid [[thread_position_in_grid]]
        ) {
            // Calculate bounding box
            int radiusSq = radius * radius;

            // Check if we're within bounding box
            int dx = int(gid.x) - center.x;
            int dy = int(gid.y) - center.y;

            // Distance squared test (avoid sqrt for performance)
            int distSq = dx * dx + dy * dy;

            // If inside circle, draw pixel
            if (distSq <= radiusSq) {
                destination.write(uint4(fillColor, 0, 0, 0), gid);
            }
        }

        // Draw a line using parallel distance test
        // Each thread checks if it's on or near the line
        kernel void primitive_line(
            texture2d<uint, access::write> destination [[texture(0)]],
            constant int4& lineCoords [[buffer(0)]],     // x0, y0, x1, y1
            constant uint& lineColor [[buffer(1)]],      // Color index (0-255)
            uint2 gid [[thread_position_in_grid]]
        ) {
            int x0 = lineCoords.x;
            int y0 = lineCoords.y;
            int x1 = lineCoords.z;
            int y1 = lineCoords.w;

            int dx = abs(x1 - x0);
            int dy = abs(y1 - y0);

            // Check if thread position is within bounding box
            int minX = min(x0, x1);
            int maxX = max(x0, x1);
            int minY = min(y0, y1);
            int maxY = max(y0, y1);

            int px = int(gid.x);
            int py = int(gid.y);

            if (px < minX || px > maxX || py < minY || py > maxY) {
                return;
            }

            // Use implicit line equation: (y1-y0)*x - (x1-x0)*y + (x1-x0)*y0 - (y1-y0)*x0 = 0
            // Distance from point (px, py) to line
            int lineEq = (y1 - y0) * px - (x1 - x0) * py + (x1 - x0) * y0 - (y1 - y0) * x0;

            // Normalize by line length for consistent thickness
            // For 1-pixel width, check if distance is less than half the line length
            int lengthSq = dx * dx + dy * dy;

            // Simple threshold - adjust for thickness
            // For single-pixel lines, we want |lineEq| / length < 0.5
            // To avoid division: |lineEq| * |lineEq| < 0.25 * lengthSq
            int distSq = lineEq * lineEq;

            // Draw if close enough to line (using squared distance to avoid sqrt)
            // 0.25 = single pixel width, increase for thicker lines
            if (distSq * 4 <= lengthSq) {
                destination.write(uint4(lineColor, 0, 0, 0), gid);
            }
        }

        // ============================================================================
        // ANTI-ALIASED GPU PRIMITIVE DRAWING OPERATIONS
        // ============================================================================

        // Anti-aliased filled circle using smooth distance falloff
        // Blends edge pixels with background for smooth appearance
        kernel void primitive_circle_fill_aa(
            texture2d<uint, access::read_write> destination [[texture(0)]],
            constant int2& center [[buffer(0)]],         // cx, cy
            constant int& radius [[buffer(1)]],          // radius
            constant uint& fillColor [[buffer(2)]],      // Color index (0-255)
            uint2 gid [[thread_position_in_grid]]
        ) {
            int dx = int(gid.x) - center.x;
            int dy = int(gid.y) - center.y;

            // Calculate actual distance (use sqrt for smooth edges)
            float dist = sqrt(float(dx * dx + dy * dy));
            float r = float(radius);

            // Anti-aliasing: smooth transition over 1.5 pixels at edge
            float coverage = 1.0 - smoothstep(r - 0.75, r + 0.75, dist);

            if (coverage > 0.01) {
                // Read existing pixel
                uint bgColor = destination.read(gid).r;

                // If fully inside, just write fill color
                if (coverage > 0.99) {
                    destination.write(uint4(fillColor, 0, 0, 0), gid);
                } else {
                    // Edge pixel: blend fill color with background
                    // Simple dithering approach - use coverage to decide color
                    // For palette mode, we can't truly blend, so use threshold dithering

                    // Add sub-pixel jitter for better dithering
                    float jitter = fract(float(gid.x) * 0.5 + float(gid.y) * 0.3);

                    if (coverage > jitter) {
                        destination.write(uint4(fillColor, 0, 0, 0), gid);
                    }
                    // else keep background pixel
                }
            }
        }

        // Anti-aliased line drawing with smooth edges
        kernel void primitive_line_aa(
            texture2d<uint, access::read_write> destination [[texture(0)]],
            constant int4& lineCoords [[buffer(0)]],     // x0, y0, x1, y1
            constant uint& lineColor [[buffer(1)]],      // Color index (0-255)
            constant float& lineWidth [[buffer(2)]],     // Line width (default 1.0)
            uint2 gid [[thread_position_in_grid]]
        ) {
            float x0 = float(lineCoords.x);
            float y0 = float(lineCoords.y);
            float x1 = float(lineCoords.z);
            float y1 = float(lineCoords.w);

            float px = float(gid.x);
            float py = float(gid.y);

            // Vector from p0 to p1
            float dx = x1 - x0;
            float dy = y1 - y0;
            float lineLength = sqrt(dx * dx + dy * dy);

            if (lineLength < 0.001) {
                // Degenerate line (point)
                float dist = sqrt((px - x0) * (px - x0) + (py - y0) * (py - y0));
                float coverage = 1.0 - smoothstep(lineWidth * 0.5 - 0.5, lineWidth * 0.5 + 0.5, dist);

                if (coverage > 0.01) {
                    float jitter = fract(px * 0.5 + py * 0.3);
                    if (coverage > jitter) {
                        destination.write(uint4(lineColor, 0, 0, 0), gid);
                    }
                }
                return;
            }

            // Normalize direction vector
            dx /= lineLength;
            dy /= lineLength;

            // Vector from p0 to pixel
            float vx = px - x0;
            float vy = py - y0;

            // Project onto line to find closest point
            float t = vx * dx + vy * dy;
            t = clamp(t, 0.0f, lineLength);

            // Closest point on line
            float closestX = x0 + t * dx;
            float closestY = y0 + t * dy;

            // Distance from pixel to line
            float distX = px - closestX;
            float distY = py - closestY;
            float distance = sqrt(distX * distX + distY * distY);

            // Anti-aliasing: smooth falloff over 1 pixel
            float halfWidth = lineWidth * 0.5;
            float coverage = 1.0 - smoothstep(halfWidth - 0.5, halfWidth + 0.5, distance);

            if (coverage > 0.01) {
                // Read existing pixel for blending
                uint bgColor = destination.read(gid).r;

                if (coverage > 0.99) {
                    // Fully inside line
                    destination.write(uint4(lineColor, 0, 0, 0), gid);
                } else {
                    // Edge pixel: use dithering for smooth appearance
                    float jitter = fract(px * 0.5 + py * 0.3);

                    if (coverage > jitter) {
                        destination.write(uint4(lineColor, 0, 0, 0), gid);
                    }
                    // else keep background
                }
            }
        }

        // =============================================================================
        // URES GPU PRIMITIVES (1280×720 Direct Color ARGB4444)
        // =============================================================================
        // These kernels work with 16-bit ARGB4444 format and support TRUE alpha blending
        // Unlike palette-indexed modes (XRES/WRES), URES can blend colors directly

        // Helper function: Pack ARGB4444 from components
        inline ushort pack_argb4444(uint a, uint r, uint g, uint b) {
            return ushort(((a & 0xFu) << 12u) | ((r & 0xFu) << 8u) | ((g & 0xFu) << 4u) | (b & 0xFu));
        }

        // Helper function: Unpack ARGB4444 to components
        inline void unpack_argb4444(ushort color, thread uint& a, thread uint& r, thread uint& g, thread uint& b) {
            a = (color >> 12u) & 0xFu;
            r = (color >> 8u) & 0xFu;
            g = (color >> 4u) & 0xFu;
            b = color & 0xFu;
        }

        // Helper function: Alpha blend two ARGB4444 colors (true blending, not dithering!)
        inline ushort blend_argb4444(ushort src, ushort dst) {
            uint srcA, srcR, srcG, srcB;
            uint dstA, dstR, dstG, dstB;

            unpack_argb4444(src, srcA, srcR, srcG, srcB);
            unpack_argb4444(dst, dstA, dstR, dstG, dstB);

            // Standard alpha compositing: out = src.a * src + (1 - src.a) * dst
            float alpha = float(srcA) / 15.0f;
            float invAlpha = 1.0f - alpha;

            uint outR = uint(float(srcR) * alpha + float(dstR) * invAlpha + 0.5f);
            uint outG = uint(float(srcG) * alpha + float(dstG) * invAlpha + 0.5f);
            uint outB = uint(float(srcB) * alpha + float(dstB) * invAlpha + 0.5f);
            uint outA = max(srcA, dstA);  // Max alpha for coverage

            return pack_argb4444(outA, outR, outG, outB);
        }

        // URES: Fill a rectangle with solid color
        kernel void ures_primitive_rect_fill(
            texture2d<ushort, access::write> destination [[texture(0)]],
            constant int4& rect [[buffer(0)]],           // x, y, width, height
            constant ushort& fillColor [[buffer(1)]],    // ARGB4444 color
            uint2 gid [[thread_position_in_grid]]
        ) {
            int x = rect.x;
            int y = rect.y;
            int width = rect.z;
            int height = rect.w;

            int px = int(gid.x);
            int py = int(gid.y);

            // Check if pixel is inside rectangle
            if (px >= x && px < x + width && py >= y && py < y + height) {
                destination.write(ushort4(fillColor, 0, 0, 0), gid);
            }
        }

        // URES: Fill a circle with solid color
        kernel void ures_primitive_circle_fill(
            texture2d<ushort, access::write> destination [[texture(0)]],
            constant int2& center [[buffer(0)]],         // cx, cy
            constant float& radius [[buffer(1)]],
            constant ushort& fillColor [[buffer(2)]],    // ARGB4444 color
            uint2 gid [[thread_position_in_grid]]
        ) {
            float px = float(gid.x);
            float py = float(gid.y);
            float cx = float(center.x);
            float cy = float(center.y);

            float dx = px - cx;
            float dy = py - cy;
            float dist = sqrt(dx * dx + dy * dy);

            if (dist <= radius) {
                destination.write(ushort4(fillColor, 0, 0, 0), gid);
            }
        }

        // URES: Draw a line with solid color
        kernel void ures_primitive_line(
            texture2d<ushort, access::write> destination [[texture(0)]],
            constant int4& lineCoords [[buffer(0)]],     // x0, y0, x1, y1
            constant ushort& lineColor [[buffer(1)]],    // ARGB4444 color
            constant float& lineWidth [[buffer(2)]],     // Line width (default 1.0)
            uint2 gid [[thread_position_in_grid]]
        ) {
            float x0 = float(lineCoords.x);
            float y0 = float(lineCoords.y);
            float x1 = float(lineCoords.z);
            float y1 = float(lineCoords.w);

            float px = float(gid.x);
            float py = float(gid.y);

            float dx = x1 - x0;
            float dy = y1 - y0;
            float lineLength = sqrt(dx * dx + dy * dy);

            if (lineLength < 0.001) {
                float dist = sqrt((px - x0) * (px - x0) + (py - y0) * (py - y0));
                if (dist <= lineWidth * 0.5) {
                    destination.write(ushort4(lineColor, 0, 0, 0), gid);
                }
                return;
            }

            dx /= lineLength;
            dy /= lineLength;

            float vx = px - x0;
            float vy = py - y0;
            float t = vx * dx + vy * dy;
            t = clamp(t, 0.0f, lineLength);

            float closestX = x0 + t * dx;
            float closestY = y0 + t * dy;

            float distX = px - closestX;
            float distY = py - closestY;
            float distance = sqrt(distX * distX + distY * distY);

            if (distance <= lineWidth * 0.5) {
                destination.write(ushort4(lineColor, 0, 0, 0), gid);
            }
        }

        // URES: Anti-aliased filled circle with TRUE alpha blending
        // This is where URES shines - we can blend colors directly, not just dither!
        kernel void ures_primitive_circle_fill_aa(
            texture2d<ushort, access::read_write> destination [[texture(0)]],
            constant int2& center [[buffer(0)]],         // cx, cy
            constant float& radius [[buffer(1)]],
            constant ushort& fillColor [[buffer(2)]],    // ARGB4444 color
            uint2 gid [[thread_position_in_grid]]
        ) {
            float px = float(gid.x) + 0.5f;  // Sample at pixel center
            float py = float(gid.y) + 0.5f;
            float cx = float(center.x);
            float cy = float(center.y);

            float dx = px - cx;
            float dy = py - cy;
            float dist = sqrt(dx * dx + dy * dy);

            // Smooth falloff over 1 pixel at edge
            float coverage = 1.0f - smoothstep(radius - 0.7f, radius + 0.7f, dist);

            if (coverage > 0.01f) {
                if (coverage > 0.99f) {
                    // Fully inside circle
                    destination.write(ushort4(fillColor, 0, 0, 0), gid);
                } else {
                    // Edge pixel: TRUE alpha blending!
                    // Read background color
                    ushort bgColor = destination.read(gid).r;

                    // Unpack fill color and modulate alpha by coverage
                    uint fillA, fillR, fillG, fillB;
                    unpack_argb4444(fillColor, fillA, fillR, fillG, fillB);

                    // Scale alpha by coverage for smooth edge
                    uint edgeA = uint(float(fillA) * coverage + 0.5f);
                    ushort edgeColor = pack_argb4444(edgeA, fillR, fillG, fillB);

                    // Blend edge color with background
                    ushort blended = blend_argb4444(edgeColor, bgColor);
                    destination.write(ushort4(blended, 0, 0, 0), gid);
                }
            }
        }

        // URES: Anti-aliased line with TRUE alpha blending
        kernel void ures_primitive_line_aa(
            texture2d<ushort, access::read_write> destination [[texture(0)]],
            constant int4& lineCoords [[buffer(0)]],     // x0, y0, x1, y1
            constant ushort& lineColor [[buffer(1)]],    // ARGB4444 color
            constant float& lineWidth [[buffer(2)]],     // Line width (default 1.0)
            uint2 gid [[thread_position_in_grid]]
        ) {
            float x0 = float(lineCoords.x);
            float y0 = float(lineCoords.y);
            float x1 = float(lineCoords.z);
            float y1 = float(lineCoords.w);

            float px = float(gid.x) + 0.5f;
            float py = float(gid.y) + 0.5f;

            float dx = x1 - x0;
            float dy = y1 - y0;
            float lineLength = sqrt(dx * dx + dy * dy);

            if (lineLength < 0.001) {
                // Degenerate line (point)
                float dist = sqrt((px - x0) * (px - x0) + (py - y0) * (py - y0));
                float coverage = 1.0f - smoothstep(lineWidth * 0.5f - 0.7f, lineWidth * 0.5f + 0.7f, dist);

                if (coverage > 0.01f) {
                    if (coverage > 0.99f) {
                        destination.write(ushort4(lineColor, 0, 0, 0), gid);
                    } else {
                        ushort bgColor = destination.read(gid).r;
                        uint fillA, fillR, fillG, fillB;
                        unpack_argb4444(lineColor, fillA, fillR, fillG, fillB);
                        uint edgeA = uint(float(fillA) * coverage + 0.5f);
                        ushort edgeColor = pack_argb4444(edgeA, fillR, fillG, fillB);
                        ushort blended = blend_argb4444(edgeColor, bgColor);
                        destination.write(ushort4(blended, 0, 0, 0), gid);
                    }
                }
                return;
            }

            dx /= lineLength;
            dy /= lineLength;

            float vx = px - x0;
            float vy = py - y0;
            float t = vx * dx + vy * dy;
            t = clamp(t, 0.0f, lineLength);

            float closestX = x0 + t * dx;
            float closestY = y0 + t * dy;

            float distX = px - closestX;
            float distY = py - closestY;
            float distance = sqrt(distX * distX + distY * distY);

            float halfWidth = lineWidth * 0.5f;
            float coverage = 1.0f - smoothstep(halfWidth - 0.7f, halfWidth + 0.7f, distance);

            if (coverage > 0.01f) {
                if (coverage > 0.99f) {
                    // Fully inside line
                    destination.write(ushort4(lineColor, 0, 0, 0), gid);
                } else {
                    // Edge pixel: TRUE alpha blending!
                    ushort bgColor = destination.read(gid).r;

                    uint fillA, fillR, fillG, fillB;
                    unpack_argb4444(lineColor, fillA, fillR, fillG, fillB);
                    uint edgeA = uint(float(fillA) * coverage + 0.5f);
                    ushort edgeColor = pack_argb4444(edgeA, fillR, fillG, fillB);

                    ushort blended = blend_argb4444(edgeColor, bgColor);
                    destination.write(ushort4(blended, 0, 0, 0), gid);
                }
            }
        }

        // =============================================================================
        // URES GPU GRADIENT PRIMITIVES
        // =============================================================================

        // URES: Fill a rectangle with 4-corner gradient (bilinear interpolation)
        kernel void ures_primitive_rect_fill_gradient(
            texture2d<ushort, access::write> destination [[texture(0)]],
            constant int4& rect [[buffer(0)]],              // x, y, width, height
            constant ushort4& cornerColors [[buffer(1)]],   // topLeft, topRight, bottomLeft, bottomRight (ARGB4444)
            uint2 gid [[thread_position_in_grid]]
        ) {
            int x = rect.x;
            int y = rect.y;
            int width = rect.z;
            int height = rect.w;

            int px = int(gid.x);
            int py = int(gid.y);

            // Check if pixel is inside rectangle
            if (px >= x && px < x + width && py >= y && py < y + height) {
                // Calculate normalized position within rectangle (0.0 to 1.0)
                float tx = float(px - x) / float(width - 1);
                float ty = float(py - y) / float(height - 1);

                // Clamp to avoid edge cases
                tx = clamp(tx, 0.0f, 1.0f);
                ty = clamp(ty, 0.0f, 1.0f);

                // Unpack corner colors
                uint tl_a, tl_r, tl_g, tl_b;
                uint tr_a, tr_r, tr_g, tr_b;
                uint bl_a, bl_r, bl_g, bl_b;
                uint br_a, br_r, br_g, br_b;

                unpack_argb4444(cornerColors.x, tl_a, tl_r, tl_g, tl_b); // top-left
                unpack_argb4444(cornerColors.y, tr_a, tr_r, tr_g, tr_b); // top-right
                unpack_argb4444(cornerColors.z, bl_a, bl_r, bl_g, bl_b); // bottom-left
                unpack_argb4444(cornerColors.w, br_a, br_r, br_g, br_b); // bottom-right

                // Bilinear interpolation
                // Top edge: lerp between top-left and top-right
                float top_a = mix(float(tl_a), float(tr_a), tx);
                float top_r = mix(float(tl_r), float(tr_r), tx);
                float top_g = mix(float(tl_g), float(tr_g), tx);
                float top_b = mix(float(tl_b), float(tr_b), tx);

                // Bottom edge: lerp between bottom-left and bottom-right
                float bot_a = mix(float(bl_a), float(br_a), tx);
                float bot_r = mix(float(bl_r), float(br_r), tx);
                float bot_g = mix(float(bl_g), float(br_g), tx);
                float bot_b = mix(float(bl_b), float(br_b), tx);

                // Final: lerp between top and bottom
                uint final_a = uint(mix(top_a, bot_a, ty) + 0.5f);
                uint final_r = uint(mix(top_r, bot_r, ty) + 0.5f);
                uint final_g = uint(mix(top_g, bot_g, ty) + 0.5f);
                uint final_b = uint(mix(top_b, bot_b, ty) + 0.5f);

                ushort gradientColor = pack_argb4444(final_a, final_r, final_g, final_b);
                destination.write(ushort4(gradientColor, 0, 0, 0), gid);
            }
        }

        // URES: Fill a circle with radial gradient (center to edge)
        kernel void ures_primitive_circle_fill_gradient(
            texture2d<ushort, access::write> destination [[texture(0)]],
            constant int2& center [[buffer(0)]],            // cx, cy
            constant float& radius [[buffer(1)]],
            constant ushort& centerColor [[buffer(2)]],     // ARGB4444 color at center
            constant ushort& edgeColor [[buffer(3)]],       // ARGB4444 color at edge
            uint2 gid [[thread_position_in_grid]]
        ) {
            float px = float(gid.x);
            float py = float(gid.y);
            float cx = float(center.x);
            float cy = float(center.y);

            float dx = px - cx;
            float dy = py - cy;
            float dist = sqrt(dx * dx + dy * dy);

            if (dist <= radius) {
                // Normalize distance (0.0 at center, 1.0 at edge)
                float t = dist / radius;

                // Unpack colors
                uint c_a, c_r, c_g, c_b;
                uint e_a, e_r, e_g, e_b;
                unpack_argb4444(centerColor, c_a, c_r, c_g, c_b);
                unpack_argb4444(edgeColor, e_a, e_r, e_g, e_b);

                // Interpolate between center and edge
                uint final_a = uint(mix(float(c_a), float(e_a), t) + 0.5f);
                uint final_r = uint(mix(float(c_r), float(e_r), t) + 0.5f);
                uint final_g = uint(mix(float(c_g), float(e_g), t) + 0.5f);
                uint final_b = uint(mix(float(c_b), float(e_b), t) + 0.5f);

                ushort gradientColor = pack_argb4444(final_a, final_r, final_g, final_b);
                destination.write(ushort4(gradientColor, 0, 0, 0), gid);
            }
        }

        // URES: Fill a circle with radial gradient and anti-aliasing
        kernel void ures_primitive_circle_fill_gradient_aa(
            texture2d<ushort, access::read_write> destination [[texture(0)]],
            constant int2& center [[buffer(0)]],            // cx, cy
            constant float& radius [[buffer(1)]],
            constant ushort& centerColor [[buffer(2)]],     // ARGB4444 color at center
            constant ushort& edgeColor [[buffer(3)]],       // ARGB4444 color at edge
            uint2 gid [[thread_position_in_grid]]
        ) {
            float px = float(gid.x) + 0.5f;  // Sample at pixel center
            float py = float(gid.y) + 0.5f;
            float cx = float(center.x);
            float cy = float(center.y);

            float dx = px - cx;
            float dy = py - cy;
            float dist = sqrt(dx * dx + dy * dy);

            // Smooth falloff over 1 pixel at edge
            float coverage = 1.0f - smoothstep(radius - 0.7f, radius + 0.7f, dist);

            if (coverage > 0.01f) {
                // Normalize distance for gradient (0.0 at center, 1.0 at edge)
                float t = clamp(dist / radius, 0.0f, 1.0f);

                // Unpack colors
                uint c_a, c_r, c_g, c_b;
                uint e_a, e_r, e_g, e_b;
                unpack_argb4444(centerColor, c_a, c_r, c_g, c_b);
                unpack_argb4444(edgeColor, e_a, e_r, e_g, e_b);

                // Interpolate between center and edge
                uint grad_a = uint(mix(float(c_a), float(e_a), t) + 0.5f);
                uint grad_r = uint(mix(float(c_r), float(e_r), t) + 0.5f);
                uint grad_g = uint(mix(float(c_g), float(e_g), t) + 0.5f);
                uint grad_b = uint(mix(float(c_b), float(e_b), t) + 0.5f);

                ushort gradientColor = pack_argb4444(grad_a, grad_r, grad_g, grad_b);

                if (coverage > 0.99f) {
                    // Fully inside circle
                    destination.write(ushort4(gradientColor, 0, 0, 0), gid);
                } else {
                    // Edge pixel: alpha blend with background
                    ushort bgColor = destination.read(gid).r;

                    // Modulate alpha by coverage for smooth edge
                    uint edge_a = uint(float(grad_a) * coverage + 0.5f);
                    ushort edgeGradColor = pack_argb4444(edge_a, grad_r, grad_g, grad_b);

                    // Blend edge color with background
                    ushort blended = blend_argb4444(edgeGradColor, bgColor);
                    destination.write(ushort4(blended, 0, 0, 0), gid);
                }
            }
        }

        // =============================================================================
        // URES GPU BLITTER OPERATIONS
        // =============================================================================

        // URES: Simple GPU-to-GPU copy blit
        kernel void ures_blitter_copy(
            texture2d<ushort, access::read> source [[texture(0)]],
            texture2d<ushort, access::write> destination [[texture(1)]],
            constant int4& srcRect [[buffer(0)]],   // srcX, srcY, width, height
            constant int2& dstPos [[buffer(1)]],    // dstX, dstY
            uint2 gid [[thread_position_in_grid]]
        ) {
            int dx = int(gid.x);
            int dy = int(gid.y);

            if (dx >= srcRect.z || dy >= srcRect.w) {
                return;
            }

            int srcX = srcRect.x + dx;
            int srcY = srcRect.y + dy;
            int dstX = dstPos.x + dx;
            int dstY = dstPos.y + dy;

            ushort color = source.read(uint2(srcX, srcY)).r;
            destination.write(ushort4(color, 0, 0, 0), uint2(dstX, dstY));
        }

        // URES: Transparent blit (skip alpha=0 pixels)
        kernel void ures_blitter_transparent(
            texture2d<ushort, access::read> source [[texture(0)]],
            texture2d<ushort, access::write> destination [[texture(1)]],
            constant int4& srcRect [[buffer(0)]],   // srcX, srcY, width, height
            constant int2& dstPos [[buffer(1)]],    // dstX, dstY
            uint2 gid [[thread_position_in_grid]]
        ) {
            int dx = int(gid.x);
            int dy = int(gid.y);

            if (dx >= srcRect.z || dy >= srcRect.w) {
                return;
            }

            int srcX = srcRect.x + dx;
            int srcY = srcRect.y + dy;
            int dstX = dstPos.x + dx;
            int dstY = dstPos.y + dy;

            ushort color = source.read(uint2(srcX, srcY)).r;

            // Skip transparent pixels (alpha == 0)
            uint a = (color >> 12u) & 0xFu;
            if (a > 0u) {
                destination.write(ushort4(color, 0, 0, 0), uint2(dstX, dstY));
            }
        }

        // URES: Alpha composite blit (proper alpha blending)
        // This is the KEY feature that makes URES special - true alpha compositing!
        kernel void ures_blitter_alpha_composite(
            texture2d<ushort, access::read> source [[texture(0)]],
            texture2d<ushort, access::read_write> destination [[texture(1)]],
            constant int4& srcRect [[buffer(0)]],   // srcX, srcY, width, height
            constant int2& dstPos [[buffer(1)]],    // dstX, dstY
            uint2 gid [[thread_position_in_grid]]
        ) {
            int dx = int(gid.x);
            int dy = int(gid.y);

            if (dx >= srcRect.z || dy >= srcRect.w) {
                return;
            }

            int srcX = srcRect.x + dx;
            int srcY = srcRect.y + dy;
            int dstX = dstPos.x + dx;
            int dstY = dstPos.y + dy;

            ushort srcColor = source.read(uint2(srcX, srcY)).r;
            ushort dstColor = destination.read(uint2(dstX, dstY)).r;

            // Skip fully transparent source pixels
            uint srcA = (srcColor >> 12u) & 0xFu;
            if (srcA == 0u) {
                return;
            }

            // TRUE alpha blending!
            ushort blended = blend_argb4444(srcColor, dstColor);
            destination.write(ushort4(blended, 0, 0, 0), uint2(dstX, dstY));
        }

        // URES: Clear entire texture with a color
        kernel void ures_blitter_clear(
            texture2d<ushort, access::write> destination [[texture(0)]],
            constant ushort& clearColor [[buffer(0)]],   // ARGB4444 color
            uint2 gid [[thread_position_in_grid]]
        ) {
            destination.write(ushort4(clearColor, 0, 0, 0), gid);
        }
    )";

    id<MTLLibrary> library = [m_impl->device newLibraryWithSource:shaderSource options:nil error:&error];
    if (!library) {
        NSLog(@"Failed to compile shaders: %@", error);
        return false;
    }

    id<MTLFunction> vertexFunction = [library newFunctionWithName:@"text_vertex"];
    id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"text_fragment"];
    id<MTLFunction> transformedTextVertexFunction = [library newFunctionWithName:@"transformedText_vertex"];

    if (!vertexFunction || !fragmentFunction || !transformedTextVertexFunction) {
        NSLog(@"Failed to load shader functions");
        return false;
    }

    // Create pipeline descriptor
    MTLRenderPipelineDescriptor* pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDescriptor.vertexFunction = vertexFunction;
    pipelineDescriptor.fragmentFunction = fragmentFunction;
    pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    pipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
    pipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    pipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

    m_impl->pipelineState = [m_impl->device newRenderPipelineStateWithDescriptor:pipelineDescriptor
                                                                           error:&error];
    if (!m_impl->pipelineState) {
        NSLog(@"Failed to create pipeline state: %@", error.localizedDescription);
        return false;
    }

    // Create transformed text pipeline (uses fixed 1920x1080 coordinates)
    MTLRenderPipelineDescriptor* transformedTextDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    transformedTextDescriptor.vertexFunction = transformedTextVertexFunction;
    transformedTextDescriptor.fragmentFunction = fragmentFunction;
    transformedTextDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    transformedTextDescriptor.colorAttachments[0].blendingEnabled = YES;
    transformedTextDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    transformedTextDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    transformedTextDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    transformedTextDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
    transformedTextDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    transformedTextDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

    m_impl->transformedTextPipelineState = [m_impl->device newRenderPipelineStateWithDescriptor:transformedTextDescriptor
                                                                                          error:&error];
    if (!m_impl->transformedTextPipelineState) {
        NSLog(@"Failed to create transformed text pipeline state: %@", error.localizedDescription);
        return false;
    }

    // Create sextant pipeline (uses same vertex shader, different fragment shader)
    id<MTLFunction> sextantFragmentFunction = [library newFunctionWithName:@"sextant_fragment"];
    NSLog(@"[SEXTANT INIT] sextantFragmentFunction = %@", sextantFragmentFunction);
    if (sextantFragmentFunction) {
        MTLRenderPipelineDescriptor* sextantDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        sextantDescriptor.vertexFunction = vertexFunction;
        sextantDescriptor.fragmentFunction = sextantFragmentFunction;
        sextantDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

        // Enable blending for sextants
        sextantDescriptor.colorAttachments[0].blendingEnabled = YES;
        sextantDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
        sextantDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
        sextantDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        sextantDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
        sextantDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        sextantDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

        m_impl->sextantPipelineState = [m_impl->device newRenderPipelineStateWithDescriptor:sextantDescriptor error:&error];
        if (!m_impl->sextantPipelineState) {
            NSLog(@"[SEXTANT INIT] ERROR: Failed to create sextant pipeline state: %@", error);
        } else {
            NSLog(@"[SEXTANT INIT] SUCCESS: Sextant pipeline state created: %@", m_impl->sextantPipelineState);
        }
    } else {
        NSLog(@"[SEXTANT INIT] ERROR: Could not find sextant_fragment shader function in library!");
    }

    // Create pipeline for rendering graphics texture as full-screen quad
    id<MTLFunction> quadVertexFunction = [library newFunctionWithName:@"graphics_vertex"];
    id<MTLFunction> quadFragmentFunction = [library newFunctionWithName:@"graphics_fragment"];

    if (quadVertexFunction && quadFragmentFunction) {
        MTLRenderPipelineDescriptor* quadDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        quadDescriptor.vertexFunction = quadVertexFunction;
        quadDescriptor.fragmentFunction = quadFragmentFunction;
        quadDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

        // Enable binary transparency for graphics layer (no color blending)
        // Where alpha = 1: draw solid color, where alpha = 0: show background
        quadDescriptor.colorAttachments[0].blendingEnabled = YES;
        quadDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
        quadDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
        quadDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
        quadDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
        quadDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        quadDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

        m_impl->primitivesPipelineState = [m_impl->device newRenderPipelineStateWithDescriptor:quadDescriptor error:&error];
    }

    // Create particle rendering pipeline
    {
        id<MTLFunction> particleVertexFunction = [library newFunctionWithName:@"particleVertexShader"];
        id<MTLFunction> particleFragmentFunction = [library newFunctionWithName:@"particleFragmentShader"];

        if (particleVertexFunction && particleFragmentFunction) {
            MTLRenderPipelineDescriptor* particleDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
            particleDescriptor.vertexFunction = particleVertexFunction;
            particleDescriptor.fragmentFunction = particleFragmentFunction;
            particleDescriptor.colorAttachments[0].pixelFormat = m_impl->metalLayer.pixelFormat;

            // Enable alpha blending for particles
            particleDescriptor.colorAttachments[0].blendingEnabled = YES;
            particleDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
            particleDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
            particleDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
            particleDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
            particleDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
            particleDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

            m_impl->particlePipelineState = [m_impl->device newRenderPipelineStateWithDescriptor:particleDescriptor error:&error];
            if (!m_impl->particlePipelineState) {
                NSLog(@"[PARTICLE INIT] ERROR: Failed to create particle pipeline state: %@", error);
            } else {
                NSLog(@"[PARTICLE INIT] SUCCESS: Particle pipeline state created");
            }
        } else {
            NSLog(@"[PARTICLE INIT] ERROR: Could not load particle shader functions");
        }
    }

    // Create LORES mode pipeline
    id<MTLFunction> loresVertexFunction = [library newFunctionWithName:@"lores_vertex"];
    id<MTLFunction> loresFragmentFunction = [library newFunctionWithName:@"lores_fragment"];
    if (loresVertexFunction && loresFragmentFunction) {
        MTLRenderPipelineDescriptor* loresDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        loresDescriptor.vertexFunction = loresVertexFunction;
        loresDescriptor.fragmentFunction = loresFragmentFunction;
        loresDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        loresDescriptor.colorAttachments[0].blendingEnabled = YES;
        loresDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
        loresDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
        loresDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        loresDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
        loresDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        loresDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

        m_impl->loresPipelineState = [m_impl->device newRenderPipelineStateWithDescriptor:loresDescriptor error:&error];
        if (!m_impl->loresPipelineState) {
            NSLog(@"[LORES] ERROR: Failed to create LORES pipeline state: %@", error);
        } else {
            NSLog(@"[LORES] SUCCESS: LORES pipeline state created");
        }
    } else {
        NSLog(@"[LORES] ERROR: Could not find lores_vertex or lores_fragment shader functions");
    }

    // Create URES mode pipeline
    id<MTLFunction> uresVertexFunction = [library newFunctionWithName:@"ures_vertex"];
    id<MTLFunction> uresFragmentFunction = [library newFunctionWithName:@"ures_fragment"];
    if (uresVertexFunction && uresFragmentFunction) {
        MTLRenderPipelineDescriptor* uresDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        uresDescriptor.vertexFunction = uresVertexFunction;
        uresDescriptor.fragmentFunction = uresFragmentFunction;
        uresDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        uresDescriptor.colorAttachments[0].blendingEnabled = YES;
        uresDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
        uresDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
        uresDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        uresDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
        uresDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        uresDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

        m_impl->uresPipelineState = [m_impl->device newRenderPipelineStateWithDescriptor:uresDescriptor error:&error];
        if (!m_impl->uresPipelineState) {
            NSLog(@"[URES] ERROR: Failed to create URES pipeline state: %@", error);
        } else {
            NSLog(@"[URES] SUCCESS: URES pipeline state created");
        }
    } else {
        NSLog(@"[URES] ERROR: Could not find ures_vertex or ures_fragment shader functions");
    }

    // Create XRES mode pipeline (Mode X: 320×240, 256-color hybrid palette)
    id<MTLFunction> xresVertexFunction = [library newFunctionWithName:@"xres_vertex"];
    id<MTLFunction> xresFragmentFunction = [library newFunctionWithName:@"xres_fragment"];
    if (xresVertexFunction && xresFragmentFunction) {
        MTLRenderPipelineDescriptor* xresDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        xresDescriptor.vertexFunction = xresVertexFunction;
        xresDescriptor.fragmentFunction = xresFragmentFunction;
        xresDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        xresDescriptor.colorAttachments[0].blendingEnabled = YES;
        xresDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
        xresDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
        xresDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        xresDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
        xresDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        xresDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

        m_impl->xresPipelineState = [m_impl->device newRenderPipelineStateWithDescriptor:xresDescriptor error:&error];
        if (!m_impl->xresPipelineState) {
            NSLog(@"[XRES] ERROR: Failed to create XRES pipeline state: %@", error);
        } else {
            NSLog(@"[XRES] SUCCESS: XRES pipeline state created");
        }
    } else {
        NSLog(@"[XRES] ERROR: Could not find xres_vertex or xres_fragment shader functions");
    }

    // Create WRES mode pipeline (Wide XRES: 432×240, 256-color hybrid palette)
    id<MTLFunction> wresVertexFunction = [library newFunctionWithName:@"wres_vertex"];
    id<MTLFunction> wresFragmentFunction = [library newFunctionWithName:@"wres_fragment"];
    if (wresVertexFunction && wresFragmentFunction) {
        MTLRenderPipelineDescriptor* wresDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        wresDescriptor.vertexFunction = wresVertexFunction;
        wresDescriptor.fragmentFunction = wresFragmentFunction;
        wresDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        wresDescriptor.colorAttachments[0].blendingEnabled = YES;
        wresDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
        wresDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
        wresDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        wresDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
        wresDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        wresDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

        m_impl->wresPipelineState = [m_impl->device newRenderPipelineStateWithDescriptor:wresDescriptor error:&error];
        if (!m_impl->wresPipelineState) {
            NSLog(@"[WRES] ERROR: Failed to create WRES pipeline state: %@", error);
        } else {
            NSLog(@"[WRES] SUCCESS: WRES pipeline state created");
        }
    } else {
        NSLog(@"[WRES] ERROR: Could not find wres_vertex or wres_fragment shader functions");
    }

    // Create PRES mode pipeline (Premium Resolution: 1280×720, 256-color hybrid palette)
    id<MTLFunction> presVertexFunction = [library newFunctionWithName:@"pres_vertex"];
    id<MTLFunction> presFragmentFunction = [library newFunctionWithName:@"pres_fragment"];
    if (presVertexFunction && presFragmentFunction) {
        MTLRenderPipelineDescriptor* presDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        presDescriptor.vertexFunction = presVertexFunction;
        presDescriptor.fragmentFunction = presFragmentFunction;
        presDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        presDescriptor.colorAttachments[0].blendingEnabled = YES;
        presDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
        presDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
        presDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        presDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
        presDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        presDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

        m_impl->presPipelineState = [m_impl->device newRenderPipelineStateWithDescriptor:presDescriptor error:&error];
        if (!m_impl->presPipelineState) {
            NSLog(@"[PRES] ERROR: Failed to create PRES pipeline state: %@", error);
        } else {
            NSLog(@"[PRES] SUCCESS: PRES pipeline state created");
        }
    } else {
        NSLog(@"[PRES] ERROR: Could not find pres_vertex or pres_fragment shader functions");
    }

    // Create text effects pipeline for DisplayText with effects
    id<MTLFunction> textEffectVertexFunction = [library newFunctionWithName:@"textEffect_vertex"];
    id<MTLFunction> textEffectFragmentFunction = [library newFunctionWithName:@"textEffect_fragment"];
    if (textEffectVertexFunction && textEffectFragmentFunction) {
        MTLRenderPipelineDescriptor* textEffectDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        textEffectDescriptor.vertexFunction = textEffectVertexFunction;
        textEffectDescriptor.fragmentFunction = textEffectFragmentFunction;
        textEffectDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

        // Enable blending for text effects (additive for glows, alpha for everything else)
        textEffectDescriptor.colorAttachments[0].blendingEnabled = YES;
        textEffectDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
        textEffectDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
        textEffectDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        textEffectDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
        textEffectDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        textEffectDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

        m_impl->textEffectsPipelineState = [m_impl->device newRenderPipelineStateWithDescriptor:textEffectDescriptor error:&error];
        if (!m_impl->textEffectsPipelineState) {
            NSLog(@"[TEXT_EFFECTS] ERROR: Failed to create text effects pipeline state: %@", error);
        } else {
            NSLog(@"[TEXT_EFFECTS] SUCCESS: Text effects pipeline state created");
        }
    } else {
        NSLog(@"[TEXT_EFFECTS] ERROR: Could not load text effect shader functions");
    }

    // Create sampler state for font atlas
    MTLSamplerDescriptor* samplerDescriptor = [[MTLSamplerDescriptor alloc] init];
    samplerDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
    samplerDescriptor.magFilter = MTLSamplerMinMagFilterLinear;
    samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;
    m_impl->samplerState = [m_impl->device newSamplerStateWithDescriptor:samplerDescriptor];

    // Create uniform buffer
    m_impl->uniformBuffer = [m_impl->device newBufferWithLength:sizeof(TextUniforms)
                                                         options:MTLResourceStorageModeShared];

    // Create LORES palette buffer (300 rows × 16 colors × 4 floats for HIRES support)
    m_impl->loresPaletteBuffer = [m_impl->device newBufferWithLength:300 * 16 * 4 * sizeof(float)
                                                              options:MTLResourceStorageModeShared];

    // Create LORES texture (start with HIRES max: 640×300 pixels, R8Uint format for color indices)
    MTLTextureDescriptor* loresTextureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Uint
                                                                                                      width:640
                                                                                                     height:300
                                                                                                  mipmapped:NO];
    loresTextureDescriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
    loresTextureDescriptor.storageMode = MTLStorageModePrivate;  // GPU-resident for blitter

    // Create 8 GPU-resident textures for LORES (Amiga-style blitter)
    for (int i = 0; i < 8; i++) {
        m_impl->loresTextures[i] = [m_impl->device newTextureWithDescriptor:loresTextureDescriptor];
        if (m_impl->loresTextures[i]) {
            NSLog(@"[LORES] Created LORES texture[%d]: 640×300 R8Uint (GPU-resident)", i);
        } else {
            NSLog(@"[LORES] ERROR: Failed to create LORES texture[%d]", i);
            return false;
        }
    }

    // Keep legacy single texture for backward compatibility (front buffer alias)
    m_impl->loresTexture = m_impl->loresTextures[0];

    // Create URES texture (1280×720 pixels, R16Uint format for ARGB4444 data)
    MTLTextureDescriptor* uresTextureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR16Uint
                                                                                                      width:1280
                                                                                                     height:720
                                                                                                  mipmapped:NO];
    uresTextureDescriptor.usage = MTLTextureUsageShaderRead;
    uresTextureDescriptor.storageMode = MTLStorageModeShared;
    m_impl->uresTexture = [m_impl->device newTextureWithDescriptor:uresTextureDescriptor];
    if (m_impl->uresTexture) {
        NSLog(@"[URES] Created URES texture: 1280×720 R16Uint (ARGB4444)");
    } else {
        NSLog(@"[URES] ERROR: Failed to create URES texture");
    }

    // Create XRES palette buffer (hybrid: 240 rows × 16 colors + 240 global colors)
    // Per-row palette: 240 rows × 16 colors × 4 floats = 15,360 floats
    // Global palette: 240 colors × 4 floats = 960 floats
    // Total: 16,320 floats = 65,280 bytes
    size_t xresPaletteSize = (240 * 16 * 4 + 240 * 4) * sizeof(float);
    m_impl->xresPaletteBuffer = [m_impl->device newBufferWithLength:xresPaletteSize
                                                             options:MTLResourceStorageModeShared];
    if (m_impl->xresPaletteBuffer) {
        NSLog(@"[XRES] Created XRES palette buffer: %zu bytes (240 rows × 16 + 240 global)", xresPaletteSize);

        // Initialize palette with IBM RGBI (0-15) + 6×8×5 RGB cube (16-255)
        float* paletteData = (float*)[m_impl->xresPaletteBuffer contents];

        // IBM RGBI 16-color palette (index 0 = transparent for blitting)
        static const uint32_t ibmRGBI[16] = {
            0x00000000, 0xFF0000AA, 0xFF00AA00, 0xFF00AAAA,  // 0 = transparent black
            0xFFAA0000, 0xFFAA00AA, 0xFFAA5500, 0xFFAAAAAA,
            0xFF555555, 0xFF5555FF, 0xFF55FF55, 0xFF55FFFF,
            0xFFFF5555, 0xFFFF55FF, 0xFFFFFF55, 0xFFFFFFFF
        };

        // Initialize per-row palette (indices 0-15) - IBM RGBI for all rows
        for (int row = 0; row < 240; row++) {
            for (int color = 0; color < 16; color++) {
                int offset = (row * 16 + color) * 4;
                uint32_t rgba = ibmRGBI[color];
                paletteData[offset + 0] = ((rgba >> 16) & 0xFF) / 255.0f;  // R
                paletteData[offset + 1] = ((rgba >> 8) & 0xFF) / 255.0f;   // G
                paletteData[offset + 2] = (rgba & 0xFF) / 255.0f;          // B
                paletteData[offset + 3] = ((rgba >> 24) & 0xFF) / 255.0f;  // A (transparent for index 0)
            }
        }

        // Initialize global palette (indices 16-255) - 6×8×5 RGB cube (240 colors)
        // 6 red levels × 8 green levels × 5 blue levels = 240 colors
        int globalOffset = 240 * 16 * 4;
        int colorIndex = 0;
        for (int r = 0; r < 6 && colorIndex < 240; r++) {
            for (int g = 0; g < 8 && colorIndex < 240; g++) {
                for (int b = 0; b < 5 && colorIndex < 240; b++) {
                    int offset = globalOffset + colorIndex * 4;
                    paletteData[offset + 0] = (r * 255 / 5) / 255.0f;  // R: 0, 51, 102, 153, 204, 255
                    paletteData[offset + 1] = (g * 255 / 7) / 255.0f;  // G: 0, 36, 73, 109, 146, 182, 219, 255
                    paletteData[offset + 2] = (b * 255 / 4) / 255.0f;  // B: 0, 64, 128, 191, 255
                    paletteData[offset + 3] = 1.0f;                    // A
                    colorIndex++;
                }
            }
        }
        NSLog(@"[XRES] Initialized palette: IBM RGBI (0=transparent, 1-15=colors) + 6×8×5 RGB cube (16-255)");
    } else {
        NSLog(@"[XRES] ERROR: Failed to create XRES palette buffer");
    }

    // Create XRES texture (320×240 pixels, R8Uint format for 8-bit palette indices)
    MTLTextureDescriptor* xresTextureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Uint
                                                                                                      width:320
                                                                                                     height:240
                                                                                                  mipmapped:NO];
    xresTextureDescriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
    xresTextureDescriptor.storageMode = MTLStorageModeShared;  // Shared for CPU upload support

    // Create 8 GPU-resident textures for XRES (Amiga-style blitter)
    for (int i = 0; i < 8; i++) {
        m_impl->xresTextures[i] = [m_impl->device newTextureWithDescriptor:xresTextureDescriptor];
        if (m_impl->xresTextures[i]) {
            NSLog(@"[XRES] Created XRES texture[%d]: 320×240 R8Uint (GPU-resident)", i);
        } else {
            NSLog(@"[XRES] ERROR: Failed to create XRES texture[%d]!", i);
            return false;
        }
    }
    NSLog(@"[XRES] GPU Blitter: 8 textures ready (614 KB total)");

    // Create WRES palette buffer (same structure as XRES: 240 rows × 16 per-row colors + 240 global colors)
    // Total: (240 * 16 + 240) * 4 floats * 4 bytes = 65,280 bytes
    m_impl->wresPaletteBuffer = [m_impl->device newBufferWithLength:65280 options:MTLResourceStorageModeShared];
    if (m_impl->wresPaletteBuffer) {
        // Initialize WRES palette (same default as XRES)
        float* paletteData = (float*)[m_impl->wresPaletteBuffer contents];

        // Per-row palette (240 rows × 16 colors)
        // Index 0: transparent (0,0,0,0), Indices 1-15: IBM RGBI colors
        static const float ibmRGBI[16][3] = {
            {0.0f, 0.0f, 0.0f},     // 0: Black (transparent for per-row index 0)
            {0.0f, 0.0f, 0.667f},   // 1: Blue
            {0.0f, 0.667f, 0.0f},   // 2: Green
            {0.0f, 0.667f, 0.667f}, // 3: Cyan
            {0.667f, 0.0f, 0.0f},   // 4: Red
            {0.667f, 0.0f, 0.667f}, // 5: Magenta
            {0.667f, 0.333f, 0.0f}, // 6: Brown
            {0.667f, 0.667f, 0.667f}, // 7: Light Gray
            {0.333f, 0.333f, 0.333f}, // 8: Dark Gray
            {0.333f, 0.333f, 1.0f}, // 9: Light Blue
            {0.333f, 1.0f, 0.333f}, // 10: Light Green
            {0.333f, 1.0f, 1.0f},   // 11: Light Cyan
            {1.0f, 0.333f, 0.333f}, // 12: Light Red
            {1.0f, 0.333f, 1.0f},   // 13: Light Magenta
            {1.0f, 1.0f, 0.333f},   // 14: Yellow
            {1.0f, 1.0f, 1.0f}      // 15: White
        };

        for (int row = 0; row < 240; row++) {
            for (int i = 0; i < 16; i++) {
                int offset = (row * 16 + i) * 4;
                paletteData[offset + 0] = ibmRGBI[i][0];  // R
                paletteData[offset + 1] = ibmRGBI[i][1];  // G
                paletteData[offset + 2] = ibmRGBI[i][2];  // B
                paletteData[offset + 3] = (i == 0) ? 0.0f : 1.0f;  // A (transparent for index 0)
            }
        }

        // Global palette (240 colors for indices 16-255)
        // Use 6×8×5 RGB cube (6 red × 8 green × 5 blue = 240 colors)
        int colorIndex = 0;
        for (int r = 0; r < 6; r++) {
            for (int g = 0; g < 8; g++) {
                for (int b = 0; b < 5; b++) {
                    int offset = (240 * 16 + colorIndex) * 4;
                    paletteData[offset + 0] = r / 5.0f;                  // R
                    paletteData[offset + 1] = g / 7.0f;                  // G
                    paletteData[offset + 2] = b / 4.0f;                  // B
                    paletteData[offset + 3] = 1.0f;                    // A
                    colorIndex++;
                }
            }
        }
        NSLog(@"[WRES] Initialized palette: IBM RGBI (0=transparent, 1-15=colors) + 6×8×5 RGB cube (16-255)");
    } else {
        NSLog(@"[WRES] ERROR: Failed to create WRES palette buffer");
    }

    // Create WRES texture (432×240 pixels, R8Uint format for 8-bit palette indices)
    MTLTextureDescriptor* wresTextureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Uint
                                                                                                      width:432
                                                                                                     height:240
                                                                                                  mipmapped:NO];
    wresTextureDescriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
    wresTextureDescriptor.storageMode = MTLStorageModeShared;  // Shared for CPU upload support

    // Create 8 GPU-resident textures for WRES (Amiga-style blitter)
    for (int i = 0; i < 8; i++) {
        m_impl->wresTextures[i] = [m_impl->device newTextureWithDescriptor:wresTextureDescriptor];
        if (m_impl->wresTextures[i]) {
            NSLog(@"[WRES] Created WRES texture[%d]: 432×240 R8Uint (GPU-resident)", i);
        } else {
            NSLog(@"[WRES] ERROR: Failed to create WRES texture[%d]", i);
            return false;
        }
    }

    // Create PRES texture (1280×720 pixels, R8Uint format for 8-bit palette indices)
    MTLTextureDescriptor* presTextureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Uint
                                                                                                      width:1280
                                                                                                     height:720
                                                                                                 mipmapped:NO];
    presTextureDescriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite | MTLTextureUsageRenderTarget;
    presTextureDescriptor.storageMode = MTLStorageModeShared;  // Shared for CPU upload support (consistent with XRES/WRES/URES)

    // Create 8 GPU-resident textures for PRES (Premium Resolution mode)
    for (int i = 0; i < 8; i++) {
        m_impl->presTextures[i] = [m_impl->device newTextureWithDescriptor:presTextureDescriptor];
        if (m_impl->presTextures[i]) {
            NSLog(@"[PRES] Created PRES texture[%d]: 1280×720 R8Uint (GPU-resident)", i);
        } else {
            NSLog(@"[PRES] ERROR: Failed to create PRES texture[%d]", i);
            return false;
        }
    }
    NSLog(@"[WRES] GPU Blitter: 8 textures ready (830 KB total)");

    // Create PRES palette buffer (720 rows × 16 per-row colors + 240 global colors)
    // Per-row palette: 720 rows × 16 colors × 4 floats = 46,080 floats
    // Global palette: 240 colors × 4 floats = 960 floats
    // Total: 47,040 floats = 188,160 bytes
    size_t presPaletteSize = (720 * 16 * 4 + 240 * 4) * sizeof(float);
    m_impl->presPaletteBuffer = [m_impl->device newBufferWithLength:presPaletteSize
                                                             options:MTLResourceStorageModeShared];
    if (m_impl->presPaletteBuffer) {
        NSLog(@"[PRES] Created PRES palette buffer: %zu bytes (720 rows × 16 + 240 global)", presPaletteSize);

        // Initialize palette with IBM RGBI (0-15) + 6×8×5 RGB cube (16-255)
        float* paletteData = (float*)[m_impl->presPaletteBuffer contents];

        // IBM RGBI 16-color palette (index 0 = transparent for blitting)
        static const uint32_t ibmRGBI[16] = {
            0x00000000, 0xFF0000AA, 0xFF00AA00, 0xFF00AAAA,  // 0 = transparent black
            0xFFAA0000, 0xFFAA00AA, 0xFFAA5500, 0xFFAAAAAA,
            0xFF555555, 0xFF5555FF, 0xFF55FF55, 0xFF55FFFF,
            0xFFFF5555, 0xFFFF55FF, 0xFFFFFF55, 0xFFFFFFFF
        };

        // Initialize per-row palette (indices 0-15) - IBM RGBI for all 720 rows
        for (int row = 0; row < 720; row++) {
            for (int color = 0; color < 16; color++) {
                int offset = (row * 16 + color) * 4;
                uint32_t rgba = ibmRGBI[color];
                paletteData[offset + 0] = ((rgba >> 16) & 0xFF) / 255.0f;  // R
                paletteData[offset + 1] = ((rgba >> 8) & 0xFF) / 255.0f;   // G
                paletteData[offset + 2] = (rgba & 0xFF) / 255.0f;          // B
                paletteData[offset + 3] = ((rgba >> 24) & 0xFF) / 255.0f;  // A (transparent for index 0)
            }
        }

        // Initialize global palette (indices 16-255) - 6×8×5 RGB cube (240 colors)
        int globalOffset = 720 * 16 * 4;
        int colorIndex = 0;
        for (int r = 0; r < 6 && colorIndex < 240; r++) {
            for (int g = 0; g < 8 && colorIndex < 240; g++) {
                for (int b = 0; b < 5 && colorIndex < 240; b++) {
                    int offset = globalOffset + colorIndex * 4;
                    paletteData[offset + 0] = (r * 255 / 5) / 255.0f;  // R
                    paletteData[offset + 1] = (g * 255 / 7) / 255.0f;  // G
                    paletteData[offset + 2] = (b * 255 / 4) / 255.0f;  // B
                    paletteData[offset + 3] = 1.0f;                    // A
                    colorIndex++;
                }
            }
        }
        NSLog(@"[PRES] Initialized palette: IBM RGBI (0=transparent, 1-15=colors) + 6×8×5 RGB cube (16-255)");
        NSLog(@"[PRES] GPU Blitter: 8 textures ready (7,373 KB total)");
    } else {
        NSLog(@"[PRES] ERROR: Failed to create PRES palette buffer");
    }

    // Create URES GPU textures (1280×720 pixels, R16Uint format for ARGB4444 data)
    MTLTextureDescriptor* uresGPUTextureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR16Uint
                                                                                                          width:1280
                                                                                                         height:720
                                                                                                      mipmapped:NO];
    uresGPUTextureDescriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
    uresGPUTextureDescriptor.storageMode = MTLStorageModeShared;  // Shared for CPU upload support

    // Create 8 GPU-resident textures for URES (GPU blitter support)
    for (int i = 0; i < 8; i++) {
        m_impl->uresTextures[i] = [m_impl->device newTextureWithDescriptor:uresGPUTextureDescriptor];
        if (m_impl->uresTextures[i]) {
            NSLog(@"[URES] Created URES GPU texture[%d]: 1280×720 R16Uint (GPU-resident)", i);
        } else {
            NSLog(@"[URES] ERROR: Failed to create URES GPU texture[%d]!", i);
            return false;
        }
    }
    NSLog(@"[URES] GPU Blitter: 8 textures ready (14.74 MB total)");

    // Create GPU Blitter compute pipelines
    NSLog(@"[GPU_BLITTER] Creating compute pipeline states...");

    id<MTLFunction> blitterCopyFunction = [library newFunctionWithName:@"blitter_copy"];
    if (blitterCopyFunction) {
        m_impl->blitterCopyPipeline = [m_impl->device newComputePipelineStateWithFunction:blitterCopyFunction error:&error];
        if (!m_impl->blitterCopyPipeline) {
            NSLog(@"[GPU_BLITTER] ERROR: Failed to create blitter_copy pipeline: %@", error);
        } else {
            NSLog(@"[GPU_BLITTER] SUCCESS: Created blitter_copy pipeline");
        }
    } else {
        NSLog(@"[GPU_BLITTER] ERROR: Could not find blitter_copy function");
    }

    id<MTLFunction> blitterTransparentFunction = [library newFunctionWithName:@"blitter_transparent"];
    if (blitterTransparentFunction) {
        m_impl->blitterTransparentPipeline = [m_impl->device newComputePipelineStateWithFunction:blitterTransparentFunction error:&error];
        if (!m_impl->blitterTransparentPipeline) {
            NSLog(@"[GPU_BLITTER] ERROR: Failed to create blitter_transparent pipeline: %@", error);
        } else {
            NSLog(@"[GPU_BLITTER] SUCCESS: Created blitter_transparent pipeline");
        }
    } else {
        NSLog(@"[GPU_BLITTER] ERROR: Could not find blitter_transparent function");
    }

    id<MTLFunction> blitterMintermFunction = [library newFunctionWithName:@"blitter_minterm"];
    if (blitterMintermFunction) {
        m_impl->blitterMintermPipeline = [m_impl->device newComputePipelineStateWithFunction:blitterMintermFunction error:&error];
        if (!m_impl->blitterMintermPipeline) {
            NSLog(@"[GPU_BLITTER] ERROR: Failed to create blitter_minterm pipeline: %@", error);
        } else {
            NSLog(@"[GPU_BLITTER] SUCCESS: Created blitter_minterm pipeline");
        }
    } else {
        NSLog(@"[GPU_BLITTER] ERROR: Could not find blitter_minterm function");
    }

    id<MTLFunction> blitterClearFunction = [library newFunctionWithName:@"blitter_clear"];
    if (blitterClearFunction) {
        m_impl->blitterClearPipeline = [m_impl->device newComputePipelineStateWithFunction:blitterClearFunction error:&error];
        if (!m_impl->blitterClearPipeline) {
            NSLog(@"[GPU_BLITTER] ERROR: Failed to create blitter_clear pipeline: %@", error);
        } else {
            NSLog(@"[GPU_BLITTER] SUCCESS: Created blitter_clear pipeline");
        }
    } else {
        NSLog(@"[GPU_BLITTER] ERROR: Could not find blitter_clear function");
    }

    // Create GPU primitive pipeline states
    NSLog(@"[GPU_PRIMITIVES] Creating compute pipeline states...");

    id<MTLFunction> primitiveRectFunction = [library newFunctionWithName:@"primitive_rect_fill"];
    if (primitiveRectFunction) {
        m_impl->primitiveRectPipeline = [m_impl->device newComputePipelineStateWithFunction:primitiveRectFunction error:&error];
        if (!m_impl->primitiveRectPipeline) {
            NSLog(@"[GPU_PRIMITIVES] ERROR: Failed to create primitive_rect_fill pipeline: %@", error);
        } else {
            NSLog(@"[GPU_PRIMITIVES] SUCCESS: Created primitive_rect_fill pipeline");
        }
    } else {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Could not find primitive_rect_fill function");
    }

    id<MTLFunction> primitiveCircleFunction = [library newFunctionWithName:@"primitive_circle_fill"];
    if (primitiveCircleFunction) {
        m_impl->primitiveCirclePipeline = [m_impl->device newComputePipelineStateWithFunction:primitiveCircleFunction error:&error];
        if (!m_impl->primitiveCirclePipeline) {
            NSLog(@"[GPU_PRIMITIVES] ERROR: Failed to create primitive_circle_fill pipeline: %@", error);
        } else {
            NSLog(@"[GPU_PRIMITIVES] SUCCESS: Created primitive_circle_fill pipeline");
        }
    } else {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Could not find primitive_circle_fill function");
    }

    id<MTLFunction> primitiveLineFunction = [library newFunctionWithName:@"primitive_line"];
    if (primitiveLineFunction) {
        m_impl->primitiveLinePipeline = [m_impl->device newComputePipelineStateWithFunction:primitiveLineFunction error:&error];
        if (!m_impl->primitiveLinePipeline) {
            NSLog(@"[GPU_PRIMITIVES] ERROR: Failed to create primitive_line pipeline: %@", error);
        } else {
            NSLog(@"[GPU_PRIMITIVES] SUCCESS: Created primitive_line pipeline");
        }
    } else {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Could not find primitive_line function");
    }

    // Create anti-aliased primitive pipeline states
    NSLog(@"[GPU_PRIMITIVES] Creating anti-aliased compute pipeline states...");

    id<MTLFunction> primitiveCircleAAFunction = [library newFunctionWithName:@"primitive_circle_fill_aa"];
    if (primitiveCircleAAFunction) {
        m_impl->primitiveCircleAAPipeline = [m_impl->device newComputePipelineStateWithFunction:primitiveCircleAAFunction error:&error];
        if (!m_impl->primitiveCircleAAPipeline) {
            NSLog(@"[GPU_PRIMITIVES] ERROR: Failed to create primitive_circle_fill_aa pipeline: %@", error);
        } else {
            NSLog(@"[GPU_PRIMITIVES] SUCCESS: Created primitive_circle_fill_aa pipeline");
        }
    } else {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Could not find primitive_circle_fill_aa function");
    }

    id<MTLFunction> primitiveLineAAFunction = [library newFunctionWithName:@"primitive_line_aa"];
    if (primitiveLineAAFunction) {
        m_impl->primitiveLineAAPipeline = [m_impl->device newComputePipelineStateWithFunction:primitiveLineAAFunction error:&error];
        if (!m_impl->primitiveLineAAPipeline) {
            NSLog(@"[GPU_PRIMITIVES] ERROR: Failed to create primitive_line_aa pipeline: %@", error);
        } else {
            NSLog(@"[GPU_PRIMITIVES] SUCCESS: Created primitive_line_aa pipeline");
        }
    } else {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Could not find primitive_line_aa function");
    }

    NSLog(@"[GPU_BLITTER] Compute pipelines ready for XRES/WRES");

    // Create URES GPU primitive pipeline states
    NSLog(@"[URES_GPU_PRIMITIVES] Creating compute pipeline states...");

    id<MTLFunction> uresPrimitiveRectFunction = [library newFunctionWithName:@"ures_primitive_rect_fill"];
    if (uresPrimitiveRectFunction) {
        m_impl->uresPrimitiveRectPipeline = [m_impl->device newComputePipelineStateWithFunction:uresPrimitiveRectFunction error:&error];
        if (!m_impl->uresPrimitiveRectPipeline) {
            NSLog(@"[URES_GPU_PRIMITIVES] ERROR: Failed to create ures_primitive_rect_fill pipeline: %@", error);
        } else {
            NSLog(@"[URES_GPU_PRIMITIVES] SUCCESS: Created ures_primitive_rect_fill pipeline");
        }
    } else {
        NSLog(@"[URES_GPU_PRIMITIVES] ERROR: Could not find ures_primitive_rect_fill function");
    }

    id<MTLFunction> uresPrimitiveCircleFunction = [library newFunctionWithName:@"ures_primitive_circle_fill"];
    if (uresPrimitiveCircleFunction) {
        m_impl->uresPrimitiveCirclePipeline = [m_impl->device newComputePipelineStateWithFunction:uresPrimitiveCircleFunction error:&error];
        if (!m_impl->uresPrimitiveCirclePipeline) {
            NSLog(@"[URES_GPU_PRIMITIVES] ERROR: Failed to create ures_primitive_circle_fill pipeline: %@", error);
        } else {
            NSLog(@"[URES_GPU_PRIMITIVES] SUCCESS: Created ures_primitive_circle_fill pipeline");
        }
    } else {
        NSLog(@"[URES_GPU_PRIMITIVES] ERROR: Could not find ures_primitive_circle_fill function");
    }

    id<MTLFunction> uresPrimitiveLineFunction = [library newFunctionWithName:@"ures_primitive_line"];
    if (uresPrimitiveLineFunction) {
        m_impl->uresPrimitiveLinePipeline = [m_impl->device newComputePipelineStateWithFunction:uresPrimitiveLineFunction error:&error];
        if (!m_impl->uresPrimitiveLinePipeline) {
            NSLog(@"[URES_GPU_PRIMITIVES] ERROR: Failed to create ures_primitive_line pipeline: %@", error);
        } else {
            NSLog(@"[URES_GPU_PRIMITIVES] SUCCESS: Created ures_primitive_line pipeline");
        }
    } else {
        NSLog(@"[URES_GPU_PRIMITIVES] ERROR: Could not find ures_primitive_line function");
    }

    // Create URES anti-aliased primitive pipeline states
    NSLog(@"[URES_GPU_PRIMITIVES] Creating anti-aliased compute pipeline states...");

    id<MTLFunction> uresPrimitiveCircleAAFunction = [library newFunctionWithName:@"ures_primitive_circle_fill_aa"];
    if (uresPrimitiveCircleAAFunction) {
        m_impl->uresPrimitiveCircleAAPipeline = [m_impl->device newComputePipelineStateWithFunction:uresPrimitiveCircleAAFunction error:&error];
        if (!m_impl->uresPrimitiveCircleAAPipeline) {
            NSLog(@"[URES_GPU_PRIMITIVES] ERROR: Failed to create ures_primitive_circle_fill_aa pipeline: %@", error);
        } else {
            NSLog(@"[URES_GPU_PRIMITIVES] SUCCESS: Created ures_primitive_circle_fill_aa pipeline");
        }
    } else {
        NSLog(@"[URES_GPU_PRIMITIVES] ERROR: Could not find ures_primitive_circle_fill_aa function");
    }

    id<MTLFunction> uresPrimitiveLineAAFunction = [library newFunctionWithName:@"ures_primitive_line_aa"];
    if (uresPrimitiveLineAAFunction) {
        m_impl->uresPrimitiveLineAAPipeline = [m_impl->device newComputePipelineStateWithFunction:uresPrimitiveLineAAFunction error:&error];
        if (!m_impl->uresPrimitiveLineAAPipeline) {
            NSLog(@"[URES_GPU_PRIMITIVES] ERROR: Failed to create ures_primitive_line_aa pipeline: %@", error);
        } else {
            NSLog(@"[URES_GPU_PRIMITIVES] SUCCESS: Created ures_primitive_line_aa pipeline");
        }
    } else {
        NSLog(@"[URES_GPU_PRIMITIVES] ERROR: Could not find ures_primitive_line_aa function");
    }

    // ===== URES GPU GRADIENT PRIMITIVE PIPELINES =====
    NSLog(@"[URES_GPU_GRADIENTS] Creating gradient compute pipeline states...");

    id<MTLFunction> uresPrimitiveRectGradientFunction = [library newFunctionWithName:@"ures_primitive_rect_fill_gradient"];
    if (uresPrimitiveRectGradientFunction) {
        m_impl->uresPrimitiveRectGradientPipeline = [m_impl->device newComputePipelineStateWithFunction:uresPrimitiveRectGradientFunction error:&error];
        if (!m_impl->uresPrimitiveRectGradientPipeline) {
            NSLog(@"[URES_GPU_GRADIENTS] ERROR: Failed to create ures_primitive_rect_fill_gradient pipeline: %@", error);
        } else {
            NSLog(@"[URES_GPU_GRADIENTS] SUCCESS: Created ures_primitive_rect_fill_gradient pipeline");
        }
    } else {
        NSLog(@"[URES_GPU_GRADIENTS] ERROR: Could not find ures_primitive_rect_fill_gradient function");
    }

    id<MTLFunction> uresPrimitiveCircleGradientFunction = [library newFunctionWithName:@"ures_primitive_circle_fill_gradient"];
    if (uresPrimitiveCircleGradientFunction) {
        m_impl->uresPrimitiveCircleGradientPipeline = [m_impl->device newComputePipelineStateWithFunction:uresPrimitiveCircleGradientFunction error:&error];
        if (!m_impl->uresPrimitiveCircleGradientPipeline) {
            NSLog(@"[URES_GPU_GRADIENTS] ERROR: Failed to create ures_primitive_circle_fill_gradient pipeline: %@", error);
        } else {
            NSLog(@"[URES_GPU_GRADIENTS] SUCCESS: Created ures_primitive_circle_fill_gradient pipeline");
        }
    } else {
        NSLog(@"[URES_GPU_GRADIENTS] ERROR: Could not find ures_primitive_circle_fill_gradient function");
    }

    id<MTLFunction> uresPrimitiveCircleGradientAAFunction = [library newFunctionWithName:@"ures_primitive_circle_fill_gradient_aa"];
    if (uresPrimitiveCircleGradientAAFunction) {
        m_impl->uresPrimitiveCircleGradientAAPipeline = [m_impl->device newComputePipelineStateWithFunction:uresPrimitiveCircleGradientAAFunction error:&error];
        if (!m_impl->uresPrimitiveCircleGradientAAPipeline) {
            NSLog(@"[URES_GPU_GRADIENTS] ERROR: Failed to create ures_primitive_circle_fill_gradient_aa pipeline: %@", error);
        } else {
            NSLog(@"[URES_GPU_GRADIENTS] SUCCESS: Created ures_primitive_circle_fill_gradient_aa pipeline");
        }
    } else {
        NSLog(@"[URES_GPU_GRADIENTS] ERROR: Could not find ures_primitive_circle_fill_gradient_aa function");
    }

    // ===== URES GPU BLITTER PIPELINES =====
    // Create URES GPU blitter pipeline states
    NSLog(@"[URES_GPU_BLITTER] Creating compute pipeline states...");

    id<MTLFunction> uresBlitterCopyFunction = [library newFunctionWithName:@"ures_blitter_copy"];
    if (uresBlitterCopyFunction) {
        m_impl->uresBlitterCopyPipeline = [m_impl->device newComputePipelineStateWithFunction:uresBlitterCopyFunction error:&error];
        if (!m_impl->uresBlitterCopyPipeline) {
            NSLog(@"[URES_GPU_BLITTER] ERROR: Failed to create ures_blitter_copy pipeline: %@", error);
        } else {
            NSLog(@"[URES_GPU_BLITTER] SUCCESS: Created ures_blitter_copy pipeline");
        }
    } else {
        NSLog(@"[URES_GPU_BLITTER] ERROR: Could not find ures_blitter_copy function");
    }

    id<MTLFunction> uresBlitterTransparentFunction = [library newFunctionWithName:@"ures_blitter_transparent"];
    if (uresBlitterTransparentFunction) {
        m_impl->uresBlitterTransparentPipeline = [m_impl->device newComputePipelineStateWithFunction:uresBlitterTransparentFunction error:&error];
        if (!m_impl->uresBlitterTransparentPipeline) {
            NSLog(@"[URES_GPU_BLITTER] ERROR: Failed to create ures_blitter_transparent pipeline: %@", error);
        } else {
            NSLog(@"[URES_GPU_BLITTER] SUCCESS: Created ures_blitter_transparent pipeline");
        }
    } else {
        NSLog(@"[URES_GPU_BLITTER] ERROR: Could not find ures_blitter_transparent function");
    }

    id<MTLFunction> uresBlitterAlphaCompositeFunction = [library newFunctionWithName:@"ures_blitter_alpha_composite"];
    if (uresBlitterAlphaCompositeFunction) {
        m_impl->uresBlitterAlphaCompositePipeline = [m_impl->device newComputePipelineStateWithFunction:uresBlitterAlphaCompositeFunction error:&error];
        if (!m_impl->uresBlitterAlphaCompositePipeline) {
            NSLog(@"[URES_GPU_BLITTER] ERROR: Failed to create ures_blitter_alpha_composite pipeline: %@", error);
        } else {
            NSLog(@"[URES_GPU_BLITTER] SUCCESS: Created ures_blitter_alpha_composite pipeline");
        }
    } else {
        NSLog(@"[URES_GPU_BLITTER] ERROR: Could not find ures_blitter_alpha_composite function");
    }

    id<MTLFunction> uresBlitterClearFunction = [library newFunctionWithName:@"ures_blitter_clear"];
    if (uresBlitterClearFunction) {
        m_impl->uresBlitterClearPipeline = [m_impl->device newComputePipelineStateWithFunction:uresBlitterClearFunction error:&error];
        if (!m_impl->uresBlitterClearPipeline) {
            NSLog(@"[URES_GPU_BLITTER] ERROR: Failed to create ures_blitter_clear pipeline: %@", error);
        } else {
            NSLog(@"[URES_GPU_BLITTER] SUCCESS: Created ures_blitter_clear pipeline");
        }
    } else {
        NSLog(@"[URES_GPU_BLITTER] ERROR: Could not find ures_blitter_clear function");
    }

    NSLog(@"[URES_GPU] Compute pipelines ready for URES (primitives + blitter)");

    // Initialize projection matrix
    updateProjectionMatrix();

    m_impl->initialized = true;
    return true;
}

void MetalRenderer::setFontAtlas(std::shared_ptr<FontAtlas> atlas) {
    m_impl->fontAtlas = atlas;
    if (atlas) {
        m_impl->cellWidth = atlas->getGlyphWidth();
        m_impl->cellHeight = atlas->getGlyphHeight();
    }
}

void MetalRenderer::setTextDisplayManager(std::shared_ptr<TextDisplayManager> textDisplay) {
    m_impl->textDisplayManager = textDisplay;
}

void MetalRenderer::setConfig(const RenderConfig& config) {
    m_impl->config = config;
}

const RenderConfig& MetalRenderer::getConfig() const {
    return m_impl->config;
}

void MetalRenderer::setViewportSize(uint32_t width, uint32_t height) {
    m_impl->viewportWidth = width;
    m_impl->viewportHeight = height;

    // NOTE: Do NOT set metalLayer.drawableSize here!
    // The DisplayManager handles drawable size and applies backing scale factor for Retina displays.
    // Setting it here would override the correct scaled size and break LORES/URES rendering.

    // GraphicsRenderer stays at fixed 1920x1080 - no resize needed
    // if (m_impl->graphicsRenderer) {
    //     m_impl->graphicsRenderer->resize(width, height);
    // }

    updateProjectionMatrix();
}

void MetalRenderer::updateProjectionMatrix() {
    // Create orthographic projection matrix
    // Maps (0, 0) to top-left, (width, height) to bottom-right
    float left = 0.0f;
    float right = static_cast<float>(m_impl->viewportWidth);
    float top = 0.0f;
    float bottom = static_cast<float>(m_impl->viewportHeight);
    float near = -1.0f;
    float far = 1.0f;

    float* m = m_impl->uniforms.projectionMatrix;
    std::memset(m, 0, sizeof(float) * 16);

    m[0] = 2.0f / (right - left);
    m[5] = 2.0f / (top - bottom);
    m[10] = 1.0f / (far - near);
    m[12] = -(right + left) / (right - left);
    m[13] = -(top + bottom) / (top - bottom);
    m[14] = -near / (far - near);
    m[15] = 1.0f;
}

void MetalRenderer::buildVertexBuffer(const TextGrid& textGrid) {
    m_impl->vertices.clear();

    if (!m_impl->fontAtlas) {
        return;
    }

    int cols = textGrid.getWidth();
    int rows = textGrid.getHeight();

    // Reserve space for maximum possible vertices
    // 6 per cell for background + 6 per cell for text = 12 per cell
    m_impl->vertices.reserve(static_cast<size_t>(rows * cols * 12));

    // Calculate cell size to fill the viewport
    float cellW = static_cast<float>(m_impl->viewportWidth) / cols;
    float cellH = static_cast<float>(m_impl->viewportHeight) / rows;

    // First pass: render all backgrounds
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            auto cell = textGrid.getCell(col, row);

            // Calculate screen position
            float x = col * cellW;
            float y = row * cellH;

            // Convert background color from uint32_t RGBA to float[4]
            float bgColor[4] = {
                ((cell.background >> 24) & 0xFF) / 255.0f,
                ((cell.background >> 16) & 0xFF) / 255.0f,
                ((cell.background >> 8) & 0xFF) / 255.0f,
                (cell.background & 0xFF) / 255.0f
            };

            // Skip rendering background if it's fully transparent (allows tilemaps/graphics to show through)
            if (bgColor[3] == 0.0f) {
                continue;
            }

            // Create background quad (solid color, no texture)
            TextVertex v0, v1, v2, v3;

            // Top-left (use negative texcoords to indicate background)
            v0.position[0] = x;
            v0.position[1] = y;
            v0.texCoord[0] = -1.0f;
            v0.texCoord[1] = -1.0f;
            std::memcpy(v0.color, bgColor, sizeof(bgColor));

            // Top-right
            v1.position[0] = x + cellW;
            v1.position[1] = y;
            v1.texCoord[0] = -1.0f;
            v1.texCoord[1] = -1.0f;
            std::memcpy(v1.color, bgColor, sizeof(bgColor));

            // Bottom-left
            v2.position[0] = x;
            v2.position[1] = y + cellH;
            v2.texCoord[0] = -1.0f;
            v2.texCoord[1] = -1.0f;
            std::memcpy(v2.color, bgColor, sizeof(bgColor));

            // Bottom-right
            v3.position[0] = x + cellW;
            v3.position[1] = y + cellH;
            v3.texCoord[0] = -1.0f;
            v3.texCoord[1] = -1.0f;
            std::memcpy(v3.color, bgColor, sizeof(bgColor));

            // Triangle 1
            m_impl->vertices.push_back(v0);
            m_impl->vertices.push_back(v1);
            m_impl->vertices.push_back(v2);

            // Triangle 2
            m_impl->vertices.push_back(v1);
            m_impl->vertices.push_back(v3);
            m_impl->vertices.push_back(v2);
        }
    }

    // Second pass: render normal text glyphs (skip sextants)
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            auto cell = textGrid.getCell(col, row);

            // Skip empty cells (space character)
            if (cell.character == 0 || cell.character == U' ') {
                continue;
            }

            // Skip sextant characters (U+1FB00-U+1FB3F) - will render separately
            if (cell.character >= 0x1FB00 && cell.character <= 0x1FB3F) {
                continue;
            }

            // Get glyph info from font atlas
            auto glyphMetrics = m_impl->fontAtlas->getGlyphMetrics(cell.character);

            // Calculate screen position
            float x = col * cellW;
            float y = row * cellH;

            // Convert colors from uint32_t RGBA to float[4]
            float fgColor[4] = {
                ((cell.foreground >> 24) & 0xFF) / 255.0f,
                ((cell.foreground >> 16) & 0xFF) / 255.0f,
                ((cell.foreground >> 8) & 0xFF) / 255.0f,
                (cell.foreground & 0xFF) / 255.0f
            };

            // Create quad (2 triangles)
            TextVertex v0, v1, v2, v3;

            // Top-left
            v0.position[0] = x;
            v0.position[1] = y;
            v0.texCoord[0] = glyphMetrics.atlasX;
            v0.texCoord[1] = glyphMetrics.atlasY;
            std::memcpy(v0.color, fgColor, sizeof(fgColor));

            // Top-right
            v1.position[0] = x + cellW;
            v1.position[1] = y;
            v1.texCoord[0] = glyphMetrics.atlasX + glyphMetrics.atlasWidth;
            v1.texCoord[1] = glyphMetrics.atlasY;
            std::memcpy(v1.color, fgColor, sizeof(fgColor));

            // Bottom-left
            v2.position[0] = x;
            v2.position[1] = y + cellH;
            v2.texCoord[0] = glyphMetrics.atlasX;
            v2.texCoord[1] = glyphMetrics.atlasY + glyphMetrics.atlasHeight;
            std::memcpy(v2.color, fgColor, sizeof(fgColor));

            // Bottom-right
            v3.position[0] = x + cellW;
            v3.position[1] = y + cellH;
            v3.texCoord[0] = glyphMetrics.atlasX + glyphMetrics.atlasWidth;
            v3.texCoord[1] = glyphMetrics.atlasY + glyphMetrics.atlasHeight;
            std::memcpy(v3.color, fgColor, sizeof(fgColor));

            // Triangle 1
            m_impl->vertices.push_back(v0);
            m_impl->vertices.push_back(v1);
            m_impl->vertices.push_back(v2);

            // Triangle 2
            m_impl->vertices.push_back(v1);
            m_impl->vertices.push_back(v3);
            m_impl->vertices.push_back(v2);
        }
    }

    // DEBUG: Force some sextant characters into the grid for testing
    static bool testSextantsAdded = false;
    if (!testSextantsAdded) {
        // Directly add test sextants to verify rendering
        for (int testCol = 0; testCol < 40 && testCol < cols; testCol++) {
            // Put various sextant patterns across row 10
            uint32_t testChar = 0x1FB00 + (testCol % 64);  // Cycle through all 64 patterns
            uint32_t testForeground = 0xFFFFFFFF;  // All white for now (color indices all 15)
            uint32_t testBackground = 0x000000FF;  // Black background

            // Manually set the cell (this is a hack for testing)
            // We'd need access to the TextGrid to actually set it properly
        }
        testSextantsAdded = true;

    }

    // Third pass: render sextant characters separately
    // Each sextant is a 2x3 grid of sub-pixels, rendered as separate colored quads
    m_impl->sextantVertices.clear();

    // NSLog(@"[LORES DEBUG] Starting sextant vertex generation for %dx%d grid", cols, rows);

    // RGBI 16-color palette (matches shader)
    static const float palette[16][4] = {
        {0.0f, 0.0f, 0.0f, 1.0f},       // 0: Black
        {0.0f, 0.0f, 0.667f, 1.0f},     // 1: Blue
        {0.0f, 0.667f, 0.0f, 1.0f},     // 2: Green
        {0.0f, 0.667f, 0.667f, 1.0f},   // 3: Cyan
        {0.667f, 0.0f, 0.0f, 1.0f},     // 4: Red
        {0.667f, 0.0f, 0.667f, 1.0f},   // 5: Magenta
        {0.667f, 0.333f, 0.0f, 1.0f},   // 6: Brown
        {0.667f, 0.667f, 0.667f, 1.0f}, // 7: Light Gray
        {0.333f, 0.333f, 0.333f, 1.0f}, // 8: Dark Gray
        {0.333f, 0.333f, 1.0f, 1.0f},   // 9: Light Blue
        {0.333f, 1.0f, 0.333f, 1.0f},   // A: Light Green
        {0.333f, 1.0f, 1.0f, 1.0f},     // B: Light Cyan
        {1.0f, 0.333f, 0.333f, 1.0f},   // C: Light Red
        {1.0f, 0.333f, 1.0f, 1.0f},     // D: Light Magenta
        {1.0f, 1.0f, 0.333f, 1.0f},     // E: Yellow
        {1.0f, 1.0f, 1.0f, 1.0f}        // F: White
    };

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            auto cell = textGrid.getCell(col, row);

            // Only render sextant characters (U+1FB00-U+1FB3F)
            if (cell.character < 0x1FB00 || cell.character > 0x1FB3F) {
                continue;
            }

            // Extract 6-bit pattern from character code
            uint32_t pattern = cell.character - 0x1FB00;  // 0-63

            // Debug logging disabled



            // Calculate cell position
            float cellX = col * cellW;
            float cellY = row * cellH;



            // Sub-pixel dimensions (2x3 grid)
            float subW = cellW / 2.0f;
            float subH = cellH / 3.0f;

            // Unpack RGBI color indices from foreground
            // R byte (bits 24-31): [color5][color4] top row
            // G byte (bits 16-23): [color3][color2] middle row
            // B byte (bits 8-15): [color1][color0] bottom row
            // A byte (bits 0-7): alpha
            uint8_t r = (cell.foreground >> 24) & 0xFF;
            uint8_t g = (cell.foreground >> 16) & 0xFF;
            uint8_t b = (cell.foreground >> 8) & 0xFF;

            uint8_t colorIndices[6];
            colorIndices[5] = (r >> 4) & 0xF;  // top-left (bit 5)
            colorIndices[4] = r & 0xF;         // top-right (bit 4)
            colorIndices[3] = (g >> 4) & 0xF;  // mid-left (bit 3)
            colorIndices[2] = g & 0xF;         // mid-right (bit 2)
            colorIndices[1] = (b >> 4) & 0xF;  // bottom-left (bit 1)
            colorIndices[0] = b & 0xF;         // bottom-right (bit 0)

            float alpha = (cell.foreground & 0xFF) / 255.0f;

            // Generate quads for each ON bit in the pattern
            for (int bitIndex = 0; bitIndex < 6; bitIndex++) {
                // Check if this bit is ON
                if ((pattern & (1 << bitIndex)) == 0) {
                    continue;  // Skip OFF pixels
                }

                // Calculate sub-pixel position from bit index
                // Bit layout: bit_index = row * 2 + col
                // Bit 0 = bottom-right (row=0, col=1)
                // Bit 1 = bottom-left (row=0, col=0)
                // Bit 2 = mid-right (row=1, col=1)
                // Bit 3 = mid-left (row=1, col=0)
                // Bit 4 = top-right (row=2, col=1)
                // Bit 5 = top-left (row=2, col=0)
                int subRow = bitIndex / 2;  // 0, 1, or 2 (bottom to top)
                int subCol = bitIndex % 2;  // 0 or 1 (left/right)

                float px = cellX + subCol * subW;
                float py = cellY + subRow * subH;

                // Get color index for this pixel
                uint8_t colorIdx = colorIndices[bitIndex];
                // Encode color index in vertex color (shader will look it up from palette buffer)
                // Store normalized color index (0-15 -> 0.0-1.0) in R channel
                float pixelColor[4] = {
                    colorIdx / 15.0f,  // Color index normalized to 0.0-1.0
                    0.0f,               // Unused
                    0.0f,               // Unused
                    1.0f                // Alpha (fully opaque)
                };

                // Debug logging disabled

                // DEBUG: Log first quad
                static int quadDebug = 0;
                if (quadDebug < 1) {
                    NSLog(@"[QUAD] First sextant quad: pattern=0x%02X bit=%d pos=(%.1f,%.1f) size=(%.1f,%.1f) color=%d",
                          pattern, bitIndex, px, py, subW, subH, colorIdx);
                    quadDebug++;
                }

                // Create quad for this sub-pixel (solid color, no texture)
                TextVertex v0, v1, v2, v3;

                // Top-left
                v0.position[0] = px;
                v0.position[1] = py;
                v0.texCoord[0] = -1.0f;  // Negative texcoord = solid color
                v0.texCoord[1] = -1.0f;
                std::memcpy(v0.color, pixelColor, sizeof(pixelColor));

                // Top-right
                v1.position[0] = px + subW;
                v1.position[1] = py;
                v1.texCoord[0] = -1.0f;
                v1.texCoord[1] = -1.0f;
                std::memcpy(v1.color, pixelColor, sizeof(pixelColor));

                // Bottom-left
                v2.position[0] = px;
                v2.position[1] = py + subH;
                v2.texCoord[0] = -1.0f;
                v2.texCoord[1] = -1.0f;
                std::memcpy(v2.color, pixelColor, sizeof(pixelColor));

                // Bottom-right
                v3.position[0] = px + subW;
                v3.position[1] = py + subH;
                v3.texCoord[0] = -1.0f;
                v3.texCoord[1] = -1.0f;
                std::memcpy(v3.color, pixelColor, sizeof(pixelColor));

                // Triangle 1
                m_impl->sextantVertices.push_back(v0);
                m_impl->sextantVertices.push_back(v1);
                m_impl->sextantVertices.push_back(v2);

                // Triangle 2
                m_impl->sextantVertices.push_back(v1);
                m_impl->sextantVertices.push_back(v3);
                m_impl->sextantVertices.push_back(v2);
            }
        }
    }

    // NSLog(@"[LORES DEBUG] Sextant vertex generation complete: %zu vertices created", m_impl->sextantVertices.size());
}

void MetalRenderer::updateUniforms(float deltaTime) {
    m_impl->elapsedTime += deltaTime;

    // Update cursor blink phase
    if (m_impl->config.enableBlink) {
        m_impl->cursorPhase += deltaTime * m_impl->config.cursorBlinkRate;
        while (m_impl->cursorPhase > 1.0f) {
            m_impl->cursorPhase -= 1.0f;
        }
    } else {
        m_impl->cursorPhase = 1.0f;
    }

    // Fill uniforms
    m_impl->uniforms.viewportSize[0] = static_cast<float>(m_impl->viewportWidth);
    m_impl->uniforms.viewportSize[1] = static_cast<float>(m_impl->viewportHeight);
    m_impl->uniforms.cellSize[0] = static_cast<float>(m_impl->cellWidth);
    m_impl->uniforms.cellSize[1] = static_cast<float>(m_impl->cellHeight);
    m_impl->uniforms.time = m_impl->elapsedTime;
    m_impl->uniforms.cursorBlink = m_impl->cursorPhase;
    m_impl->uniforms.renderMode = m_impl->config.renderMode;

    // Get current LORES resolution height for shader
    auto displayMgr = STApi::Context::instance().display();
    if (displayMgr && displayMgr->isLoResMode()) {
        auto loresBuffer = displayMgr->getLoResBuffer();
        if (loresBuffer) {
            m_impl->uniforms.loresHeight = static_cast<uint32_t>(loresBuffer->getHeight());
        } else {
            m_impl->uniforms.loresHeight = 75;  // Default to LORES
        }
    } else {
        m_impl->uniforms.loresHeight = 75;  // Default to LORES
    }

    // Copy to GPU buffer
    std::memcpy([m_impl->uniformBuffer contents], &m_impl->uniforms, sizeof(TextUniforms));
}

bool MetalRenderer::renderFrame(TextGrid& textGrid, float deltaTime) {
    if (!m_impl->initialized || !m_impl->fontAtlas) {
        NSLog(@"Renderer not initialized or no font atlas");
        return false;
    }

    @autoreleasepool {
        // Get next drawable
        id<CAMetalDrawable> drawable = [m_impl->metalLayer nextDrawable];
        if (!drawable) {
            return false;
        }

        // Build vertex buffer from TextGrid
        buildVertexBuffer(textGrid);

        // Update LORES palette buffer if needed
        if (m_impl->loresPaletteBuffer) {
            // Get palette manager from API context
            auto paletteMgr = STApi::Context::instance().loresPalette();
            if (paletteMgr) {
                // Always upload on first frame or when dirty
                static bool firstUpload = true;
                if (firstUpload || paletteMgr->isDirty()) {
                    NSLog(@"[LORES] Uploading palette buffer (firstUpload=%d, dirty=%d)", firstUpload, paletteMgr->isDirty());

                    // Convert uint8_t RGBA palette to float4
                    const uint8_t* paletteData = paletteMgr->getPaletteData();
                    float* bufferData = (float*)[m_impl->loresPaletteBuffer contents];

                    for (int row = 0; row < 32; row++) {
                        for (int col = 0; col < 16; col++) {
                            int srcIdx = (row * 16 + col) * 4;
                            int dstIdx = (row * 16 + col) * 4;
                            bufferData[dstIdx + 0] = paletteData[srcIdx + 0] / 255.0f; // R
                            bufferData[dstIdx + 1] = paletteData[srcIdx + 1] / 255.0f; // G
                            bufferData[dstIdx + 2] = paletteData[srcIdx + 2] / 255.0f; // B
                            bufferData[dstIdx + 3] = paletteData[srcIdx + 3] / 255.0f; // A
                        }
                    }

                    // Debug: Log first palette entry
                    NSLog(@"[LORES] Palette[0][0] = RGBA(%.2f, %.2f, %.2f, %.2f)",
                          bufferData[0], bufferData[1], bufferData[2], bufferData[3]);
                    NSLog(@"[LORES] Palette[0][15] = RGBA(%.2f, %.2f, %.2f, %.2f)",
                          bufferData[15*4], bufferData[15*4+1], bufferData[15*4+2], bufferData[15*4+3]);

                    paletteMgr->clearDirty();
                    firstUpload = false;
                }
            }
        }

        if (m_impl->vertices.empty()) {
            // Nothing to render - clear and present
            id<MTLCommandBuffer> commandBuffer = [m_impl->commandQueue commandBuffer];
            MTLRenderPassDescriptor* renderPass = [MTLRenderPassDescriptor renderPassDescriptor];
            renderPass.colorAttachments[0].texture = drawable.texture;
            renderPass.colorAttachments[0].loadAction = MTLLoadActionClear;
            renderPass.colorAttachments[0].storeAction = MTLStoreActionStore;
            renderPass.colorAttachments[0].clearColor = MTLClearColorMake(0.5, 0.5, 0.5, 1.0);

            id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPass];
            [encoder endEncoding];
            [commandBuffer presentDrawable:drawable];
            [commandBuffer commit];
            return true;
        }

        // Create or update vertex buffer
        NSUInteger vertexBufferSize = m_impl->vertices.size() * sizeof(TextVertex);
        if (!m_impl->vertexBuffer || m_impl->vertexBuffer.length < vertexBufferSize) {
            m_impl->vertexBuffer = [m_impl->device newBufferWithLength:vertexBufferSize
                                                               options:MTLResourceStorageModeShared];
        }

        std::memcpy([m_impl->vertexBuffer contents], m_impl->vertices.data(), vertexBufferSize);

        // Update uniforms
        updateUniforms(deltaTime);

        // Create command buffer
        id<MTLCommandBuffer> commandBuffer = [m_impl->commandQueue commandBuffer];

        // Create render pass
        MTLRenderPassDescriptor* renderPass = [MTLRenderPassDescriptor renderPassDescriptor];
        renderPass.colorAttachments[0].texture = drawable.texture;
        renderPass.colorAttachments[0].loadAction = MTLLoadActionClear;
        renderPass.colorAttachments[0].storeAction = MTLStoreActionStore;
        renderPass.colorAttachments[0].clearColor = MTLClearColorMake(0.5, 0.5, 0.5, 1.0);

        // Create render encoder
        id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPass];

        [encoder setRenderPipelineState:m_impl->pipelineState];

        // Bind vertex buffer (interleaved)
        [encoder setVertexBuffer:m_impl->vertexBuffer offset:0 atIndex:0];

        // Bind uniforms
        [encoder setVertexBuffer:m_impl->uniformBuffer offset:0 atIndex:1];

        // Bind font atlas texture
        id<MTLTexture> atlasTexture = (id<MTLTexture>)m_impl->fontAtlas->getTexture();
        [encoder setFragmentTexture:atlasTexture atIndex:0];
        [encoder setFragmentSamplerState:m_impl->samplerState atIndex:0];

        // Draw normal text
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                    vertexStart:0
                    vertexCount:m_impl->vertices.size()];

        // Render sextants separately if any exist
        if (!m_impl->sextantVertices.empty() && m_impl->sextantPipelineState) {
            NSLog(@"[LORES] Rendering %zu sextant vertices", m_impl->sextantVertices.size());

            // Create or update sextant vertex buffer
            NSUInteger sextantBufferSize = m_impl->sextantVertices.size() * sizeof(TextVertex);
            if (!m_impl->sextantVertexBuffer || m_impl->sextantVertexBuffer.length < sextantBufferSize) {
                m_impl->sextantVertexBuffer = [m_impl->device newBufferWithLength:sextantBufferSize
                                                                          options:MTLResourceStorageModeShared];
            }
            std::memcpy([m_impl->sextantVertexBuffer contents], m_impl->sextantVertices.data(), sextantBufferSize);

            // Debug: Log first vertex
            if (m_impl->sextantVertices.size() > 0) {
                auto& v = m_impl->sextantVertices[0];
                NSLog(@"[LORES] First vertex: pos=(%.1f,%.1f) color=(%.3f,%.3f,%.3f,%.3f)",
                      v.position[0], v.position[1], v.color[0], v.color[1], v.color[2], v.color[3]);
            }

            // Switch to sextant pipeline
            [encoder setRenderPipelineState:m_impl->sextantPipelineState];
            [encoder setVertexBuffer:m_impl->sextantVertexBuffer offset:0 atIndex:0];
            [encoder setVertexBuffer:m_impl->uniformBuffer offset:0 atIndex:1];
            [encoder setFragmentBuffer:m_impl->loresPaletteBuffer offset:0 atIndex:2];
            [encoder setFragmentTexture:atlasTexture atIndex:0];
            [encoder setFragmentSamplerState:m_impl->samplerState atIndex:0];

            NSLog(@"[LORES] Drawing sextants...");
            // Draw sextants
            [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                        vertexStart:0
                        vertexCount:m_impl->sextantVertices.size()];
        }

        // --- TEXT DISPLAY LAYER (OVERLAY) ---
        // Render transformed overlay text on top of everything else
        if (m_impl->textDisplayManager && m_impl->textDisplayManager->hasContent()) {
            // Build vertex buffers for current viewport
            m_impl->textDisplayManager->buildVertexBuffers(m_impl->viewportWidth, m_impl->viewportHeight);

            // Check if effects are present and choose appropriate pipeline
            bool hasEffects = m_impl->textDisplayManager->hasEffects();
            if (hasEffects && m_impl->textEffectsPipelineState) {
                // Use text effects pipeline
                [encoder setRenderPipelineState:m_impl->textEffectsPipelineState];
            } else {
                // Use transformed text pipeline (fixed 1920x1080 coordinates)
                [encoder setRenderPipelineState:m_impl->transformedTextPipelineState];
            }

            [encoder setVertexBuffer:m_impl->uniformBuffer offset:0 atIndex:1];
            [encoder setFragmentTexture:m_impl->fontAtlas->getTexture() atIndex:0];
            [encoder setFragmentSamplerState:m_impl->samplerState atIndex:0];

            // Render transformed text
            m_impl->textDisplayManager->render(encoder, m_impl->viewportWidth, m_impl->viewportHeight);
        }

        [encoder endEncoding];

        // Present drawable with proper synchronization
        [commandBuffer presentDrawable:drawable];
        [commandBuffer commit];

        return true;
    }
}

MTLDevicePtr MetalRenderer::getDevice() const {
    return m_impl->device;
}

MTLCommandQueuePtr MetalRenderer::getCommandQueue() const {
    return m_impl->commandQueue;
}

bool MetalRenderer::isInitialized() const {
    return m_impl->initialized;
}

uint32_t MetalRenderer::getCellWidth() const {
    return m_impl->cellWidth;
}

uint32_t MetalRenderer::getCellHeight() const {
    return m_impl->cellHeight;
}

bool MetalRenderer::renderFrame(TextGrid& textGrid, GraphicsLayer* graphicsLayer, float deltaTime) {
    if (!m_impl->initialized || !m_impl->fontAtlas) {
        return false;
    }

    @autoreleasepool {
        // Get next drawable
        id<CAMetalDrawable> drawable = [m_impl->metalLayer nextDrawable];
        if (!drawable) {
            return false;
        }

        // Build vertex buffer from TextGrid
        buildVertexBuffer(textGrid);

        // Create or update vertex buffer
        if (!m_impl->vertices.empty()) {
            NSUInteger vertexBufferSize = m_impl->vertices.size() * sizeof(TextVertex);
            if (!m_impl->vertexBuffer || m_impl->vertexBuffer.length < vertexBufferSize) {
                m_impl->vertexBuffer = [m_impl->device newBufferWithLength:vertexBufferSize
                                                                   options:MTLResourceStorageModeShared];
            }
            std::memcpy([m_impl->vertexBuffer contents], m_impl->vertices.data(), vertexBufferSize);
        }

        // Update uniforms
        updateUniforms(deltaTime);

        // Create command buffer
        id<MTLCommandBuffer> commandBuffer = [m_impl->commandQueue commandBuffer];

        // Create render pass
        MTLRenderPassDescriptor* renderPass = [MTLRenderPassDescriptor renderPassDescriptor];
        renderPass.colorAttachments[0].texture = drawable.texture;
        renderPass.colorAttachments[0].loadAction = MTLLoadActionClear;
        renderPass.colorAttachments[0].storeAction = MTLStoreActionStore;
        renderPass.colorAttachments[0].clearColor = MTLClearColorMake(0.5, 0.5, 0.5, 1.0);

        // Create render encoder
        id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPass];

        // Render text if we have vertices
        if (!m_impl->vertices.empty()) {
            [encoder setRenderPipelineState:m_impl->pipelineState];
            [encoder setVertexBuffer:m_impl->vertexBuffer offset:0 atIndex:0];
            [encoder setVertexBuffer:m_impl->uniformBuffer offset:0 atIndex:1];

            id<MTLTexture> atlasTexture = (id<MTLTexture>)m_impl->fontAtlas->getTexture();
            [encoder setFragmentTexture:atlasTexture atIndex:0];
            [encoder setFragmentSamplerState:m_impl->samplerState atIndex:0];

            [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                        vertexStart:0
                        vertexCount:m_impl->vertices.size()];
        }

        // Render graphics layer with dirty tracking optimization
        if (graphicsLayer && m_impl->graphicsRenderer) {
            // Check if layer is visible
            if (!graphicsLayer->isVisible()) {
                // Skip rendering entirely if not visible
            } else {
                // Check if we have new commands or layer is dirty
                bool hasCommands = !graphicsLayer->isEmpty();
                bool layerDirty = graphicsLayer->isDirty();
                bool rendererDirty = m_impl->graphicsRenderer->isDirty();

                // Only re-render if layer has changed
                if (layerDirty || rendererDirty) {
                    if (hasCommands) {
                        // Render new commands to back buffer and swap
                        m_impl->graphicsRenderer->render(*graphicsLayer);

                        // Mark layer as clean after rendering
                        graphicsLayer->clearDirty();
                        m_impl->graphicsRenderer->markClean();
                    }
                }

                // Always composite cached texture if we have content (cheap operation)
                id<MTLTexture> graphicsTexture = m_impl->graphicsRenderer->getTexture();
                bool hasContent = m_impl->graphicsRenderer->hasContent();

                // NSLog(@"[MetalRenderer] Graphics compositing check: hasContent=%d, texture=%p, pipeline=%p",
                //       hasContent, graphicsTexture, m_impl->primitivesPipelineState);

                if (hasContent && graphicsTexture && m_impl->primitivesPipelineState) {
                    NSLog(@"[MetalRenderer] Compositing graphics texture to screen (fullscreen quad)");
                    [encoder setRenderPipelineState:m_impl->primitivesPipelineState];

                    // Create full-screen quad vertices (matches GraphicsVertex shader)
                    struct QuadVertex {
                        float position[2];
                        float texCoord[2];
                    } quadVertices[6] = {
                        {{-1.0f, -1.0f}, {0, 1}},
                        {{ 1.0f, -1.0f}, {1, 1}},
                        {{-1.0f,  1.0f}, {0, 0}},
                        {{ 1.0f, -1.0f}, {1, 1}},
                        {{ 1.0f,  1.0f}, {1, 0}},
                        {{-1.0f,  1.0f}, {0, 0}}
                    };

                    [encoder setVertexBytes:quadVertices length:sizeof(quadVertices) atIndex:0];
                    [encoder setVertexBuffer:m_impl->uniformBuffer offset:0 atIndex:1];
                    [encoder setFragmentTexture:graphicsTexture atIndex:0];
                    [encoder setFragmentSamplerState:m_impl->samplerState atIndex:0];

                    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
                    NSLog(@"[MetalRenderer] Graphics quad drawn successfully");
                } else {
                    if (!hasContent) {
                        NSLog(@"[MetalRenderer] Skipping graphics: no content (hasContent=false)");
                    } else if (!graphicsTexture) {
                        NSLog(@"[MetalRenderer] Skipping graphics: texture is nil");
                    } else if (!m_impl->primitivesPipelineState) {
                        NSLog(@"[MetalRenderer] Skipping graphics: pipeline state is nil");
                    }
                }
            }
        }

        // --- TEXT DISPLAY LAYER (OVERLAY) ---
        // Render transformed overlay text on top of everything else
        if (m_impl->textDisplayManager && m_impl->textDisplayManager->hasContent()) {
            // Build vertex buffers for current viewport
            m_impl->textDisplayManager->buildVertexBuffers(m_impl->viewportWidth, m_impl->viewportHeight);

            // Check if effects are present and choose appropriate pipeline
            bool hasEffects = m_impl->textDisplayManager->hasEffects();
            if (hasEffects && m_impl->textEffectsPipelineState) {
                // Use text effects pipeline
                [encoder setRenderPipelineState:m_impl->textEffectsPipelineState];
            } else {
                // Use transformed text pipeline (fixed 1920x1080 coordinates)
                [encoder setRenderPipelineState:m_impl->transformedTextPipelineState];
            }

            [encoder setVertexBuffer:m_impl->uniformBuffer offset:0 atIndex:1];
            [encoder setFragmentTexture:m_impl->fontAtlas->getTexture() atIndex:0];
            [encoder setFragmentSamplerState:m_impl->samplerState atIndex:0];

            // Render transformed text
            m_impl->textDisplayManager->render(encoder, m_impl->viewportWidth, m_impl->viewportHeight);
        }

        [encoder endEncoding];

        // Present drawable with proper synchronization
        [commandBuffer presentDrawable:drawable];
        [commandBuffer commit];

        return true;
    }
}

bool MetalRenderer::renderFrame(TextGrid& textGrid, GraphicsLayer* graphicsLayer,
                                 SpriteManager* spriteManager, float deltaTime) {
    if (!m_impl->initialized || !m_impl->fontAtlas) {
        return false;
    }

    @autoreleasepool {
        // Get next drawable
        id<CAMetalDrawable> drawable = [m_impl->metalLayer nextDrawable];
        if (!drawable) {
            return false;
        }

        // Build vertex buffer from TextGrid
        buildVertexBuffer(textGrid);

        // Create or update vertex buffer
        if (!m_impl->vertices.empty()) {
            NSUInteger vertexBufferSize = m_impl->vertices.size() * sizeof(TextVertex);
            if (!m_impl->vertexBuffer || m_impl->vertexBuffer.length < vertexBufferSize) {
                m_impl->vertexBuffer = [m_impl->device newBufferWithLength:vertexBufferSize
                                                                   options:MTLResourceStorageModeShared];
            }
            std::memcpy([m_impl->vertexBuffer contents], m_impl->vertices.data(), vertexBufferSize);
        }

        // Update uniforms
        updateUniforms(deltaTime);

        // Create command buffer
        id<MTLCommandBuffer> commandBuffer = [m_impl->commandQueue commandBuffer];

        // Create render pass
        MTLRenderPassDescriptor* renderPass = [MTLRenderPassDescriptor renderPassDescriptor];
        renderPass.colorAttachments[0].texture = drawable.texture;
        renderPass.colorAttachments[0].loadAction = MTLLoadActionClear;
        renderPass.colorAttachments[0].storeAction = MTLStoreActionStore;
        renderPass.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.125, 1.0);

        // Create render encoder
        id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPass];

        // Render text if we have vertices
        if (!m_impl->vertices.empty()) {
            // NSLog(@"[LORES DEBUG] Rendering %zu text vertices", m_impl->vertices.size());
            [encoder setRenderPipelineState:m_impl->pipelineState];
            [encoder setVertexBuffer:m_impl->vertexBuffer offset:0 atIndex:0];
            [encoder setVertexBuffer:m_impl->uniformBuffer offset:0 atIndex:1];

            id<MTLTexture> atlasTexture = (id<MTLTexture>)m_impl->fontAtlas->getTexture();
            [encoder setFragmentTexture:atlasTexture atIndex:0];
            [encoder setFragmentSamplerState:m_impl->samplerState atIndex:0];

            [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                        vertexStart:0
                        vertexCount:m_impl->vertices.size()];
        }

        // Render sextants separately if any exist
        // NSLog(@"[LORES DEBUG] Checking sextants: vertices=%zu, pipeline=%@",
        //       m_impl->sextantVertices.size(), m_impl->sextantPipelineState);
        if (!m_impl->sextantVertices.empty() && m_impl->sextantPipelineState) {
            // NSLog(@"[LORES DEBUG] RENDERING %zu SEXTANT VERTICES", m_impl->sextantVertices.size());
            NSLog(@"[RENDER] Rendering %zu sextant vertices with sextant pipeline", m_impl->sextantVertices.size());

            // Create or update sextant vertex buffer
            NSUInteger sextantBufferSize = m_impl->sextantVertices.size() * sizeof(TextVertex);
            if (!m_impl->sextantVertexBuffer || m_impl->sextantVertexBuffer.length < sextantBufferSize) {
                m_impl->sextantVertexBuffer = [m_impl->device newBufferWithLength:sextantBufferSize
                                                                          options:MTLResourceStorageModeShared];
            }
            std::memcpy([m_impl->sextantVertexBuffer contents], m_impl->sextantVertices.data(), sextantBufferSize);

            // Switch to sextant pipeline
            [encoder setRenderPipelineState:m_impl->sextantPipelineState];
            [encoder setVertexBuffer:m_impl->sextantVertexBuffer offset:0 atIndex:0];
            [encoder setVertexBuffer:m_impl->uniformBuffer offset:0 atIndex:1];
            [encoder setFragmentBuffer:m_impl->loresPaletteBuffer offset:0 atIndex:2];

            id<MTLTexture> atlasTexture = (id<MTLTexture>)m_impl->fontAtlas->getTexture();
            [encoder setFragmentTexture:atlasTexture atIndex:0];
            [encoder setFragmentSamplerState:m_impl->samplerState atIndex:0];

            // Draw sextants
            [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                        vertexStart:0
                        vertexCount:m_impl->sextantVertices.size()];

            NSLog(@"[RENDER] Sextant draw complete");
        }

        // Render graphics layer with dirty tracking optimization
        if (graphicsLayer && m_impl->graphicsRenderer) {
            // Check if layer is visible
            if (!graphicsLayer->isVisible()) {
                // Skip rendering entirely if not visible
            } else {
                // Check if we have new commands or layer is dirty
                bool hasCommands = !graphicsLayer->isEmpty();
                bool layerDirty = graphicsLayer->isDirty();
                bool rendererDirty = m_impl->graphicsRenderer->isDirty();

                // Only re-render if layer has changed
                if (layerDirty || rendererDirty) {
                    if (hasCommands) {
                        // Render new commands to back buffer and swap
                        m_impl->graphicsRenderer->render(*graphicsLayer);

                        // Mark layer as clean after rendering
                        graphicsLayer->clearDirty();
                        m_impl->graphicsRenderer->markClean();
                    }
                }

                // Always composite cached texture if we have content (cheap operation)
                id<MTLTexture> graphicsTexture = m_impl->graphicsRenderer->getTexture();
                bool hasContent = m_impl->graphicsRenderer->hasContent();

                if (hasContent && graphicsTexture && m_impl->primitivesPipelineState) {
                    [encoder setRenderPipelineState:m_impl->primitivesPipelineState];

                    // Create full-screen quad vertices (matches GraphicsVertex shader)
                    struct QuadVertex {
                        float position[2];
                        float texCoord[2];
                    } quadVertices[6] = {
                        {{-1.0f, -1.0f}, {0, 1}},
                        {{ 1.0f, -1.0f}, {1, 1}},
                        {{-1.0f,  1.0f}, {0, 0}},
                        {{ 1.0f, -1.0f}, {1, 1}},
                        {{ 1.0f,  1.0f}, {1, 0}},
                        {{-1.0f,  1.0f}, {0, 0}}
                    };

                    [encoder setVertexBytes:quadVertices length:sizeof(quadVertices) atIndex:0];
                    [encoder setVertexBuffer:m_impl->uniformBuffer offset:0 atIndex:1];
                    [encoder setFragmentTexture:graphicsTexture atIndex:0];
                    [encoder setFragmentSamplerState:m_impl->samplerState atIndex:0];

                    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
                }
            }
        }

        // Render sprites with dirty tracking optimization
        if (spriteManager) {
            // Sprite rendering is lightweight - just draw calls
            // The vertex buffer is built once and reused when sprites don't change
            spriteManager->render(encoder,
                                 m_impl->viewportWidth,
                                 m_impl->viewportHeight);

            // Clear dirty flag after render
            if (spriteManager->isDirty()) {
                spriteManager->clearDirty();
            }
        }

        // Render particles using GPU instanced quads (v1-style)
        bool systemReady = st_particle_system_is_ready();
        bool hasPipeline = (m_impl->particlePipelineState != nil);
        NSLog(@"[PARTICLE DEBUG] systemReady=%d hasPipeline=%d", systemReady, hasPipeline);

        if (systemReady && hasPipeline) {
            const void* particleData = nullptr;
            uint32_t particleCount = 0;

            NSLog(@"[PARTICLE RENDER] System ready, checking for particles...");
            if (st_particle_get_render_data(&particleData, &particleCount) && particleCount > 0) {
                const SuperTerminal::Particle* particles = static_cast<const SuperTerminal::Particle*>(particleData);
                NSLog(@"[PARTICLE RENDER] Got %d particles to render", particleCount);

                // Get sprite texture from first particle's source sprite (if needed)
                id<MTLTexture> spriteTexture = nil;
                uint16_t sourceSprite = particles[0].sourceSprite;
                bool useTexture = (particles[0].mode == SuperTerminal::ParticleMode::SPRITE_FRAGMENT);
                NSLog(@"[PARTICLE RENDER] First particle sourceSprite: %d, mode: %d, useTexture: %d",
                      sourceSprite, (int)particles[0].mode, useTexture);

                if (useTexture && sourceSprite > 0 && spriteManager) {
                    spriteTexture = (id<MTLTexture>)spriteManager->getSpriteTexture(sourceSprite);
                    NSLog(@"[PARTICLE RENDER] Sprite texture: %@", spriteTexture ? @"valid" : @"nil");
                }

                // Prepare instance data
                struct ParticleInstanceData {
                    simd_float2 position;
                    simd_float2 texCoordMin;
                    simd_float2 texCoordMax;
                    simd_float4 color;
                    float scale;
                    float rotation;
                    float alpha;
                };

                size_t instanceDataSize = sizeof(ParticleInstanceData) * particleCount;
                if (!m_impl->particleInstanceBuffer || [m_impl->particleInstanceBuffer length] < instanceDataSize) {
                    m_impl->particleInstanceBuffer = [m_impl->device newBufferWithLength:instanceDataSize
                                                                                  options:MTLResourceStorageModeShared];
                }

                ParticleInstanceData* instanceData = (ParticleInstanceData*)[m_impl->particleInstanceBuffer contents];

                // Populate instance data from particles
                for (uint32_t i = 0; i < particleCount; i++) {
                    const auto& p = particles[i];
                    if (!p.active) continue;

                    if (i < 3) {
                        NSLog(@"[PARTICLE RENDER] Particle %d: pos=(%.1f, %.1f) size=%.2f life=%.2f/%.2f active=%d color=0x%08X",
                              i, p.x, p.y, p.size, p.life, p.maxLife, p.active, p.color);
                    }

                    instanceData[i].position = simd_make_float2(p.x, p.y);
                    instanceData[i].texCoordMin = simd_make_float2(p.texCoordMinX, p.texCoordMinY);
                    instanceData[i].texCoordMax = simd_make_float2(p.texCoordMaxX, p.texCoordMaxY);

                    // Unpack color
                    uint32_t packed = p.color;
                    float r = ((packed >> 24) & 0xFF) / 255.0f;
                    float g = ((packed >> 16) & 0xFF) / 255.0f;
                    float b = ((packed >> 8) & 0xFF) / 255.0f;
                    float a = (packed & 0xFF) / 255.0f;
                    instanceData[i].color = simd_make_float4(r, g, b, a);

                    instanceData[i].scale = p.size * p.scaleMultiplier; // Use particle's custom scale multiplier
                    instanceData[i].rotation = p.rotation; // Use actual rotation from particle
                    instanceData[i].alpha = p.life / p.maxLife; // Fade out as life decreases (1.0 = new, 0.0 = dead)
                }

                // Create base quad vertices (unit quad -1 to 1)
                struct ParticleVertex {
                    simd_float2 position;
                    simd_float2 texCoord;
                };

                ParticleVertex quadVertices[6] = {
                    {{-1.0f, -1.0f}, {0.0f, 1.0f}},
                    {{ 1.0f, -1.0f}, {1.0f, 1.0f}},
                    {{-1.0f,  1.0f}, {0.0f, 0.0f}},
                    {{ 1.0f, -1.0f}, {1.0f, 1.0f}},
                    {{ 1.0f,  1.0f}, {1.0f, 0.0f}},
                    {{-1.0f,  1.0f}, {0.0f, 0.0f}}
                };

                // Set up rendering
                [encoder setRenderPipelineState:m_impl->particlePipelineState];
                [encoder setVertexBytes:quadVertices length:sizeof(quadVertices) atIndex:0];
                [encoder setVertexBytes:&m_impl->uniforms length:sizeof(TextUniforms) atIndex:1];
                [encoder setVertexBuffer:m_impl->particleInstanceBuffer offset:0 atIndex:2];

                // Bind sprite texture and sampler (if using texture mode)
                if (useTexture) {
                    if (spriteTexture) {
                        [encoder setFragmentTexture:spriteTexture atIndex:0];
                        [encoder setFragmentSamplerState:m_impl->samplerState atIndex:0];
                        NSLog(@"[PARTICLE RENDER] Using texture mode with sprite texture");
                    } else {
                        NSLog(@"[PARTICLE] Warning: SPRITE_FRAGMENT mode but no texture for sprite %d - skipping", sourceSprite);
                    }
                } else {
                    // POINT_SPRITE mode - no texture needed, will use particle colors
                    NSLog(@"[PARTICLE RENDER] Using POINT_SPRITE mode (no texture)");
                }

                // Draw particles (works for both modes)
                [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                            vertexStart:0
                            vertexCount:6
                          instanceCount:particleCount];
                NSLog(@"[PARTICLE RENDER] Drew %d particle instances", particleCount);
            } else {
                NSLog(@"[PARTICLE RENDER] No particles to render (count=%d)", particleCount);
            }
        } else {
            if (!st_particle_system_is_ready()) {
                NSLog(@"[PARTICLE RENDER] System not ready");
            }
            if (!m_impl->particlePipelineState) {
                NSLog(@"[PARTICLE RENDER] No pipeline state");
            }
        }

        // --- TEXT DISPLAY LAYER (OVERLAY) ---
        // Render transformed overlay text on top of everything else
        if (m_impl->textDisplayManager && m_impl->textDisplayManager->hasContent()) {
            // Build vertex buffers for current viewport
            m_impl->textDisplayManager->buildVertexBuffers(m_impl->viewportWidth, m_impl->viewportHeight);

            // Check if effects are present and choose appropriate pipeline
            bool hasEffects = m_impl->textDisplayManager->hasEffects();
            if (hasEffects && m_impl->textEffectsPipelineState) {
                // Use text effects pipeline
                [encoder setRenderPipelineState:m_impl->textEffectsPipelineState];
            } else {
                // Use transformed text pipeline (fixed 1920x1080 coordinates)
                [encoder setRenderPipelineState:m_impl->transformedTextPipelineState];
            }

            [encoder setVertexBuffer:m_impl->uniformBuffer offset:0 atIndex:1];
            [encoder setFragmentTexture:m_impl->fontAtlas->getTexture() atIndex:0];
            [encoder setFragmentSamplerState:m_impl->samplerState atIndex:0];

            // Render transformed text
            m_impl->textDisplayManager->render(encoder, m_impl->viewportWidth, m_impl->viewportHeight);
        }

        [encoder endEncoding];

        // Present drawable with proper synchronization
        [commandBuffer presentDrawable:drawable];
        [commandBuffer commit];

        return true;
    }
}

bool MetalRenderer::renderFrame(TextGrid& textGrid, GraphicsLayer* graphicsLayer,
                                 SpriteManager* spriteManager,
                                 RectangleManager* rectangleManager,
                                 CircleManager* circleManager,
                                 LineManager* lineManager,
                                 PolygonManager* polygonManager,
                                 StarManager* starManager,
                                 TilemapManager* tilemapManager, TilemapRenderer* tilemapRenderer,
                                 float deltaTime) {
    if (!m_impl->initialized || !m_impl->fontAtlas) {
        return false;
    }

    @autoreleasepool {
        // Wait for previous frame to finish (prevents GPU overrun)
        dispatch_semaphore_wait(m_impl->inflightSemaphore, DISPATCH_TIME_FOREVER);

        // Get next drawable
        id<CAMetalDrawable> drawable = [m_impl->metalLayer nextDrawable];
        if (!drawable) {
            static std::atomic<int> nilDrawableCount(0);
            int count = ++nilDrawableCount;
            if (count <= 10 || count % 60 == 0) {
                NSLog(@"[MetalRenderer] nextDrawable returned nil (count: %d)", count);
            }
            // Signal semaphore if we can't get drawable
            dispatch_semaphore_signal(m_impl->inflightSemaphore);
            return false;
        }

        static bool loggedSizeOnce = false;
        if (!loggedSizeOnce) {
            NSLog(@"[MetalRenderer] Drawable texture size: %lu x %lu",
                  (unsigned long)drawable.texture.width,
                  (unsigned long)drawable.texture.height);
            if (m_impl->graphicsRenderer) {
                id<MTLTexture> gfxTex = m_impl->graphicsRenderer->getTexture();
                if (gfxTex) {
                    NSLog(@"[MetalRenderer] Graphics texture size: %lu x %lu",
                          (unsigned long)gfxTex.width,
                          (unsigned long)gfxTex.height);
                }
            }
            loggedSizeOnce = true;
        }

        // Build vertex buffer from TextGrid only if dirty (optimization)
        if (textGrid.isDirty()) {
            buildVertexBuffer(textGrid);

            // Create or update vertex buffer
            if (!m_impl->vertices.empty()) {
                NSUInteger vertexBufferSize = m_impl->vertices.size() * sizeof(TextVertex);
                if (!m_impl->vertexBuffer || m_impl->vertexBuffer.length < vertexBufferSize) {
                    m_impl->vertexBuffer = [m_impl->device newBufferWithLength:vertexBufferSize
                                                                       options:MTLResourceStorageModeShared];
                }
                std::memcpy([m_impl->vertexBuffer contents], m_impl->vertices.data(), vertexBufferSize);
            }

            // Clear dirty flag after successful update
            textGrid.clearDirty();

            // Vertex buffer rebuilt tracking disabled (too noisy)
        }

        // Update LORES palette buffer if needed (300 palettes for HIRES support)
        if (m_impl->loresPaletteBuffer) {
            auto paletteMgr = STApi::Context::instance().loresPalette();
            if (paletteMgr) {
                static bool firstUpload = true;
                if (firstUpload || paletteMgr->isDirty()) {
                    const uint8_t* paletteData = paletteMgr->getPaletteData();
                    float* bufferData = (float*)[m_impl->loresPaletteBuffer contents];

                    // Upload all 300 palettes (supports LORES/MIDRES/HIRES)
                    for (int row = 0; row < 300; row++) {
                        for (int col = 0; col < 16; col++) {
                            int srcIdx = (row * 16 + col) * 4;
                            int dstIdx = (row * 16 + col) * 4;
                            bufferData[dstIdx + 0] = paletteData[srcIdx + 0] / 255.0f; // R
                            bufferData[dstIdx + 1] = paletteData[srcIdx + 1] / 255.0f; // G
                            bufferData[dstIdx + 2] = paletteData[srcIdx + 2] / 255.0f; // B
                            bufferData[dstIdx + 3] = paletteData[srcIdx + 3] / 255.0f; // A
                        }
                    }

                    paletteMgr->clearDirty();
                    firstUpload = false;
                }
            }
        }

        // Update XRES palette buffer if needed (hybrid: 240 rows × 16 per-row + 240 global)
        if (m_impl->xresPaletteBuffer) {
            auto xresPaletteMgr = STApi::Context::instance().xresPalette();
            if (xresPaletteMgr) {
                static bool firstXResUpload = true;
                if (firstXResUpload || xresPaletteMgr->isDirty()) {
                    const float* floatData = xresPaletteMgr->getPaletteDataFloat();
                    size_t dataSize = xresPaletteMgr->getPaletteDataSize();

                    // Copy float data directly to GPU buffer
                    memcpy([m_impl->xresPaletteBuffer contents], floatData, dataSize);

                    // Log palette data for colors we're using (64, 96, 128, 160, 192, 200, 255)
                    static bool loggedPalette = false;
                    if (!loggedPalette) {
                        NSLog(@"[XRES PALETTE] Checking global palette colors:");
                        for (int idx : {64, 96, 128, 160, 192, 200, 255}) {
                            // Global colors are at offset: 240 rows * 16 colors/row + (idx - 16)
                            // Each color is 4 floats (RGBA)
                            int offset = (240 * 16 + (idx - 16)) * 4;
                            NSLog(@"  Color %d: R=%.2f G=%.2f B=%.2f A=%.2f",
                                  idx, floatData[offset], floatData[offset+1],
                                  floatData[offset+2], floatData[offset+3]);
                        }
                        loggedPalette = true;
                    }

                    xresPaletteMgr->clearDirty();
                    firstXResUpload = false;
                }
            }
        }

        // Update WRES palette buffer if needed (hybrid: 240 rows × 16 per-row + 240 global)
        if (m_impl->wresPaletteBuffer) {
            auto wresPaletteMgr = STApi::Context::instance().wresPalette();
            if (wresPaletteMgr) {
                static bool firstWResUpload = true;
                if (firstWResUpload || wresPaletteMgr->isDirty()) {
                    const float* floatData = wresPaletteMgr->getPaletteDataFloat();
                    size_t dataSize = wresPaletteMgr->getPaletteDataSize();

                    // Copy float data directly to GPU buffer
                    memcpy([m_impl->wresPaletteBuffer contents], floatData, dataSize);

                    wresPaletteMgr->clearDirty();
                    firstWResUpload = false;
                }
            }
        }

        // Update PRES palette buffer if needed (hybrid: 720 rows × 16 per-row + 240 global)
        if (m_impl->presPaletteBuffer) {
            auto presPaletteMgr = STApi::Context::instance().presPalette();
            if (presPaletteMgr) {
                static bool firstPResUpload = true;
                if (firstPResUpload || presPaletteMgr->isDirty()) {
                    const float* floatData = presPaletteMgr->getPaletteDataFloat();
                    size_t dataSize = presPaletteMgr->getPaletteDataSize();

                    // Copy float data directly to GPU buffer
                    memcpy([m_impl->presPaletteBuffer contents], floatData, dataSize);

                    presPaletteMgr->clearDirty();
                    firstPResUpload = false;
                }
            }
        }

        // Update LORES or URES texture if needed
        auto displayMgr = STApi::Context::instance().display();

        // Check for URES mode first (highest priority)
        if (displayMgr && displayMgr->isUResMode() && m_impl->uresTexture) {
            // NSLog(@"[URES] Checking URES buffer...");
            auto uresBuffer = displayMgr->getUResBuffer();
            if (uresBuffer && uresBuffer->isDirty()) {
                NSLog(@"[URES] Buffer is dirty, locking mutex...");
                std::lock_guard<std::mutex> lock(uresBuffer->getMutex());

                NSLog(@"[URES] Mutex locked, getting buffer data...");
                int width = uresBuffer->getWidth();
                int height = uresBuffer->getHeight();

                const uint16_t* pixelData = uresBuffer->getPixelData();
                NSLog(@"[URES] Starting texture upload (%dx%d)...", width, height);
                MTLRegion region = MTLRegionMake2D(0, 0, width, height);
                [m_impl->uresTexture replaceRegion:region
                                        mipmapLevel:0
                                          withBytes:pixelData
                                        bytesPerRow:width * sizeof(uint16_t)];

                NSLog(@"[URES] Texture upload complete, clearing dirty flag...");
                uresBuffer->clearDirty();
                NSLog(@"[URES] URES buffer update complete");
            }
        }
        // Check for XRES mode (Mode X: 320×240 with 256-color hybrid palette)
        else if (displayMgr && displayMgr->isXResMode() && m_impl->xresTextures[0]) {
            static bool loggedXResCheck = false;
            if (!loggedXResCheck) {
                NSLog(@"[XRES DEBUG] Checking XRES buffers for uploads...");
                loggedXResCheck = true;
            }

            // Upload ALL dirty buffers to their corresponding GPU textures
            for (int bufferIndex = 0; bufferIndex < 4; bufferIndex++) {
                auto xresBuffer = displayMgr->getXResBuffer(bufferIndex);

                static bool loggedBufferState[4] = {false, false, false, false};
                if (!loggedBufferState[bufferIndex]) {
                    NSLog(@"[XRES DEBUG] Buffer[%d]: ptr=%p, isDirty=%d",
                          bufferIndex, xresBuffer.get(), xresBuffer ? xresBuffer->isDirty() : -1);
                    loggedBufferState[bufferIndex] = true;
                }

                if (xresBuffer && xresBuffer->isDirty()) {
                    std::lock_guard<std::mutex> lock(xresBuffer->getMutex());

                    int width = xresBuffer->getWidth();
                    int height = xresBuffer->getHeight();

                    const uint8_t* pixelData = xresBuffer->getPixelData();
                    MTLRegion region = MTLRegionMake2D(0, 0, width, height);

                    // Check pixel data (first 100 pixels)
                    static bool loggedPixelData = false;
                    if (!loggedPixelData) {
                        int nonZeroCount = 0;
                        for (int i = 0; i < 100; i++) {
                            if (pixelData[i] != 0) nonZeroCount++;
                        }
                        NSLog(@"[XRES PIXELS] First 100 pixels: %d non-zero. Sample: [0]=%d [10]=%d [50]=%d [99]=%d",
                              nonZeroCount, pixelData[0], pixelData[10], pixelData[50], pixelData[99]);
                        loggedPixelData = true;
                    }

                    // Upload to corresponding GPU texture
                    [m_impl->xresTextures[bufferIndex] replaceRegion:region
                                                mipmapLevel:0
                                                  withBytes:pixelData
                                                bytesPerRow:width];
                    xresBuffer->clearDirty();

                    static int uploadCount = 0;
                    uploadCount++;
                    NSLog(@"[XRES] Texture[%d] uploaded (%d×%d, upload #%d)", bufferIndex, width, height, uploadCount);
                }
            }
        }

        // Check for WRES mode (Wide Mode X: 432×240 with 256-color hybrid palette)
        else if (displayMgr && displayMgr->isWResMode() && m_impl->wresTextures[0]) {
            // Upload ALL dirty buffers to their corresponding GPU textures
            for (int bufferIndex = 0; bufferIndex < 4; bufferIndex++) {
                auto wresBuffer = displayMgr->getWResBuffer(bufferIndex);
                if (wresBuffer && wresBuffer->isDirty()) {
                    std::lock_guard<std::mutex> lock(wresBuffer->getMutex());

                    int width = wresBuffer->getWidth();
                    int height = wresBuffer->getHeight();

                    const uint8_t* pixelData = wresBuffer->getPixelData();
                    MTLRegion region = MTLRegionMake2D(0, 0, width, height);

                    // Upload to corresponding GPU texture
                    [m_impl->wresTextures[bufferIndex] replaceRegion:region
                                                mipmapLevel:0
                                                  withBytes:pixelData
                                                bytesPerRow:width];

                    wresBuffer->clearDirty();

                    static int uploadCount = 0;
                    if (++uploadCount % 60 == 0) {
                        NSLog(@"[WRES] Texture[%d] uploaded (%d×%d, %d times)", bufferIndex, width, height, uploadCount);
                    }
                }
            }


        }

        // Check for PRES mode (Premium Resolution: 1280×720 with 256-color hybrid palette)
        else if (displayMgr && displayMgr->isPResMode() && m_impl->presTextures[0]) {
            // Upload ALL dirty buffers to their corresponding GPU textures
            for (int bufferIndex = 0; bufferIndex < 8; bufferIndex++) {
                auto presBuffer = displayMgr->getPResBuffer(bufferIndex);
                if (presBuffer && presBuffer->isDirty()) {
                    std::lock_guard<std::mutex> lock(presBuffer->getMutex());

                    int width = presBuffer->getWidth();
                    int height = presBuffer->getHeight();

                    const uint8_t* pixelData = presBuffer->getPixelData();
                    MTLRegion region = MTLRegionMake2D(0, 0, width, height);

                    // Upload to corresponding GPU texture
                    [m_impl->presTextures[bufferIndex] replaceRegion:region
                                                mipmapLevel:0
                                                  withBytes:pixelData
                                                bytesPerRow:width];

                    presBuffer->clearDirty();

                    static int uploadCount = 0;
                    if (++uploadCount % 60 == 0) {
                        NSLog(@"[PRES] Texture[%d] uploaded (%d×%d, %d times)", bufferIndex, width, height, uploadCount);
                    }
                }
            }
        }
        // Check for LORES mode (palette-based modes) - upload all 8 buffers
        else if (displayMgr && displayMgr->isLoResMode() && m_impl->loresTextures[0]) {
            // Upload all 8 LORES buffers to their corresponding GPU textures
            for (int bufferIndex = 0; bufferIndex < 8; bufferIndex++) {
                auto loresBuffer = displayMgr->getLoResBuffer(bufferIndex);
                if (loresBuffer && loresBuffer->isDirty()) {
                    // Get current buffer dimensions
                    int width = loresBuffer->getWidth();
                    int height = loresBuffer->getHeight();

                    // Upload pixel data to texture with correct dimensions
                    const uint8_t* pixelData = loresBuffer->getPixelData();
                    MTLRegion region = MTLRegionMake2D(0, 0, width, height);
                    [m_impl->loresTextures[bufferIndex] replaceRegion:region
                                            mipmapLevel:0
                                              withBytes:pixelData
                                            bytesPerRow:width];
                    loresBuffer->clearDirty();

                    static int uploadCount = 0;
                    if (++uploadCount % 60 == 0) {
                        NSLog(@"[LORES] Texture[%d] uploaded (%d×%d, %d times)", bufferIndex, width, height, uploadCount);
                    }
                }
            }

            // Upload LORES palette to GPU
            auto loresPalette = STApi::Context::instance().loresPalette();
            if (loresPalette && m_impl->loresPaletteBuffer) {
                // Get palette buffer pointer
                float* paletteData = (float*)[m_impl->loresPaletteBuffer contents];

                static bool loggedPaletteData = false;

                // Upload all 300 rows × 16 colors
                for (int row = 0; row < 300; row++) {
                    for (int index = 0; index < 16; index++) {
                        uint32_t rgba = loresPalette->getPaletteEntry(row, index);

                        // Log first row palette on first upload
                        if (!loggedPaletteData && row == 0 && index < 4) {
                            NSLog(@"[LORES PALETTE] Row 0, Index %d: RGBA=0x%08X", index, rgba);
                        }

                        // Convert RGBA to float4
                        float r = ((rgba >> 24) & 0xFF) / 255.0f;
                        float g = ((rgba >> 16) & 0xFF) / 255.0f;
                        float b = ((rgba >> 8) & 0xFF) / 255.0f;
                        float a = (rgba & 0xFF) / 255.0f;

                        // Store in buffer: row * 16 * 4 + index * 4
                        int offset = (row * 16 + index) * 4;
                        paletteData[offset + 0] = r;
                        paletteData[offset + 1] = g;
                        paletteData[offset + 2] = b;
                        paletteData[offset + 3] = a;
                    }
                }

                if (!loggedPaletteData) {
                    loggedPaletteData = true;
                    NSLog(@"[LORES PALETTE] First palette upload complete - check if values are non-zero");
                }

                static int paletteUploadCount = 0;
                if (++paletteUploadCount % 60 == 0) {
                    NSLog(@"[LORES] Palette uploaded to GPU (%d times)", paletteUploadCount);
                }
            } else {
                static bool loggedMissing = false;
                if (!loggedMissing) {
                    NSLog(@"[LORES PALETTE] ERROR: loresPalette=%p, paletteBuffer=%p",
                          loresPalette, m_impl->loresPaletteBuffer);
                    loggedMissing = true;
                }
            }
        }

        // Update uniforms
        updateUniforms(deltaTime);

        // Create command buffer
        id<MTLCommandBuffer> commandBuffer = [m_impl->commandQueue commandBuffer];

        // Create render pass with background color clear
        MTLRenderPassDescriptor* renderPass = [MTLRenderPassDescriptor renderPassDescriptor];
        renderPass.colorAttachments[0].texture = drawable.texture;
        renderPass.colorAttachments[0].loadAction = MTLLoadActionClear;
        renderPass.colorAttachments[0].storeAction = MTLStoreActionStore;
        renderPass.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);  // Black background

        // Create render encoder
        id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPass];

        // =================================================================
        // LAYER RENDERING ORDER (with transparency support):
        // 1. Background color (done via clear color above - GPU optimized)
        // 2. BG/FG tile layers (sorted by Z-order)
        // 3. Video mode buffer (LORES/XRES/WRES/URES) - color 0 transparent
        // 4. Shape primitives (rectangles, circles, lines, polygons, stars)
        // 5. Graphics layer
        // 6. Text layer (always renders if content exists)
        // 7. Sprite layer
        // 8. Particles
        // =================================================================

        // --- TILEMAP LAYERS (Background and Foreground) ---
        // Render all enabled tilemap layers in Z-order (lowest to highest)
        // Layers with negative Z are background, positive Z are foreground
        if (tilemapManager && tilemapRenderer && tilemapManager->isInitialized() && tilemapRenderer->isInitialized()) {
            auto camera = tilemapManager->getCamera();
            if (camera) {
                // Begin tilemap rendering frame
                tilemapRenderer->beginFrame(encoder);

                // Get all renderable layers (sorted by Z-order)
                auto layers = tilemapManager->getRenderableLayers();

                // Render each visible layer
                for (const auto& layer : layers) {
                    if (layer && layer->isVisible()) {
                        tilemapRenderer->renderLayer(*layer, *camera, m_impl->elapsedTime);
                    }
                }

                // End tilemap rendering frame
                tilemapRenderer->endFrame();
            }
        }



        // =================================================================
        // VIDEO MODE LAYER (Independent of text mode)
        // Renders if any video mode is active
        // =================================================================

        bool isLoResMode = false;
        bool isUResMode = false;
        bool isXResMode = false;
        bool isWResMode = false;
        if (displayMgr) {
            isLoResMode = displayMgr->isLoResMode();
            isUResMode = displayMgr->isUResMode();
            isXResMode = displayMgr->isXResMode();
            isWResMode = displayMgr->isWResMode();

            static int modeCheckCount = 0;
            if (++modeCheckCount < 5) {
                NSLog(@"[MODE] isLoResMode=%d, isUResMode=%d, isXResMode=%d, isWResMode=%d",
                      isLoResMode, isUResMode, isXResMode, isWResMode);
            }
        }

        // --- XRES MODE ---
        if (isXResMode && m_impl->xresPipelineState && m_impl->xresTextures[0]) {
            // --- XRES MODE: Render Mode X buffer (320×240 with 256-color hybrid palette) ---
            [encoder setRenderPipelineState:m_impl->xresPipelineState];

            // XRES has fixed 320×240 resolution (4:3 aspect ratio, square pixels)
            int pixelWidth = 320;
            int pixelHeight = 240;

            // Get actual drawable dimensions (accounts for Retina scaling)
            float drawableWidth = static_cast<float>(drawable.texture.width);
            float drawableHeight = static_cast<float>(drawable.texture.height);

            // Calculate aspect-corrected quad dimensions
            // Pixels should have 1:1 aspect ratio (square pixels)
            float pixelAspect = static_cast<float>(pixelWidth) / static_cast<float>(pixelHeight);
            float drawableAspect = drawableWidth / drawableHeight;

            float quadWidth = 2.0f;   // Full width in NDC
            float quadHeight = 2.0f;  // Full height in NDC

            // Scale to maintain pixel aspect ratio
            if (drawableAspect > pixelAspect) {
                // Drawable is wider than pixel buffer - letterbox horizontally
                quadWidth = 2.0f * (pixelAspect / drawableAspect);
            } else {
                // Drawable is taller than pixel buffer - letterbox vertically
                quadHeight = 2.0f * (drawableAspect / pixelAspect);
            }

            float left = -quadWidth / 2.0f;
            float right = quadWidth / 2.0f;
            float top = quadHeight / 2.0f;
            float bottom = -quadHeight / 2.0f;

            // Create aspect-corrected quad vertices (NDC)
            struct QuadVertex {
                float position[2];
                float texCoord[2];
            } xresQuad[6] = {
                {{left, bottom}, {0, 1}},   // Bottom-left
                {{right, bottom}, {1, 1}},  // Bottom-right
                {{left, top}, {0, 0}},      // Top-left
                {{right, bottom}, {1, 1}},  // Bottom-right
                {{right, top}, {1, 0}},     // Top-right
                {{left, top}, {0, 0}}       // Top-left
            };

            [encoder setVertexBytes:xresQuad length:sizeof(xresQuad) atIndex:0];
            // Use front buffer (texture 0) for display
            [encoder setFragmentTexture:m_impl->xresTextures[0] atIndex:0];
            [encoder setFragmentBuffer:m_impl->xresPaletteBuffer offset:0 atIndex:2];

            [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];

            static int xresRenderCount = 0;
            if (++xresRenderCount % 60 == 0) {
                NSLog(@"[XRES] Rendering XRES mode (%d×%d, pixelAspect=%.2f, drawableAspect=%.2f)",
                      pixelWidth, pixelHeight, pixelAspect, drawableAspect);
                NSLog(@"[XRES] Drawable: %.0fx%.0f, Viewport: %dx%d",
                      drawableWidth, drawableHeight, m_impl->viewportWidth, m_impl->viewportHeight);
                NSLog(@"[XRES] Quad: left=%.4f, right=%.4f, top=%.4f, bottom=%.4f",
                      left, right, top, bottom);
            }
        }

        // --- WRES MODE ---
        if (isWResMode && m_impl->wresPipelineState && m_impl->wresTextures[0]) {
            // --- WRES MODE: Render Wide Mode X buffer (432×240 with 256-color hybrid palette) ---
            [encoder setRenderPipelineState:m_impl->wresPipelineState];

            // WRES has fixed 432×240 resolution (1.8:1 aspect ratio, square pixels)
            int pixelWidth = 432;
            int pixelHeight = 240;

            // Get actual drawable dimensions (accounts for Retina scaling)
            float drawableWidth = static_cast<float>(drawable.texture.width);
            float drawableHeight = static_cast<float>(drawable.texture.height);

            // Calculate aspect-corrected quad dimensions
            // Pixels should have 1:1 aspect ratio (square pixels)
            float pixelAspect = static_cast<float>(pixelWidth) / static_cast<float>(pixelHeight);
            float drawableAspect = drawableWidth / drawableHeight;

            float quadWidth = 2.0f;   // Full width in NDC
            float quadHeight = 2.0f;  // Full height in NDC

            // Scale to maintain pixel aspect ratio
            if (drawableAspect > pixelAspect) {
                // Drawable is wider than pixel buffer - letterbox horizontally
                quadWidth = 2.0f * (pixelAspect / drawableAspect);
            } else {
                // Drawable is taller than pixel buffer - letterbox vertically
                quadHeight = 2.0f * (drawableAspect / pixelAspect);
            }

            float left = -quadWidth / 2.0f;
            float right = quadWidth / 2.0f;
            float top = quadHeight / 2.0f;
            float bottom = -quadHeight / 2.0f;

            // Create aspect-corrected quad vertices (NDC)
            struct QuadVertex {
                float position[2];
                float texCoord[2];
            } wresQuad[6] = {
                {{left, bottom}, {0, 1}},   // Bottom-left
                {{right, bottom}, {1, 1}},  // Bottom-right
                {{left, top}, {0, 0}},      // Top-left
                {{right, bottom}, {1, 1}},  // Bottom-right
                {{right, top}, {1, 0}},     // Top-right
                {{left, top}, {0, 0}}       // Top-left
            };

            [encoder setVertexBytes:wresQuad length:sizeof(wresQuad) atIndex:0];
            // Use front buffer (texture 0) for display
            [encoder setFragmentTexture:m_impl->wresTextures[0] atIndex:0];
            [encoder setFragmentBuffer:m_impl->wresPaletteBuffer offset:0 atIndex:2];

            [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];

            static int wresRenderCount = 0;
            if (++wresRenderCount % 60 == 0) {
                NSLog(@"[WRES] Rendering WRES mode (%d×%d, pixelAspect=%.2f, drawableAspect=%.2f)",
                      pixelWidth, pixelHeight, pixelAspect, drawableAspect);
                NSLog(@"[WRES] Drawable: %.0fx%.0f, Viewport: %dx%d",
                      drawableWidth, drawableHeight, m_impl->viewportWidth, m_impl->viewportHeight);
                NSLog(@"[WRES] Quad: left=%.4f, right=%.4f, top=%.4f, bottom=%.4f",
                      left, right, top, bottom);
            }
        }

        // --- PRES MODE ---
        if (displayMgr && displayMgr->isPResMode() && m_impl->presPipelineState && m_impl->presTextures[0]) {
            // --- PRES MODE: Render Premium Resolution buffer (1280×720 with 256-color hybrid palette) ---
            [encoder setRenderPipelineState:m_impl->presPipelineState];

            // PRES has fixed 1280×720 resolution (16:9 aspect ratio)
            int pixelWidth = 1280;
            int pixelHeight = 720;

            // Get actual drawable dimensions (accounts for Retina scaling)
            float drawableWidth = static_cast<float>(drawable.texture.width);
            float drawableHeight = static_cast<float>(drawable.texture.height);

            // Calculate aspect-corrected quad dimensions
            float pixelAspect = static_cast<float>(pixelWidth) / static_cast<float>(pixelHeight);
            float drawableAspect = drawableWidth / drawableHeight;

            float quadWidth = 2.0f;   // Full width in NDC
            float quadHeight = 2.0f;  // Full height in NDC

            // Scale to maintain pixel aspect ratio
            if (drawableAspect > pixelAspect) {
                // Drawable is wider than pixel buffer - letterbox horizontally
                quadWidth = 2.0f * (pixelAspect / drawableAspect);
            } else {
                // Drawable is taller than pixel buffer - letterbox vertically
                quadHeight = 2.0f * (drawableAspect / pixelAspect);
            }

            float left = -quadWidth / 2.0f;
            float right = quadWidth / 2.0f;
            float top = quadHeight / 2.0f;
            float bottom = -quadHeight / 2.0f;

            // Create aspect-corrected quad vertices (NDC)
            struct QuadVertex {
                float position[2];
                float texCoord[2];
            } presQuad[6] = {
                {{left, bottom}, {0, 1}},   // Bottom-left
                {{right, bottom}, {1, 1}},  // Bottom-right
                {{left, top}, {0, 0}},      // Top-left
                {{right, bottom}, {1, 1}},  // Bottom-right
                {{right, top}, {1, 0}},     // Top-right
                {{left, top}, {0, 0}}       // Top-left
            };

            [encoder setVertexBytes:presQuad length:sizeof(presQuad) atIndex:0];
            // Use front buffer (texture 0) for display
            [encoder setFragmentTexture:m_impl->presTextures[0] atIndex:0];
            [encoder setFragmentBuffer:m_impl->presPaletteBuffer offset:0 atIndex:2];

            [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
        }

        // --- URES MODE ---
        if (isUResMode && m_impl->uresPipelineState && m_impl->uresTextures[0]) {
            // --- URES MODE: Render high-resolution direct-color buffer (1280×720 ARGB4444) ---
            // NSLog(@"[URES] Entering URES render path");
            [encoder setRenderPipelineState:m_impl->uresPipelineState];

            // Get URES GPU buffer dimensions (always 1280x720)
            int pixelWidth = 1280;
            int pixelHeight = 720;

            // Get actual drawable dimensions (accounts for Retina scaling)
            float drawableWidth = static_cast<float>(drawable.texture.width);
            float drawableHeight = static_cast<float>(drawable.texture.height);

            // Calculate aspect-corrected quad dimensions
            // Pixels should have 1:1 aspect ratio (square pixels)
            float pixelAspect = static_cast<float>(pixelWidth) / static_cast<float>(pixelHeight);
            float drawableAspect = drawableWidth / drawableHeight;

            float quadWidth = 2.0f;   // Full width in NDC
            float quadHeight = 2.0f;  // Full height in NDC

            // Scale to maintain pixel aspect ratio
            if (drawableAspect > pixelAspect) {
                // Drawable is wider than pixel buffer - letterbox horizontally
                quadWidth = 2.0f * (pixelAspect / drawableAspect);
            } else {
                // Drawable is taller than pixel buffer - letterbox vertically
                quadHeight = 2.0f * (drawableAspect / pixelAspect);
            }

            float left = -quadWidth / 2.0f;
            float right = quadWidth / 2.0f;
            float top = quadHeight / 2.0f;
            float bottom = -quadHeight / 2.0f;

            // Create aspect-corrected quad vertices (NDC)
            struct QuadVertex {
                float position[2];
                float texCoord[2];
            } uresQuad[6] = {
                {{left, bottom}, {0, 1}},   // Bottom-left
                {{right, bottom}, {1, 1}},  // Bottom-right
                {{left, top}, {0, 0}},      // Top-left
                {{right, bottom}, {1, 1}},  // Bottom-right
                {{right, top}, {1, 0}},     // Top-right
                {{left, top}, {0, 0}}       // Top-left
            };

            [encoder setVertexBytes:uresQuad length:sizeof(uresQuad) atIndex:0];
            [encoder setFragmentTexture:m_impl->uresTextures[0] atIndex:0];

            [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];

            static int uresRenderCount = 0;
            if (++uresRenderCount % 60 == 0 || uresRenderCount < 5) {
                NSLog(@"[URES] Rendering URES mode (%d×%d, pixelAspect=%.2f, drawableAspect=%.2f)",
                      pixelWidth, pixelHeight, pixelAspect, drawableAspect);
                NSLog(@"[URES] Drawable: %.0fx%.0f, Viewport: %dx%d",
                      drawableWidth, drawableHeight, m_impl->viewportWidth, m_impl->viewportHeight);
                NSLog(@"[URES] Quad: left=%.4f, right=%.4f, top=%.4f, bottom=%.4f",
                      left, right, top, bottom);
            }
        }

        // --- LORES MODE ---
        if (isLoResMode && m_impl->loresPipelineState && m_impl->loresTextures[0]) {
            // --- LORES MODE: Render chunky pixel buffer (dynamic resolution) ---
            [encoder setRenderPipelineState:m_impl->loresPipelineState];

            // Get current buffer dimensions
            auto loresBuffer = displayMgr->getLoResBuffer();
            int pixelWidth = loresBuffer ? loresBuffer->getWidth() : 160;
            int pixelHeight = loresBuffer ? loresBuffer->getHeight() : 75;

            // Get actual drawable dimensions (accounts for Retina scaling)
            float drawableWidth = static_cast<float>(drawable.texture.width);
            float drawableHeight = static_cast<float>(drawable.texture.height);

            // Calculate texture coordinate scaling
            // The LORES texture is 640×300 (HIRES max), but we only use a portion for LORES (160×75)
            // We need to scale texture coordinates to sample only the active region
            float texMaxU = static_cast<float>(pixelWidth) / 640.0f;   // e.g., 160/640 = 0.25
            float texMaxV = static_cast<float>(pixelHeight) / 300.0f;  // e.g., 75/300 = 0.25

            // Calculate aspect-corrected quad dimensions
            // Pixels should have 1:1 aspect ratio (square pixels)
            float pixelAspect = static_cast<float>(pixelWidth) / static_cast<float>(pixelHeight);
            float drawableAspect = drawableWidth / drawableHeight;

            float quadWidth = 2.0f;   // Full width in NDC
            float quadHeight = 2.0f;  // Full height in NDC

            // Scale to maintain pixel aspect ratio
            if (drawableAspect > pixelAspect) {
                // Drawable is wider than pixel buffer - letterbox horizontally
                quadWidth = 2.0f * (pixelAspect / drawableAspect);
            } else {
                // Drawable is taller than pixel buffer - letterbox vertically
                quadHeight = 2.0f * (drawableAspect / pixelAspect);
            }

            float left = -quadWidth / 2.0f;
            float right = quadWidth / 2.0f;
            float top = quadHeight / 2.0f;
            float bottom = -quadHeight / 2.0f;

            // Create aspect-corrected quad vertices (NDC)
            // Use scaled texture coordinates to sample only the active region of the texture
            struct QuadVertex {
                float position[2];
                float texCoord[2];
            } loresQuad[6] = {
                {{left, bottom}, {0, texMaxV}},         // Bottom-left
                {{right, bottom}, {texMaxU, texMaxV}},  // Bottom-right
                {{left, top}, {0, 0}},                  // Top-left
                {{right, bottom}, {texMaxU, texMaxV}},  // Bottom-right
                {{right, top}, {texMaxU, 0}},           // Top-right
                {{left, top}, {0, 0}}                   // Top-left
            };

            [encoder setVertexBytes:loresQuad length:sizeof(loresQuad) atIndex:0];
            [encoder setFragmentTexture:m_impl->loresTextures[0] atIndex:0];  // Use front buffer for display
            [encoder setFragmentBuffer:m_impl->loresPaletteBuffer offset:0 atIndex:2];
            [encoder setFragmentBuffer:m_impl->uniformBuffer offset:0 atIndex:3];

            [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];

            static int loresRenderCount = 0;
            if (++loresRenderCount % 60 == 0) {
                NSLog(@"[LORES] Rendering LORES mode (%d×%d, pixelAspect=%.2f, drawableAspect=%.2f)",
                      pixelWidth, pixelHeight, pixelAspect, drawableAspect);
                NSLog(@"[LORES] Drawable: %.0fx%.0f, Viewport: %dx%d",
                      drawableWidth, drawableHeight, m_impl->viewportWidth, m_impl->viewportHeight);
                NSLog(@"[LORES] CAMetalLayer.drawableSize: %.0fx%.0f",
                      m_impl->metalLayer.drawableSize.width, m_impl->metalLayer.drawableSize.height);
                NSLog(@"[LORES] Texture coords scaled: U=0-%.4f, V=0-%.4f (buffer %dx%d in 640x300 texture)",
                      texMaxU, texMaxV, pixelWidth, pixelHeight);
                NSLog(@"[LORES] Quad dimensions: width=%.4f, height=%.4f", quadWidth, quadHeight);
                NSLog(@"[LORES] Quad corners: left=%.4f, right=%.4f, top=%.4f, bottom=%.4f", left, right, top, bottom);
                NSLog(@"[LORES] Vertex data being sent:");
                for (int i = 0; i < 6; i++) {
                    NSLog(@"[LORES]   Vertex %d: pos=(%.4f, %.4f), tex=(%.4f, %.4f)",
                          i, loresQuad[i].position[0], loresQuad[i].position[1],
                          loresQuad[i].texCoord[0], loresQuad[i].texCoord[1]);
                }
            }
        }

        // =================================================================
        // SHAPE PRIMITIVES LAYER (Rectangles, Circles, Lines, Polygons, Stars)
        // Renders after video mode, before graphics layer
        // =================================================================

        // --- RECTANGLE LAYER ---
        if (rectangleManager && !rectangleManager->isEmpty()) {
            rectangleManager->render(encoder);
        }

        // --- CIRCLE LAYER ---
        if (circleManager && !circleManager->isEmpty()) {
            circleManager->render(encoder);
        }

        // --- LINE LAYER ---
        if (lineManager && !lineManager->isEmpty()) {
            lineManager->render(encoder);
        }

        // --- POLYGON LAYER ---
        if (polygonManager && !polygonManager->isEmpty()) {
            polygonManager->render(encoder);
        }

        // --- STAR LAYER ---
        if (starManager && !starManager->isEmpty()) {
            starManager->render(encoder);
        }

        // =================================================================
        // GRAPHICS LAYER
        // Renders after shapes, before text
        // =================================================================

        if (graphicsLayer && m_impl->graphicsRenderer) {
            // Check if layer is visible
            if (!graphicsLayer->isVisible()) {
                // Skip rendering entirely if not visible
            } else {
                // Check if we have new commands or layer is dirty
                bool hasCommands = !graphicsLayer->isEmpty();
                bool layerDirty = graphicsLayer->isDirty();
                bool rendererDirty = m_impl->graphicsRenderer->isDirty();

                // Only re-render if layer has changed
                if (layerDirty || rendererDirty) {
                    if (hasCommands) {
                        // Render new commands to back buffer and swap
                        m_impl->graphicsRenderer->render(*graphicsLayer);

                        // Mark layer as clean after rendering
                        graphicsLayer->clearDirty();
                        m_impl->graphicsRenderer->markClean();
                    }
                }

                // Always composite cached texture if we have content (cheap operation)
                id<MTLTexture> graphicsTexture = m_impl->graphicsRenderer->getTexture();
                bool hasContent = m_impl->graphicsRenderer->hasContent();

                if (hasContent && graphicsTexture && m_impl->primitivesPipelineState) {
                    [encoder setRenderPipelineState:m_impl->primitivesPipelineState];

                    // Create full-screen quad vertices (matches GraphicsVertex shader)
                    struct QuadVertex {
                        float position[2];
                        float texCoord[2];
                    } quadVertices[6] = {
                        {{-1.0f, -1.0f}, {0, 1}},
                        {{ 1.0f, -1.0f}, {1, 1}},
                        {{-1.0f,  1.0f}, {0, 0}},
                        {{ 1.0f, -1.0f}, {1, 1}},
                        {{ 1.0f,  1.0f}, {1, 0}},
                        {{-1.0f,  1.0f}, {0, 0}}
                    };

                    [encoder setVertexBytes:quadVertices length:sizeof(quadVertices) atIndex:0];
                    [encoder setVertexBuffer:m_impl->uniformBuffer offset:0 atIndex:1];
                    [encoder setFragmentTexture:graphicsTexture atIndex:0];
                    [encoder setFragmentSamplerState:m_impl->samplerState atIndex:0];

                    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
                } else {
                    static bool loggedOnce = false;
                    if (!loggedOnce) {
                        if (!hasContent) {
                            NSLog(@"[MetalRenderer] Skipping graphics: no content (hasContent=false)");
                        } else if (!graphicsTexture) {
                            NSLog(@"[MetalRenderer] Skipping graphics: texture is nil");
                        } else if (!m_impl->primitivesPipelineState) {
                            NSLog(@"[MetalRenderer] Skipping graphics: pipeline state is nil");
                        }
                        loggedOnce = true;
                    }
                }
            }
        }

        // =================================================================
        // TEXT LAYER - DISABLED FOR DEBUGGING
        // =================================================================

        // Text rendering layer (renders on top of graphics)
        if (!m_impl->vertices.empty()) {
            // Debug output suppressed
            [encoder setRenderPipelineState:m_impl->pipelineState];
            [encoder setVertexBuffer:m_impl->vertexBuffer offset:0 atIndex:0];
            [encoder setVertexBuffer:m_impl->uniformBuffer offset:0 atIndex:1];

            id<MTLTexture> atlasTexture = (id<MTLTexture>)m_impl->fontAtlas->getTexture();
            [encoder setFragmentTexture:atlasTexture atIndex:0];
            [encoder setFragmentSamplerState:m_impl->samplerState atIndex:0];

            [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                        vertexStart:0
                        vertexCount:m_impl->vertices.size()];
        }

        // Render sextants separately if any exist
        if (!m_impl->sextantVertices.empty() && m_impl->sextantPipelineState) {
            // Create or update sextant vertex buffer
            NSUInteger sextantBufferSize = m_impl->sextantVertices.size() * sizeof(TextVertex);
            if (!m_impl->sextantVertexBuffer || m_impl->sextantVertexBuffer.length < sextantBufferSize) {
                m_impl->sextantVertexBuffer = [m_impl->device newBufferWithLength:sextantBufferSize
                                                                          options:MTLResourceStorageModeShared];
            }
            std::memcpy([m_impl->sextantVertexBuffer contents], m_impl->sextantVertices.data(), sextantBufferSize);

            // Log first few vertices
            if (m_impl->sextantVertices.size() > 0) {
                auto& v0 = m_impl->sextantVertices[0];
                // NSLog(@"[LORES DEBUG] First vertex: pos=(%.1f,%.1f) color=(%.3f,%.3f,%.3f,%.3f)",
                //       v0.position[0], v0.position[1], v0.color[0], v0.color[1], v0.color[2], v0.color[3]);
            }

            // Switch to sextant pipeline
            // NSLog(@"[LORES DEBUG] Setting sextant pipeline state");
            [encoder setRenderPipelineState:m_impl->sextantPipelineState];
            [encoder setVertexBuffer:m_impl->sextantVertexBuffer offset:0 atIndex:0];
            [encoder setVertexBuffer:m_impl->uniformBuffer offset:0 atIndex:1];

            // NSLog(@"[LORES DEBUG] Binding palette buffer: %@", m_impl->loresPaletteBuffer);
            [encoder setFragmentBuffer:m_impl->loresPaletteBuffer offset:0 atIndex:2];

            id<MTLTexture> atlasTexture = (id<MTLTexture>)m_impl->fontAtlas->getTexture();
            [encoder setFragmentTexture:atlasTexture atIndex:0];
            [encoder setFragmentSamplerState:m_impl->samplerState atIndex:0];

            // Draw sextants
            // NSLog(@"[LORES DEBUG] Drawing %zu sextant vertices as triangles", m_impl->sextantVertices.size());
            [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                        vertexStart:0
                        vertexCount:m_impl->sextantVertices.size()];
            // NSLog(@"[LORES DEBUG] Sextant draw call complete");
        } else {
            // NSLog(@"[LORES DEBUG] NOT rendering sextants - empty=%d, no pipeline=%d",
            //       m_impl->sextantVertices.empty(), (m_impl->sextantPipelineState == nil));
        }

        // =================================================================
        // SPRITE LAYER
        // Renders after text
        // =================================================================
        NSLog(@"[SPRITE LAYER] Rendering with viewport: %dx%d", m_impl->viewportWidth, m_impl->viewportHeight);
        if (spriteManager) {
            spriteManager->render(encoder, m_impl->viewportWidth, m_impl->viewportHeight);

            // Clear dirty flag after render
            if (spriteManager->isDirty()) {
                spriteManager->clearDirty();
            }
        }

        // --- PARTICLE LAYER ---
        // Render particles using GPU instanced quads
        bool systemReady2 = st_particle_system_is_ready();
        bool hasPipeline2 = (m_impl->particlePipelineState != nil);
        NSLog(@"[PARTICLE DEBUG 2] systemReady=%d hasPipeline=%d viewport=%dx%d", systemReady2, hasPipeline2, m_impl->viewportWidth, m_impl->viewportHeight);

        if (systemReady2 && hasPipeline2) {
            const void* particleData = nullptr;
            uint32_t particleCount = 0;

            NSLog(@"[PARTICLE RENDER 2] System ready, checking for particles...");
            if (st_particle_get_render_data(&particleData, &particleCount) && particleCount > 0) {
                NSLog(@"[PARTICLE RENDER 2] Got %d particles to render", particleCount);
                const SuperTerminal::Particle* particles = static_cast<const SuperTerminal::Particle*>(particleData);

                // Get sprite texture from first particle's source sprite (if needed)
                id<MTLTexture> spriteTexture = nil;
                uint16_t sourceSprite = particles[0].sourceSprite;
                bool useTexture = (particles[0].mode == SuperTerminal::ParticleMode::SPRITE_FRAGMENT);
                NSLog(@"[PARTICLE RENDER 2] First particle sourceSprite: %d, mode: %d, useTexture: %d",
                      sourceSprite, (int)particles[0].mode, useTexture);

                if (useTexture && sourceSprite > 0 && spriteManager) {
                    spriteTexture = (id<MTLTexture>)spriteManager->getSpriteTexture(sourceSprite);
                    NSLog(@"[PARTICLE RENDER 2] Sprite texture: %@", spriteTexture ? @"valid" : @"nil");
                }

                // Prepare instance data
                struct ParticleInstanceData {
                    simd_float2 position;
                    simd_float2 texCoordMin;
                    simd_float2 texCoordMax;
                    simd_float4 color;
                    float scale;
                    float rotation;
                    float alpha;
                };

                size_t instanceDataSize = sizeof(ParticleInstanceData) * particleCount;
                if (!m_impl->particleInstanceBuffer || [m_impl->particleInstanceBuffer length] < instanceDataSize) {
                    m_impl->particleInstanceBuffer = [m_impl->device newBufferWithLength:instanceDataSize
                                                                                  options:MTLResourceStorageModeShared];
                }

                ParticleInstanceData* instanceData = (ParticleInstanceData*)[m_impl->particleInstanceBuffer contents];

                // Get display scale factor dynamically (1.0 for non-Retina, 2.0 for Retina)
                NSScreen* screen = [NSScreen mainScreen];
                CGFloat displayScale = screen ? screen.backingScaleFactor : 1.0f;

                // Populate instance data from particles
                for (uint32_t i = 0; i < particleCount; i++) {
                    const auto& p = particles[i];
                    if (!p.active) continue;

                    if (i < 3) {
                        NSLog(@"[PARTICLE RENDER 2] Particle %d: pos=(%.1f, %.1f) size=%.2f life=%.2f/%.2f active=%d color=0x%08X",
                              i, p.x, p.y, p.size, p.life, p.maxLife, p.active, p.color);
                    }

                    // Scale positions to match physical viewport (accounts for Retina displays)
                    instanceData[i].position = simd_make_float2(p.x * displayScale, p.y * displayScale);

                    if (i < 3) {
                        NSLog(@"[PARTICLE RENDER 2] Instance %d: position=(%.1f, %.1f) scale=%.2f displayScale=%.1f (from %.1f, %.1f)",
                              i, instanceData[i].position.x, instanceData[i].position.y, p.size * p.scaleMultiplier, displayScale, p.x, p.y);
                    }
                    instanceData[i].texCoordMin = simd_make_float2(p.texCoordMinX, p.texCoordMinY);
                    instanceData[i].texCoordMax = simd_make_float2(p.texCoordMaxX, p.texCoordMaxY);

                    // Unpack color
                    uint32_t packed = p.color;
                    float r = ((packed >> 24) & 0xFF) / 255.0f;
                    float g = ((packed >> 16) & 0xFF) / 255.0f;
                    float b = ((packed >> 8) & 0xFF) / 255.0f;
                    float a = (packed & 0xFF) / 255.0f;
                    instanceData[i].color = simd_make_float4(r, g, b, a);

                    instanceData[i].scale = p.size * p.scaleMultiplier;
                    instanceData[i].rotation = p.rotation;
                    instanceData[i].alpha = p.life / p.maxLife;
                }

                // Create base quad vertices
                struct ParticleVertex {
                    simd_float2 position;
                    simd_float2 texCoord;
                };

                ParticleVertex quadVertices[6] = {
                    {{-1.0f, -1.0f}, {0.0f, 1.0f}},
                    {{ 1.0f, -1.0f}, {1.0f, 1.0f}},
                    {{-1.0f,  1.0f}, {0.0f, 0.0f}},
                    {{ 1.0f, -1.0f}, {1.0f, 1.0f}},
                    {{ 1.0f,  1.0f}, {1.0f, 0.0f}},
                    {{-1.0f,  1.0f}, {0.0f, 0.0f}}
                };

                // Set up rendering
                [encoder setRenderPipelineState:m_impl->particlePipelineState];
                [encoder setVertexBytes:quadVertices length:sizeof(quadVertices) atIndex:0];
                [encoder setVertexBytes:&m_impl->uniforms length:sizeof(TextUniforms) atIndex:1];
                [encoder setVertexBuffer:m_impl->particleInstanceBuffer offset:0 atIndex:2];

                // Bind sprite texture and sampler (if using texture mode)
                if (useTexture) {
                    if (spriteTexture) {
                        [encoder setFragmentTexture:spriteTexture atIndex:0];
                        [encoder setFragmentSamplerState:m_impl->samplerState atIndex:0];
                        NSLog(@"[PARTICLE RENDER 2] Using texture mode with sprite texture");
                    } else {
                        NSLog(@"[PARTICLE 2] Warning: SPRITE_FRAGMENT mode but no texture for sprite %d - skipping", sourceSprite);
                    }
                } else {
                    // POINT_SPRITE mode - no texture needed, will use particle colors
                    NSLog(@"[PARTICLE RENDER 2] Using POINT_SPRITE mode (no texture)");
                }

                // Draw particles (works for both modes)
                [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                            vertexStart:0
                            vertexCount:6
                          instanceCount:particleCount];
                NSLog(@"[PARTICLE RENDER 2] Drew %d particle instances", particleCount);
            }
        }

        // --- TEXT DISPLAY LAYER (OVERLAY) ---
        // Render transformed overlay text on top of everything else
        if (m_impl->textDisplayManager && m_impl->textDisplayManager->hasContent()) {
            // Build vertex buffers for current viewport
            m_impl->textDisplayManager->buildVertexBuffers(m_impl->viewportWidth, m_impl->viewportHeight);

            // Check if effects are present and choose appropriate pipeline
            bool hasEffects = m_impl->textDisplayManager->hasEffects();
            if (hasEffects && m_impl->textEffectsPipelineState) {
                // Use text effects pipeline
                [encoder setRenderPipelineState:m_impl->textEffectsPipelineState];
            } else {
                // Use transformed text pipeline (fixed 1920x1080 coordinates)
                [encoder setRenderPipelineState:m_impl->transformedTextPipelineState];
            }

            [encoder setVertexBuffer:m_impl->uniformBuffer offset:0 atIndex:1];
            [encoder setFragmentTexture:m_impl->fontAtlas->getTexture() atIndex:0];
            [encoder setFragmentSamplerState:m_impl->samplerState atIndex:0];

            // Render transformed text
            m_impl->textDisplayManager->render(encoder, m_impl->viewportWidth, m_impl->viewportHeight);
        }

        [encoder endEncoding];

        // Present drawable with proper synchronization
        [commandBuffer presentDrawable:drawable];

        // Add completion handler to signal semaphore when frame is done
        __block dispatch_semaphore_t blockSemaphore = m_impl->inflightSemaphore;
        [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
            dispatch_semaphore_signal(blockSemaphore);
        }];

        // Commit command buffer
        [commandBuffer commit];

        return true;
    }
}

void MetalRenderer::renderGraphics(void* encoderPtr, const GraphicsLayer& graphicsLayer) {
    // This method is now unused - graphics are rendered via CoreGraphics
}

// GPU Blitter implementations

void MetalRenderer::beginBlitBatch() {
    if (!m_impl) {
        NSLog(@"[GPU_BLITTER] ERROR: Renderer not initialized");
        return;
    }

    if (m_impl->inBlitBatch) {
        NSLog(@"[GPU_BLITTER] WARNING: Already in blit batch, ending previous batch");
        endBlitBatch();
    }

    // Create command buffer for the batch
    m_impl->batchCommandBuffer = [m_impl->commandQueue commandBuffer];
    m_impl->batchEncoder = [m_impl->batchCommandBuffer computeCommandEncoder];
    m_impl->inBlitBatch = true;
}

void MetalRenderer::endBlitBatch() {
    if (!m_impl) {
        NSLog(@"[GPU_BLITTER] ERROR: Renderer not initialized");
        return;
    }

    if (!m_impl->inBlitBatch) {
        NSLog(@"[GPU_BLITTER] WARNING: Not in blit batch, nothing to end");
        return;
    }

    // End encoding and commit
    [m_impl->batchEncoder endEncoding];
    [m_impl->batchCommandBuffer commit];

    // Wait for command buffer to be scheduled to ensure it starts executing
    // This is critical for PRES mode where we need the GPU operations to complete
    // before flipping buffers
    [m_impl->batchCommandBuffer waitUntilScheduled];

    // Clear batch state
    m_impl->batchCommandBuffer = nil;
    m_impl->batchEncoder = nil;
    m_impl->inBlitBatch = false;
}

bool MetalRenderer::isInBlitBatch() const {
    return m_impl && m_impl->inBlitBatch;
}

// =============================================================================
// LORES GPU operations
// =============================================================================

void MetalRenderer::loresBlitGPU(int srcBuffer, int dstBuffer,
                                  int srcX, int srcY, int width, int height,
                                  int dstX, int dstY) {
    if (!m_impl || !m_impl->blitterCopyPipeline) {
        NSLog(@"[GPU_BLITTER] ERROR: Blitter not initialized");
        return;
    }

    if (srcBuffer < 0 || srcBuffer > 7 || dstBuffer < 0 || dstBuffer > 7) {
        NSLog(@"[GPU_BLITTER] ERROR: Invalid buffer indices: src=%d dst=%d", srcBuffer, dstBuffer);
        return;
    }

    // Get textures from LORES texture array
    id<MTLTexture> srcTexture = m_impl->loresTextures[srcBuffer];
    id<MTLTexture> dstTexture = m_impl->loresTextures[dstBuffer];

    if (!srcTexture || !dstTexture) {
        NSLog(@"[GPU_BLITTER] ERROR: Null textures: src=%@, dst=%@", srcTexture, dstTexture);
        return;
    }

    // Use batch encoder if available, otherwise create one-off command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->blitterCopyPipeline];
    [encoder setTexture:srcTexture atIndex:0];
    [encoder setTexture:dstTexture atIndex:1];

    struct BlitParams {
        int srcX, srcY, width, height, dstX, dstY;
        int transparentColor;
        int useTransparency;
    } params = { srcX, srcY, width, height, dstX, dstY, 0, 0 };

    [encoder setBytes:&params length:sizeof(params) atIndex:0];

    MTLSize gridSize = MTLSizeMake(width, height, 1);
    NSUInteger w = m_impl->blitterCopyPipeline.threadExecutionWidth;
    NSUInteger h = m_impl->blitterCopyPipeline.maxTotalThreadsPerThreadgroup / w;
    MTLSize threadgroupSize = MTLSizeMake(w, h, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::loresBlitTransparentGPU(int srcBuffer, int dstBuffer,
                                             int srcX, int srcY, int width, int height,
                                             int dstX, int dstY, uint8_t transparentColor) {
    if (!m_impl || !m_impl->blitterTransparentPipeline) {
        NSLog(@"[GPU_BLITTER] ERROR: Transparent blitter not initialized");
        return;
    }

    if (srcBuffer < 0 || srcBuffer > 7 || dstBuffer < 0 || dstBuffer > 7) {
        NSLog(@"[GPU_BLITTER] ERROR: Invalid buffer indices: src=%d dst=%d", srcBuffer, dstBuffer);
        return;
    }

    // Get textures from LORES texture array
    id<MTLTexture> srcTexture = m_impl->loresTextures[srcBuffer];
    id<MTLTexture> dstTexture = m_impl->loresTextures[dstBuffer];

    if (!srcTexture || !dstTexture) {
        NSLog(@"[GPU_BLITTER] ERROR: Null LORES textures: src=%@, dst=%@", srcTexture, dstTexture);
        return;
    }

    // Use batch encoder if available, otherwise create one-off command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->blitterTransparentPipeline];
    [encoder setTexture:srcTexture atIndex:0];
    [encoder setTexture:dstTexture atIndex:1];

    struct BlitParams {
        int srcX, srcY, width, height, dstX, dstY;
        int transparentColor;
        int useTransparency;
    } params = { srcX, srcY, width, height, dstX, dstY, transparentColor, 1 };

    [encoder setBytes:&params length:sizeof(params) atIndex:0];

    MTLSize gridSize = MTLSizeMake(width, height, 1);
    NSUInteger w = m_impl->blitterTransparentPipeline.threadExecutionWidth;
    NSUInteger h = m_impl->blitterTransparentPipeline.maxTotalThreadsPerThreadgroup / w;
    MTLSize threadgroupSize = MTLSizeMake(w, h, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::loresClearGPU(int bufferIndex, uint8_t colorIndex) {
    if (!m_impl || !m_impl->blitterClearPipeline) {
        NSLog(@"[GPU_BLITTER] ERROR: Clear pipeline not initialized");
        return;
    }

    if (bufferIndex < 0 || bufferIndex > 7) {
        NSLog(@"[GPU_BLITTER] ERROR: Invalid buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->loresTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[GPU_BLITTER] ERROR: Null LORES texture for buffer %d", bufferIndex);
        return;
    }

    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->blitterClearPipeline];
    [encoder setTexture:texture atIndex:0];

    struct ClearParams {
        uint32_t colorIndex;
        uint32_t width;
        uint32_t height;
    } params = { colorIndex, (uint32_t)texture.width, (uint32_t)texture.height };

    [encoder setBytes:&params length:sizeof(params) atIndex:0];

    MTLSize gridSize = MTLSizeMake(texture.width, texture.height, 1);
    NSUInteger w = m_impl->blitterClearPipeline.threadExecutionWidth;
    NSUInteger h = m_impl->blitterClearPipeline.maxTotalThreadsPerThreadgroup / w;
    MTLSize threadgroupSize = MTLSizeMake(w, h, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::loresRectFillGPU(int bufferIndex, int x, int y, int width, int height, uint8_t colorIndex) {
    if (!m_impl || !m_impl->blitterClearPipeline) {
        NSLog(@"[GPU_BLITTER] ERROR: Clear pipeline not initialized (using as fallback for rect fill)");
        // Note: We reuse clear pipeline since LORES doesn't have dedicated rect fill pipeline
        return;
    }

    if (bufferIndex < 0 || bufferIndex > 7) {
        NSLog(@"[GPU_BLITTER] ERROR: Invalid buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->loresTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[GPU_BLITTER] ERROR: Null texture for buffer %d", bufferIndex);
        return;
    }

    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->blitterClearPipeline];
    [encoder setTexture:texture atIndex:0];

    struct RectFillParams {
        int x, y, width, height;
        uint32_t colorIndex;
    } params = { x, y, width, height, colorIndex };

    [encoder setBytes:&params length:sizeof(params) atIndex:0];

    MTLSize gridSize = MTLSizeMake(width, height, 1);
    NSUInteger w = m_impl->blitterClearPipeline.threadExecutionWidth;
    NSUInteger h = m_impl->blitterClearPipeline.maxTotalThreadsPerThreadgroup / w;
    MTLSize threadgroupSize = MTLSizeMake(w, h, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::loresCircleFillGPU(int bufferIndex, int cx, int cy, int radius, uint8_t colorIndex) {
    if (!m_impl || !m_impl->blitterClearPipeline) {
        NSLog(@"[GPU_BLITTER] ERROR: Clear pipeline not initialized (using as fallback for circle fill)");
        return;
    }

    if (bufferIndex < 0 || bufferIndex > 7) {
        NSLog(@"[GPU_BLITTER] ERROR: Invalid buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->loresTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[GPU_BLITTER] ERROR: Null texture for buffer %d", bufferIndex);
        return;
    }

    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->blitterClearPipeline];
    [encoder setTexture:texture atIndex:0];

    struct CircleFillParams {
        int cx, cy, radius;
        uint32_t colorIndex;
    } params = { cx, cy, radius, colorIndex };

    [encoder setBytes:&params length:sizeof(params) atIndex:0];

    int diameter = radius * 2 + 1;
    MTLSize gridSize = MTLSizeMake(diameter, diameter, 1);
    NSUInteger w = m_impl->blitterClearPipeline.threadExecutionWidth;
    NSUInteger h = m_impl->blitterClearPipeline.maxTotalThreadsPerThreadgroup / w;
    MTLSize threadgroupSize = MTLSizeMake(w, h, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::loresLineGPU(int bufferIndex, int x0, int y0, int x1, int y1, uint8_t colorIndex) {
    if (!m_impl || !m_impl->blitterClearPipeline) {
        NSLog(@"[GPU_BLITTER] ERROR: Clear pipeline not initialized (using as fallback for line)");
        return;
    }

    if (bufferIndex < 0 || bufferIndex > 7) {
        NSLog(@"[GPU_BLITTER] ERROR: Invalid buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->loresTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[GPU_BLITTER] ERROR: Null texture for buffer %d", bufferIndex);
        return;
    }

    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->blitterClearPipeline];
    [encoder setTexture:texture atIndex:0];

    struct LineParams {
        int x0, y0, x1, y1;
        uint32_t colorIndex;
    } params = { x0, y0, x1, y1, colorIndex };

    [encoder setBytes:&params length:sizeof(params) atIndex:0];

    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int length = (dx > dy ? dx : dy) + 1;

    MTLSize gridSize = MTLSizeMake(length, 1, 1);
    NSUInteger w = m_impl->blitterClearPipeline.threadExecutionWidth;
    MTLSize threadgroupSize = MTLSizeMake(w, 1, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

// =============================================================================
// XRES GPU operations
// =============================================================================

void MetalRenderer::xresBlitGPU(int srcBuffer, int dstBuffer,
                                 int srcX, int srcY, int width, int height,
                                 int dstX, int dstY) {
    if (!m_impl || !m_impl->blitterCopyPipeline) {
        NSLog(@"[GPU_BLITTER] ERROR: Blitter not initialized");
        return;
    }

    if (srcBuffer < 0 || srcBuffer > 7 || dstBuffer < 0 || dstBuffer > 7) {
        NSLog(@"[GPU_BLITTER] ERROR: Invalid buffer indices: src=%d dst=%d", srcBuffer, dstBuffer);
        return;
    }

    // Get textures
    id<MTLTexture> srcTexture = m_impl->xresTextures[srcBuffer];
    id<MTLTexture> dstTexture = m_impl->xresTextures[dstBuffer];

    if (!srcTexture || !dstTexture) {
        NSLog(@"[GPU_BLITTER] ERROR: Null textures: src=%@, dst=%@", srcTexture, dstTexture);
        return;
    }

    // Use batch encoder if available, otherwise create one-off command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->blitterCopyPipeline];
    [encoder setTexture:srcTexture atIndex:0];
    [encoder setTexture:dstTexture atIndex:1];

    // Set parameters
    int sourceRect[4] = {srcX, srcY, width, height};
    int destPos[2] = {dstX, dstY};
    [encoder setBytes:&sourceRect length:sizeof(sourceRect) atIndex:0];
    [encoder setBytes:&destPos length:sizeof(destPos) atIndex:1];

    // Dispatch compute threads
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);
    MTLSize threadgroups = MTLSizeMake(
        (width + threadgroupSize.width - 1) / threadgroupSize.width,
        (height + threadgroupSize.height - 1) / threadgroupSize.height,
        1
    );

    [encoder dispatchThreadgroups:threadgroups threadsPerThreadgroup:threadgroupSize];

    // Only end encoding and commit if not batching
    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::xresBlitTransparentGPU(int srcBuffer, int dstBuffer,
                                            int srcX, int srcY, int width, int height,
                                            int dstX, int dstY, uint8_t transparentColor) {
    if (!m_impl || !m_impl->blitterTransparentPipeline) {
        NSLog(@"[GPU_BLITTER] ERROR: Transparent blitter not initialized");
        return;
    }

    if (srcBuffer < 0 || srcBuffer > 7 || dstBuffer < 0 || dstBuffer > 7) {
        NSLog(@"[GPU_BLITTER] ERROR: Invalid buffer indices: src=%d dst=%d", srcBuffer, dstBuffer);
        return;
    }

    id<MTLTexture> srcTexture = m_impl->xresTextures[srcBuffer];
    id<MTLTexture> dstTexture = m_impl->xresTextures[dstBuffer];

    if (!srcTexture || !dstTexture) {
        NSLog(@"[GPU_BLITTER] ERROR: Null textures");
        return;
    }

    // Use batch encoder if available, otherwise create one-off command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->blitterCopyPipeline];
    [encoder setTexture:srcTexture atIndex:0];
    [encoder setTexture:dstTexture atIndex:1];

    // Set parameters
    int sourceRect[4] = {srcX, srcY, width, height};
    int destPos[2] = {dstX, dstY};
    [encoder setBytes:&sourceRect length:sizeof(sourceRect) atIndex:0];
    [encoder setBytes:&destPos length:sizeof(destPos) atIndex:1];

    // Dispatch compute threads
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);
    MTLSize threadgroups = MTLSizeMake(
        (width + threadgroupSize.width - 1) / threadgroupSize.width,
        (height + threadgroupSize.height - 1) / threadgroupSize.height,
        1
    );

    [encoder dispatchThreadgroups:threadgroups threadsPerThreadgroup:threadgroupSize];

    // Only end encoding and commit if not batching
    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
    // Async: no wait - let Metal handle scheduling
}

void MetalRenderer::wresBlitGPU(int srcBuffer, int dstBuffer,
                                 int srcX, int srcY, int width, int height,
                                 int dstX, int dstY) {
    if (!m_impl || !m_impl->blitterCopyPipeline) {
        NSLog(@"[GPU_BLITTER] ERROR: Blitter not initialized");
        return;
    }

    if (srcBuffer < 0 || srcBuffer > 7 || dstBuffer < 0 || dstBuffer > 7) {
        NSLog(@"[GPU_BLITTER] ERROR: Invalid buffer indices: src=%d dst=%d", srcBuffer, dstBuffer);
        return;
    }

    id<MTLTexture> srcTexture = m_impl->wresTextures[srcBuffer];
    id<MTLTexture> dstTexture = m_impl->wresTextures[dstBuffer];

    if (!srcTexture || !dstTexture) {
        NSLog(@"[GPU_BLITTER] ERROR: Null textures");
        return;
    }

    // Use batch encoder if available, otherwise create one-off command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->blitterCopyPipeline];
    [encoder setTexture:srcTexture atIndex:0];
    [encoder setTexture:dstTexture atIndex:1];

    // Set parameters
    int sourceRect[4] = {srcX, srcY, width, height};
    int destPos[2] = {dstX, dstY};
    [encoder setBytes:&sourceRect length:sizeof(sourceRect) atIndex:0];
    [encoder setBytes:&destPos length:sizeof(destPos) atIndex:1];

    // Dispatch compute threads
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);
    MTLSize threadgroups = MTLSizeMake(
        (width + threadgroupSize.width - 1) / threadgroupSize.width,
        (height + threadgroupSize.height - 1) / threadgroupSize.height,
        1
    );

    [encoder dispatchThreadgroups:threadgroups threadsPerThreadgroup:threadgroupSize];

    // Only end encoding and commit if not batching
    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
    // Async: no wait - let Metal handle scheduling
}

void MetalRenderer::wresBlitTransparentGPU(int srcBuffer, int dstBuffer,
                                            int srcX, int srcY, int width, int height,
                                            int dstX, int dstY, uint8_t transparentColor) {
    if (!m_impl || !m_impl->blitterTransparentPipeline) {
        NSLog(@"[GPU_BLITTER] ERROR: Transparent blitter not initialized");
        return;
    }

    if (srcBuffer < 0 || srcBuffer > 7 || dstBuffer < 0 || dstBuffer > 7) {
        NSLog(@"[GPU_BLITTER] ERROR: Invalid buffer indices: src=%d dst=%d", srcBuffer, dstBuffer);
        return;
    }

    id<MTLTexture> srcTexture = m_impl->wresTextures[srcBuffer];
    id<MTLTexture> dstTexture = m_impl->wresTextures[dstBuffer];

    if (!srcTexture || !dstTexture) {
        NSLog(@"[GPU_BLITTER] ERROR: Null textures");
        return;
    }

    // Use batch encoder if available, otherwise create one-off command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->blitterTransparentPipeline];
    [encoder setTexture:srcTexture atIndex:0];
    [encoder setTexture:dstTexture atIndex:1];

    // Set parameters
    int sourceRect[4] = {srcX, srcY, width, height};
    int destPos[2] = {dstX, dstY};
    uint32_t transparentColorU32 = static_cast<uint32_t>(transparentColor);
    [encoder setBytes:&sourceRect length:sizeof(sourceRect) atIndex:0];
    [encoder setBytes:&destPos length:sizeof(destPos) atIndex:1];
    [encoder setBytes:&transparentColorU32 length:sizeof(transparentColorU32) atIndex:2];

    // Dispatch compute threads
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);
    MTLSize threadgroups = MTLSizeMake(
        (width + threadgroupSize.width - 1) / threadgroupSize.width,
        (height + threadgroupSize.height - 1) / threadgroupSize.height,
        1
    );

    [encoder dispatchThreadgroups:threadgroups threadsPerThreadgroup:threadgroupSize];

    // Only end encoding and commit if not batching
    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
    // Async: no wait - let Metal handle scheduling
}

void MetalRenderer::xresClearGPU(int bufferIndex, uint8_t colorIndex) {
    if (!m_impl || !m_impl->blitterClearPipeline) {
        NSLog(@"[GPU_BLITTER] ERROR: Clear pipeline not initialized");
        return;
    }

    // Validate buffer index
    if (bufferIndex < 0 || bufferIndex > 7) {
        NSLog(@"[GPU_BLITTER] ERROR: Invalid buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->xresTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[GPU_BLITTER] ERROR: XRES texture[%d] not available", bufferIndex);
        return;
    }

    // Get or create command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;

    if (m_impl->inBlitBatch && m_impl->batchEncoder) {
        // Use existing batch encoder
        encoder = m_impl->batchEncoder;
    } else {
        // Create new command buffer
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    // Set up clear operation
    [encoder setComputePipelineState:m_impl->blitterClearPipeline];
    [encoder setTexture:texture atIndex:0];

    uint32_t clearColor = (uint32_t)colorIndex;
    [encoder setBytes:&clearColor length:sizeof(uint32_t) atIndex:0];

    // Dispatch threads to cover entire texture (320x240)
    MTLSize gridSize = MTLSizeMake(320, 240, 1);
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    // If not batching, commit immediately
    if (!m_impl->inBlitBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::wresClearGPU(int bufferIndex, uint8_t colorIndex) {
    if (!m_impl || !m_impl->blitterClearPipeline) {
        NSLog(@"[GPU_BLITTER] ERROR: Clear pipeline not initialized");
        return;
    }

    // Validate buffer index
    if (bufferIndex < 0 || bufferIndex > 7) {
        NSLog(@"[GPU_BLITTER] ERROR: Invalid buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->wresTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[GPU_BLITTER] ERROR: WRES texture[%d] not available", bufferIndex);
        return;
    }

    // Get or create command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;

    if (m_impl->inBlitBatch && m_impl->batchEncoder) {
        // Use existing batch encoder
        encoder = m_impl->batchEncoder;
    } else {
        // Create new command buffer
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    // Set up clear operation
    [encoder setComputePipelineState:m_impl->blitterClearPipeline];
    [encoder setTexture:texture atIndex:0];

    uint32_t clearColor = (uint32_t)colorIndex;
    [encoder setBytes:&clearColor length:sizeof(uint32_t) atIndex:0];

    // Dispatch threads to cover entire texture (432x240)
    MTLSize gridSize = MTLSizeMake(432, 240, 1);
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    // If not batching, commit immediately
    if (!m_impl->inBlitBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::syncGPU() {
    if (!m_impl || !m_impl->commandQueue) {
        return;
    }

    // Create a command buffer and wait for it to complete
    // This ensures all previously submitted command buffers have finished
    id<MTLCommandBuffer> syncBuffer = [m_impl->commandQueue commandBuffer];
    [syncBuffer commit];
    [syncBuffer waitUntilCompleted];
}

// ============================================================================
// GPU PRIMITIVE DRAWING IMPLEMENTATIONS
// ============================================================================

void MetalRenderer::xresRectFillGPU(int bufferIndex, int x, int y, int width, int height, uint8_t colorIndex) {
    if (!m_impl || !m_impl->primitiveRectPipeline) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Rect pipeline not initialized");
        return;
    }

    if (bufferIndex < 0 || bufferIndex >= 4) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Invalid XRES buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->xresTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: XRES texture[%d] not available", bufferIndex);
        return;
    }

    // Get or create command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->primitiveRectPipeline];
    [encoder setTexture:texture atIndex:0];

    int rect[4] = {x, y, width, height};
    uint32_t fillColor = (uint32_t)colorIndex;
    [encoder setBytes:&rect length:sizeof(rect) atIndex:0];
    [encoder setBytes:&fillColor length:sizeof(uint32_t) atIndex:1];

    // Dispatch threads to cover rectangle
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);
    MTLSize threadgroups = MTLSizeMake(
        (width + threadgroupSize.width - 1) / threadgroupSize.width,
        (height + threadgroupSize.height - 1) / threadgroupSize.height,
        1
    );

    [encoder dispatchThreadgroups:threadgroups threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::wresRectFillGPU(int bufferIndex, int x, int y, int width, int height, uint8_t colorIndex) {
    if (!m_impl || !m_impl->primitiveRectPipeline) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Rect pipeline not initialized");
        return;
    }

    if (bufferIndex < 0 || bufferIndex >= 4) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Invalid WRES buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->wresTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: WRES texture[%d] not available", bufferIndex);
        return;
    }

    // Get or create command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->primitiveRectPipeline];
    [encoder setTexture:texture atIndex:0];

    int rect[4] = {x, y, width, height};
    uint32_t fillColor = (uint32_t)colorIndex;
    [encoder setBytes:&rect length:sizeof(rect) atIndex:0];
    [encoder setBytes:&fillColor length:sizeof(uint32_t) atIndex:1];

    // Dispatch threads to cover rectangle
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);
    MTLSize threadgroups = MTLSizeMake(
        (width + threadgroupSize.width - 1) / threadgroupSize.width,
        (height + threadgroupSize.height - 1) / threadgroupSize.height,
        1
    );

    [encoder dispatchThreadgroups:threadgroups threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::xresCircleFillGPU(int bufferIndex, int cx, int cy, int radius, uint8_t colorIndex) {
    if (!m_impl || !m_impl->primitiveCirclePipeline) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Circle pipeline not initialized");
        return;
    }

    if (bufferIndex < 0 || bufferIndex >= 4) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Invalid XRES buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->xresTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: XRES texture[%d] not available", bufferIndex);
        return;
    }

    // Get or create command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->primitiveCirclePipeline];
    [encoder setTexture:texture atIndex:0];

    int center[2] = {cx, cy};
    int rad = radius;
    uint32_t fillColor = (uint32_t)colorIndex;
    [encoder setBytes:&center length:sizeof(center) atIndex:0];
    [encoder setBytes:&rad length:sizeof(int) atIndex:1];
    [encoder setBytes:&fillColor length:sizeof(uint32_t) atIndex:2];

    // Dispatch threads to cover bounding box of circle
    int diameter = radius * 2 + 1;
    MTLSize gridSize = MTLSizeMake(320, 240, 1);  // XRES size
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::wresCircleFillGPU(int bufferIndex, int cx, int cy, int radius, uint8_t colorIndex) {
    if (!m_impl || !m_impl->primitiveCirclePipeline) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Circle pipeline not initialized");
        return;
    }

    if (bufferIndex < 0 || bufferIndex >= 4) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Invalid WRES buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->wresTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: WRES texture[%d] not available", bufferIndex);
        return;
    }

    // Get or create command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->primitiveCirclePipeline];
    [encoder setTexture:texture atIndex:0];

    int center[2] = {cx, cy};
    int rad = radius;
    uint32_t fillColor = (uint32_t)colorIndex;
    [encoder setBytes:&center length:sizeof(center) atIndex:0];
    [encoder setBytes:&rad length:sizeof(int) atIndex:1];
    [encoder setBytes:&fillColor length:sizeof(uint32_t) atIndex:2];

    // Dispatch threads to cover bounding box of circle
    MTLSize gridSize = MTLSizeMake(432, 240, 1);  // WRES size
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::xresLineGPU(int bufferIndex, int x0, int y0, int x1, int y1, uint8_t colorIndex) {
    if (!m_impl || !m_impl->primitiveLinePipeline) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Line pipeline not initialized");
        return;
    }

    if (bufferIndex < 0 || bufferIndex >= 4) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Invalid XRES buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->xresTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: XRES texture[%d] not available", bufferIndex);
        return;
    }

    // Get or create command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->primitiveLinePipeline];
    [encoder setTexture:texture atIndex:0];

    int lineCoords[4] = {x0, y0, x1, y1};
    uint32_t lineColor = (uint32_t)colorIndex;
    [encoder setBytes:&lineCoords length:sizeof(lineCoords) atIndex:0];
    [encoder setBytes:&lineColor length:sizeof(uint32_t) atIndex:1];

    // Dispatch threads to cover entire texture (line shader does bounds checking)
    MTLSize gridSize = MTLSizeMake(320, 240, 1);  // XRES size
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::wresLineGPU(int bufferIndex, int x0, int y0, int x1, int y1, uint8_t colorIndex) {
    if (!m_impl || !m_impl->primitiveLinePipeline) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Line pipeline not initialized");
        return;
    }

    if (bufferIndex < 0 || bufferIndex >= 4) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Invalid WRES buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->wresTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: WRES texture[%d] not available", bufferIndex);
        return;
    }

    // Get or create command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->primitiveLinePipeline];
    [encoder setTexture:texture atIndex:0];

    int lineCoords[4] = {x0, y0, x1, y1};
    uint32_t lineColor = (uint32_t)colorIndex;
    [encoder setBytes:&lineCoords length:sizeof(lineCoords) atIndex:0];
    [encoder setBytes:&lineColor length:sizeof(uint32_t) atIndex:1];

    // Dispatch threads to cover entire texture (line shader does bounds checking)
    MTLSize gridSize = MTLSizeMake(432, 240, 1);  // WRES size
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

// ============================================================================
// ANTI-ALIASED GPU PRIMITIVE DRAWING IMPLEMENTATIONS
// ============================================================================

void MetalRenderer::xresCircleFillAA(int bufferIndex, int cx, int cy, int radius, uint8_t colorIndex) {
    if (!m_impl || !m_impl->primitiveCircleAAPipeline) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Circle AA pipeline not initialized");
        return;
    }

    if (bufferIndex < 0 || bufferIndex >= 4) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Invalid XRES buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->xresTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: XRES texture[%d] not available", bufferIndex);
        return;
    }

    // Get or create command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->primitiveCircleAAPipeline];
    [encoder setTexture:texture atIndex:0];

    int center[2] = {cx, cy};
    int rad = radius;
    uint32_t fillColor = (uint32_t)colorIndex;
    [encoder setBytes:&center length:sizeof(center) atIndex:0];
    [encoder setBytes:&rad length:sizeof(int) atIndex:1];
    [encoder setBytes:&fillColor length:sizeof(uint32_t) atIndex:2];

    // Dispatch threads to cover bounding box of circle
    MTLSize gridSize = MTLSizeMake(320, 240, 1);  // XRES size
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::wresCircleFillAA(int bufferIndex, int cx, int cy, int radius, uint8_t colorIndex) {
    if (!m_impl || !m_impl->primitiveCircleAAPipeline) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Circle AA pipeline not initialized");
        return;
    }

    if (bufferIndex < 0 || bufferIndex >= 4) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Invalid WRES buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->wresTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: WRES texture[%d] not available", bufferIndex);
        return;
    }

    // Get or create command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->primitiveCircleAAPipeline];
    [encoder setTexture:texture atIndex:0];

    int center[2] = {cx, cy};
    int rad = radius;
    uint32_t fillColor = (uint32_t)colorIndex;
    [encoder setBytes:&center length:sizeof(center) atIndex:0];
    [encoder setBytes:&rad length:sizeof(int) atIndex:1];
    [encoder setBytes:&fillColor length:sizeof(uint32_t) atIndex:2];

    // Dispatch threads to cover bounding box of circle
    MTLSize gridSize = MTLSizeMake(432, 240, 1);  // WRES size
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::xresLineAA(int bufferIndex, int x0, int y0, int x1, int y1, uint8_t colorIndex, float lineWidth) {
    if (!m_impl || !m_impl->primitiveLineAAPipeline) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Line AA pipeline not initialized");
        return;
    }

    if (bufferIndex < 0 || bufferIndex >= 4) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Invalid XRES buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->xresTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: XRES texture[%d] not available", bufferIndex);
        return;
    }

    // Get or create command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->primitiveLineAAPipeline];
    [encoder setTexture:texture atIndex:0];

    int lineCoords[4] = {x0, y0, x1, y1};
    uint32_t lineColor = (uint32_t)colorIndex;
    [encoder setBytes:&lineCoords length:sizeof(lineCoords) atIndex:0];
    [encoder setBytes:&lineColor length:sizeof(uint32_t) atIndex:1];
    [encoder setBytes:&lineWidth length:sizeof(float) atIndex:2];

    // Dispatch threads to cover entire texture (line shader does bounds checking)
    MTLSize gridSize = MTLSizeMake(320, 240, 1);  // XRES size
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::wresLineAA(int bufferIndex, int x0, int y0, int x1, int y1, uint8_t colorIndex, float lineWidth) {
    if (!m_impl || !m_impl->primitiveLineAAPipeline) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Line AA pipeline not initialized");
        return;
    }

    if (bufferIndex < 0 || bufferIndex >= 4) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Invalid WRES buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->wresTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: WRES texture[%d] not available", bufferIndex);
        return;
    }

    // Get or create command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->primitiveLineAAPipeline];
    [encoder setTexture:texture atIndex:0];

    int lineCoords[4] = {x0, y0, x1, y1};
    uint32_t lineColor = (uint32_t)colorIndex;
    [encoder setBytes:&lineCoords length:sizeof(lineCoords) atIndex:0];
    [encoder setBytes:&lineColor length:sizeof(uint32_t) atIndex:1];
    [encoder setBytes:&lineWidth length:sizeof(float) atIndex:2];

    // Dispatch threads to cover entire texture (line shader does bounds checking)
    MTLSize gridSize = MTLSizeMake(432, 240, 1);  // WRES size
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

// ============================================================================
// PRES GPU IMPLEMENTATIONS (Premium Resolution 1280×720, 256-color palette)
// ============================================================================

void MetalRenderer::presBlitGPU(int srcBuffer, int dstBuffer,
                                 int srcX, int srcY, int width, int height,
                                 int dstX, int dstY) {
    if (!m_impl || !m_impl->blitterCopyPipeline) {
        NSLog(@"[GPU_BLITTER] ERROR: Blitter not initialized");
        return;
    }

    if (srcBuffer < 0 || srcBuffer > 7 || dstBuffer < 0 || dstBuffer > 7) {
        NSLog(@"[GPU_BLITTER] ERROR: Invalid PRES buffer indices: src=%d dst=%d", srcBuffer, dstBuffer);
        return;
    }

    // Get textures (PRES uses same format as XRES - R8Uint)
    id<MTLTexture> srcTexture = m_impl->presTextures[srcBuffer];
    id<MTLTexture> dstTexture = m_impl->presTextures[dstBuffer];

    if (!srcTexture || !dstTexture) {
        NSLog(@"[GPU_BLITTER] ERROR: Null PRES textures: src=%@, dst=%@", srcTexture, dstTexture);
        return;
    }

    // Use batch encoder if available, otherwise create one-off command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->blitterCopyPipeline];
    [encoder setTexture:srcTexture atIndex:0];
    [encoder setTexture:dstTexture atIndex:1];

    // Set parameters
    int sourceRect[4] = {srcX, srcY, width, height};
    int destPos[2] = {dstX, dstY};
    [encoder setBytes:&sourceRect length:sizeof(sourceRect) atIndex:0];
    [encoder setBytes:&destPos length:sizeof(destPos) atIndex:1];

    // Dispatch compute threads
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);
    MTLSize threadgroups = MTLSizeMake(
        (width + threadgroupSize.width - 1) / threadgroupSize.width,
        (height + threadgroupSize.height - 1) / threadgroupSize.height,
        1
    );

    [encoder dispatchThreadgroups:threadgroups threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::presBlitTransparentGPU(int srcBuffer, int dstBuffer,
                                           int srcX, int srcY, int width, int height,
                                           int dstX, int dstY, uint8_t transparentColor) {
    if (!m_impl || !m_impl->blitterTransparentPipeline) {
        NSLog(@"[GPU_BLITTER] ERROR: Transparent blitter not initialized");
        return;
    }

    if (srcBuffer < 0 || srcBuffer > 7 || dstBuffer < 0 || dstBuffer > 7) {
        NSLog(@"[GPU_BLITTER] ERROR: Invalid PRES buffer indices: src=%d dst=%d", srcBuffer, dstBuffer);
        return;
    }

    id<MTLTexture> srcTexture = m_impl->presTextures[srcBuffer];
    id<MTLTexture> dstTexture = m_impl->presTextures[dstBuffer];

    if (!srcTexture || !dstTexture) {
        NSLog(@"[GPU_BLITTER] ERROR: Null PRES textures (src=%p dst=%p)", srcTexture, dstTexture);
        return;
    }

    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->blitterTransparentPipeline];
    [encoder setTexture:srcTexture atIndex:0];
    [encoder setTexture:dstTexture atIndex:1];

    int sourceRect[4] = {srcX, srcY, width, height};
    int destPos[2] = {dstX, dstY};
    uint32_t transColor = (uint32_t)transparentColor;
    [encoder setBytes:&sourceRect length:sizeof(sourceRect) atIndex:0];
    [encoder setBytes:&destPos length:sizeof(destPos) atIndex:1];
    [encoder setBytes:&transColor length:sizeof(uint32_t) atIndex:2];

    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);
    MTLSize threadgroups = MTLSizeMake(
        (width + threadgroupSize.width - 1) / threadgroupSize.width,
        (height + threadgroupSize.height - 1) / threadgroupSize.height,
        1
    );

    [encoder dispatchThreadgroups:threadgroups threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::presClearGPU(int bufferIndex, uint8_t colorIndex) {
    if (!m_impl || !m_impl->blitterClearPipeline) {
        NSLog(@"[GPU_BLITTER] ERROR: Clear pipeline not initialized");
        return;
    }

    if (bufferIndex < 0 || bufferIndex >= 8) {
        NSLog(@"[GPU_BLITTER] ERROR: Invalid PRES buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->presTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[GPU_BLITTER] ERROR: PRES texture[%d] not available", bufferIndex);
        return;
    }

    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->blitterClearPipeline];
    [encoder setTexture:texture atIndex:0];

    uint32_t clearColor = (uint32_t)colorIndex;
    [encoder setBytes:&clearColor length:sizeof(uint32_t) atIndex:0];

    MTLSize gridSize = MTLSizeMake(1280, 720, 1);  // PRES size
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::presRectFillGPU(int bufferIndex, int x, int y, int width, int height, uint8_t colorIndex) {
    if (!m_impl || !m_impl->primitiveRectPipeline) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Rect pipeline not initialized");
        return;
    }

    if (!m_impl->commandQueue) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Command queue not available");
        return;
    }

    if (bufferIndex < 0 || bufferIndex >= 8) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Invalid PRES buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->presTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: PRES texture[%d] not available", bufferIndex);
        return;
    }

    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
        if (!encoder) {
            NSLog(@"[GPU_PRIMITIVES] ERROR: Batch encoder not available");
            return;
        }
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        if (!commandBuffer) {
            NSLog(@"[GPU_PRIMITIVES] ERROR: Failed to create command buffer");
            return;
        }
        encoder = [commandBuffer computeCommandEncoder];
        if (!encoder) {
            NSLog(@"[GPU_PRIMITIVES] ERROR: Failed to create compute encoder");
            return;
        }
    }

    [encoder setComputePipelineState:m_impl->primitiveRectPipeline];
    [encoder setTexture:texture atIndex:0];

    int rect[4] = {x, y, width, height};
    uint32_t fillColor = (uint32_t)colorIndex;
    [encoder setBytes:&rect length:sizeof(rect) atIndex:0];
    [encoder setBytes:&fillColor length:sizeof(uint32_t) atIndex:1];

    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);
    MTLSize threadgroups = MTLSizeMake(
        (width + threadgroupSize.width - 1) / threadgroupSize.width,
        (height + threadgroupSize.height - 1) / threadgroupSize.height,
        1
    );

    [encoder dispatchThreadgroups:threadgroups threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::presCircleFillGPU(int bufferIndex, int cx, int cy, int radius, uint8_t colorIndex) {
    if (!m_impl || !m_impl->primitiveCirclePipeline) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Circle pipeline not initialized");
        return;
    }

    if (bufferIndex < 0 || bufferIndex >= 8) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Invalid PRES buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->presTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: PRES texture[%d] not available", bufferIndex);
        return;
    }

    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->primitiveCirclePipeline];
    [encoder setTexture:texture atIndex:0];

    int center[2] = {cx, cy};
    int rad = radius;
    uint32_t fillColor = (uint32_t)colorIndex;
    [encoder setBytes:&center length:sizeof(center) atIndex:0];
    [encoder setBytes:&rad length:sizeof(int) atIndex:1];
    [encoder setBytes:&fillColor length:sizeof(uint32_t) atIndex:2];

    int diameter = radius * 2 + 1;
    MTLSize gridSize = MTLSizeMake(1280, 720, 1);  // PRES size
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::presLineGPU(int bufferIndex, int x0, int y0, int x1, int y1, uint8_t colorIndex) {
    if (!m_impl || !m_impl->primitiveLinePipeline) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Line pipeline not initialized");
        return;
    }

    if (bufferIndex < 0 || bufferIndex >= 8) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Invalid PRES buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->presTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: PRES texture[%d] not available", bufferIndex);
        return;
    }

    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->primitiveLinePipeline];
    [encoder setTexture:texture atIndex:0];

    int lineCoords[4] = {x0, y0, x1, y1};
    uint32_t lineColor = (uint32_t)colorIndex;
    [encoder setBytes:&lineCoords length:sizeof(lineCoords) atIndex:0];
    [encoder setBytes:&lineColor length:sizeof(uint32_t) atIndex:1];

    MTLSize gridSize = MTLSizeMake(1280, 720, 1);  // PRES size
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::presCircleFillAA(int bufferIndex, int cx, int cy, int radius, uint8_t colorIndex) {
    if (!m_impl || !m_impl->primitiveCircleAAPipeline) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Circle AA pipeline not initialized");
        return;
    }

    if (!m_impl->commandQueue) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Command queue not available");
        return;
    }

    if (bufferIndex < 0 || bufferIndex >= 8) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Invalid PRES buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->presTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: PRES texture[%d] not available", bufferIndex);
        return;
    }

    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
        if (!encoder) {
            NSLog(@"[GPU_PRIMITIVES] ERROR: Batch encoder not available");
            return;
        }
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        if (!commandBuffer) {
            NSLog(@"[GPU_PRIMITIVES] ERROR: Failed to create command buffer");
            return;
        }
        encoder = [commandBuffer computeCommandEncoder];
        if (!encoder) {
            NSLog(@"[GPU_PRIMITIVES] ERROR: Failed to create compute encoder");
            return;
        }
    }

    [encoder setComputePipelineState:m_impl->primitiveCircleAAPipeline];
    [encoder setTexture:texture atIndex:0];

    int center[2] = {cx, cy};
    int rad = radius;
    uint32_t fillColor = (uint32_t)colorIndex;
    [encoder setBytes:&center length:sizeof(center) atIndex:0];
    [encoder setBytes:&rad length:sizeof(int) atIndex:1];
    [encoder setBytes:&fillColor length:sizeof(uint32_t) atIndex:2];

    int diameter = radius * 2 + 8;
    MTLSize gridSize = MTLSizeMake(1280, 720, 1);  // PRES size
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::presLineAA(int bufferIndex, int x0, int y0, int x1, int y1, uint8_t colorIndex, float lineWidth) {
    if (!m_impl || !m_impl->primitiveLineAAPipeline) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Line AA pipeline not initialized");
        return;
    }

    if (!m_impl->commandQueue) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Command queue not available");
        return;
    }

    if (bufferIndex < 0 || bufferIndex >= 8) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: Invalid PRES buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->presTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[GPU_PRIMITIVES] ERROR: PRES texture[%d] not available", bufferIndex);
        return;
    }

    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
        if (!encoder) {
            NSLog(@"[GPU_PRIMITIVES] ERROR: Batch encoder not available");
            return;
        }
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        if (!commandBuffer) {
            NSLog(@"[GPU_PRIMITIVES] ERROR: Failed to create command buffer");
            return;
        }
        encoder = [commandBuffer computeCommandEncoder];
        if (!encoder) {
            NSLog(@"[GPU_PRIMITIVES] ERROR: Failed to create compute encoder");
            return;
        }
    }

    [encoder setComputePipelineState:m_impl->primitiveLineAAPipeline];
    [encoder setTexture:texture atIndex:0];

    int lineCoords[4] = {x0, y0, x1, y1};
    uint32_t lineColor = (uint32_t)colorIndex;
    [encoder setBytes:&lineCoords length:sizeof(lineCoords) atIndex:0];
    [encoder setBytes:&lineColor length:sizeof(uint32_t) atIndex:1];
    [encoder setBytes:&lineWidth length:sizeof(float) atIndex:2];

    MTLSize gridSize = MTLSizeMake(1280, 720, 1);  // PRES size
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

// ============================================================================
// URES GPU BLITTER IMPLEMENTATIONS (Direct Color ARGB4444)
// ============================================================================

void MetalRenderer::uresBlitCopyGPU(int srcBufferIndex, int dstBufferIndex, int srcX, int srcY, int width, int height, int dstX, int dstY) {
    if (!m_impl || !m_impl->uresBlitterCopyPipeline) {
        NSLog(@"[URES_GPU_BLITTER] ERROR: Copy pipeline not initialized");
        return;
    }

    if (srcBufferIndex < 0 || srcBufferIndex >= 8 || dstBufferIndex < 0 || dstBufferIndex >= 8) {
        NSLog(@"[URES_GPU_BLITTER] ERROR: Invalid buffer index: src=%d, dst=%d", srcBufferIndex, dstBufferIndex);
        return;
    }

    id<MTLTexture> srcTexture = m_impl->uresTextures[srcBufferIndex];
    id<MTLTexture> dstTexture = m_impl->uresTextures[dstBufferIndex];

    if (!srcTexture || !dstTexture) {
        NSLog(@"[URES_GPU_BLITTER] ERROR: Textures not available");
        return;
    }

    // Get or create command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->uresBlitterCopyPipeline];
    [encoder setTexture:srcTexture atIndex:0];
    [encoder setTexture:dstTexture atIndex:1];

    int srcRect[4] = {srcX, srcY, width, height};
    int dstPos[2] = {dstX, dstY};
    [encoder setBytes:&srcRect length:sizeof(srcRect) atIndex:0];
    [encoder setBytes:&dstPos length:sizeof(dstPos) atIndex:1];

    MTLSize gridSize = MTLSizeMake(width, height, 1);
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::uresBlitTransparentGPU(int srcBufferIndex, int dstBufferIndex, int srcX, int srcY, int width, int height, int dstX, int dstY) {
    if (!m_impl || !m_impl->uresBlitterTransparentPipeline) {
        NSLog(@"[URES_GPU_BLITTER] ERROR: Transparent pipeline not initialized");
        return;
    }

    if (srcBufferIndex < 0 || srcBufferIndex >= 8 || dstBufferIndex < 0 || dstBufferIndex >= 8) {
        NSLog(@"[URES_GPU_BLITTER] ERROR: Invalid buffer index: src=%d, dst=%d", srcBufferIndex, dstBufferIndex);
        return;
    }

    id<MTLTexture> srcTexture = m_impl->uresTextures[srcBufferIndex];
    id<MTLTexture> dstTexture = m_impl->uresTextures[dstBufferIndex];

    if (!srcTexture || !dstTexture) {
        NSLog(@"[URES_GPU_BLITTER] ERROR: Textures not available");
        return;
    }

    // Get or create command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->uresBlitterTransparentPipeline];
    [encoder setTexture:srcTexture atIndex:0];
    [encoder setTexture:dstTexture atIndex:1];

    int srcRect[4] = {srcX, srcY, width, height};
    int dstPos[2] = {dstX, dstY};
    [encoder setBytes:&srcRect length:sizeof(srcRect) atIndex:0];
    [encoder setBytes:&dstPos length:sizeof(dstPos) atIndex:1];

    MTLSize gridSize = MTLSizeMake(width, height, 1);
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::uresBlitAlphaCompositeGPU(int srcBufferIndex, int dstBufferIndex, int srcX, int srcY, int width, int height, int dstX, int dstY) {
    if (!m_impl || !m_impl->uresBlitterAlphaCompositePipeline) {
        NSLog(@"[URES_GPU_BLITTER] ERROR: Alpha composite pipeline not initialized");
        return;
    }

    if (srcBufferIndex < 0 || srcBufferIndex >= 8 || dstBufferIndex < 0 || dstBufferIndex >= 8) {
        NSLog(@"[URES_GPU_BLITTER] ERROR: Invalid buffer index: src=%d, dst=%d", srcBufferIndex, dstBufferIndex);
        return;
    }

    id<MTLTexture> srcTexture = m_impl->uresTextures[srcBufferIndex];
    id<MTLTexture> dstTexture = m_impl->uresTextures[dstBufferIndex];

    if (!srcTexture || !dstTexture) {
        NSLog(@"[URES_GPU_BLITTER] ERROR: Textures not available");
        return;
    }

    // Get or create command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->uresBlitterAlphaCompositePipeline];
    [encoder setTexture:srcTexture atIndex:0];
    [encoder setTexture:dstTexture atIndex:1];

    int srcRect[4] = {srcX, srcY, width, height};
    int dstPos[2] = {dstX, dstY};
    [encoder setBytes:&srcRect length:sizeof(srcRect) atIndex:0];
    [encoder setBytes:&dstPos length:sizeof(dstPos) atIndex:1];

    MTLSize gridSize = MTLSizeMake(width, height, 1);
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::uresClearGPU(int bufferIndex, uint16_t color) {
    if (!m_impl || !m_impl->uresBlitterClearPipeline) {
        NSLog(@"[URES_GPU_BLITTER] ERROR: Clear pipeline not initialized");
        return;
    }

    if (bufferIndex < 0 || bufferIndex >= 8) {
        NSLog(@"[URES_GPU_BLITTER] ERROR: Invalid buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->uresTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[URES_GPU_BLITTER] ERROR: Texture[%d] not available", bufferIndex);
        return;
    }

    // Get or create command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->uresBlitterClearPipeline];
    [encoder setTexture:texture atIndex:0];
    [encoder setBytes:&color length:sizeof(uint16_t) atIndex:0];

    MTLSize gridSize = MTLSizeMake(1280, 720, 1);  // URES size
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

// ============================================================================
// URES GPU PRIMITIVE DRAWING IMPLEMENTATIONS (Direct Color ARGB4444)
// ============================================================================

void MetalRenderer::uresRectFillGPU(int bufferIndex, int x, int y, int width, int height, uint16_t color) {
    if (!m_impl || !m_impl->uresPrimitiveRectPipeline) {
        NSLog(@"[URES_GPU_PRIMITIVES] ERROR: Rect pipeline not initialized");
        return;
    }

    if (bufferIndex < 0 || bufferIndex >= 8) {
        NSLog(@"[URES_GPU_PRIMITIVES] ERROR: Invalid buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->uresTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[URES_GPU_PRIMITIVES] ERROR: Texture[%d] not available", bufferIndex);
        return;
    }

    // Get or create command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->uresPrimitiveRectPipeline];
    [encoder setTexture:texture atIndex:0];

    int rect[4] = {x, y, width, height};
    [encoder setBytes:&rect length:sizeof(rect) atIndex:0];
    [encoder setBytes:&color length:sizeof(uint16_t) atIndex:1];

    MTLSize gridSize = MTLSizeMake(1280, 720, 1);  // URES size
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::uresCircleFillGPU(int bufferIndex, int cx, int cy, int radius, uint16_t color) {
    if (!m_impl || !m_impl->uresPrimitiveCirclePipeline) {
        NSLog(@"[URES_GPU_PRIMITIVES] ERROR: Circle pipeline not initialized");
        return;
    }

    if (bufferIndex < 0 || bufferIndex >= 8) {
        NSLog(@"[URES_GPU_PRIMITIVES] ERROR: Invalid buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->uresTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[URES_GPU_PRIMITIVES] ERROR: Texture[%d] not available", bufferIndex);
        return;
    }

    // Get or create command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->uresPrimitiveCirclePipeline];
    [encoder setTexture:texture atIndex:0];

    int center[2] = {cx, cy};
    float rad = (float)radius;
    [encoder setBytes:&center length:sizeof(center) atIndex:0];
    [encoder setBytes:&rad length:sizeof(float) atIndex:1];
    [encoder setBytes:&color length:sizeof(uint16_t) atIndex:2];

    MTLSize gridSize = MTLSizeMake(1280, 720, 1);  // URES size
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::uresLineGPU(int bufferIndex, int x0, int y0, int x1, int y1, uint16_t color) {
    if (!m_impl || !m_impl->uresPrimitiveLinePipeline) {
        NSLog(@"[URES_GPU_PRIMITIVES] ERROR: Line pipeline not initialized");
        return;
    }

    if (bufferIndex < 0 || bufferIndex >= 8) {
        NSLog(@"[URES_GPU_PRIMITIVES] ERROR: Invalid buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->uresTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[URES_GPU_PRIMITIVES] ERROR: Texture[%d] not available", bufferIndex);
        return;
    }

    // Get or create command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->uresPrimitiveLinePipeline];
    [encoder setTexture:texture atIndex:0];

    int lineCoords[4] = {x0, y0, x1, y1};
    float lineWidth = 1.0f;
    [encoder setBytes:&lineCoords length:sizeof(lineCoords) atIndex:0];
    [encoder setBytes:&color length:sizeof(uint16_t) atIndex:1];
    [encoder setBytes:&lineWidth length:sizeof(float) atIndex:2];

    MTLSize gridSize = MTLSizeMake(1280, 720, 1);  // URES size
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

// ============================================================================
// URES ANTI-ALIASED GPU PRIMITIVE DRAWING IMPLEMENTATIONS
// ============================================================================

void MetalRenderer::uresCircleFillAA(int bufferIndex, int cx, int cy, int radius, uint16_t color) {
    if (!m_impl || !m_impl->uresPrimitiveCircleAAPipeline) {
        NSLog(@"[URES_GPU_PRIMITIVES] ERROR: Circle AA pipeline not initialized");
        return;
    }

    if (bufferIndex < 0 || bufferIndex >= 8) {
        NSLog(@"[URES_GPU_PRIMITIVES] ERROR: Invalid buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->uresTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[URES_GPU_PRIMITIVES] ERROR: Texture[%d] not available", bufferIndex);
        return;
    }

    // Get or create command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->uresPrimitiveCircleAAPipeline];
    [encoder setTexture:texture atIndex:0];

    int center[2] = {cx, cy};
    float rad = (float)radius;
    [encoder setBytes:&center length:sizeof(center) atIndex:0];
    [encoder setBytes:&rad length:sizeof(float) atIndex:1];
    [encoder setBytes:&color length:sizeof(uint16_t) atIndex:2];

    MTLSize gridSize = MTLSizeMake(1280, 720, 1);  // URES size
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::uresLineAA(int bufferIndex, int x0, int y0, int x1, int y1, uint16_t color, float lineWidth) {
    if (!m_impl || !m_impl->uresPrimitiveLineAAPipeline) {
        NSLog(@"[URES_GPU_PRIMITIVES] ERROR: Line AA pipeline not initialized");
        return;
    }

    if (bufferIndex < 0 || bufferIndex >= 8) {
        NSLog(@"[URES_GPU_PRIMITIVES] ERROR: Invalid buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->uresTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[URES_GPU_PRIMITIVES] ERROR: Texture[%d] not available", bufferIndex);
        return;
    }

    // Get or create command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->uresPrimitiveLineAAPipeline];
    [encoder setTexture:texture atIndex:0];

    int lineCoords[4] = {x0, y0, x1, y1};
    [encoder setBytes:&lineCoords length:sizeof(lineCoords) atIndex:0];
    [encoder setBytes:&color length:sizeof(uint16_t) atIndex:1];
    [encoder setBytes:&lineWidth length:sizeof(float) atIndex:2];

    MTLSize gridSize = MTLSizeMake(1280, 720, 1);  // URES size
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

// =============================================================================
// URES GPU GRADIENT PRIMITIVES
// =============================================================================

void MetalRenderer::uresRectFillGradientGPU(int bufferIndex, int x, int y, int width, int height,
                                            uint16_t colorTopLeft, uint16_t colorTopRight,
                                            uint16_t colorBottomLeft, uint16_t colorBottomRight) {
    if (!m_impl || !m_impl->uresPrimitiveRectGradientPipeline) {
        NSLog(@"[URES_GPU_GRADIENTS] ERROR: Rect gradient pipeline not initialized");
        return;
    }

    if (bufferIndex < 0 || bufferIndex >= 8) {
        NSLog(@"[URES_GPU_GRADIENTS] ERROR: Invalid buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->uresTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[URES_GPU_GRADIENTS] ERROR: Texture[%d] not available", bufferIndex);
        return;
    }

    // Get or create command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->uresPrimitiveRectGradientPipeline];
    [encoder setTexture:texture atIndex:0];

    int rect[4] = {x, y, width, height};
    uint16_t cornerColors[4] = {colorTopLeft, colorTopRight, colorBottomLeft, colorBottomRight};

    [encoder setBytes:&rect length:sizeof(rect) atIndex:0];
    [encoder setBytes:&cornerColors length:sizeof(cornerColors) atIndex:1];

    MTLSize gridSize = MTLSizeMake(1280, 720, 1);  // URES size
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::uresCircleFillGradientGPU(int bufferIndex, int cx, int cy, int radius,
                                              uint16_t centerColor, uint16_t edgeColor) {
    if (!m_impl || !m_impl->uresPrimitiveCircleGradientPipeline) {
        NSLog(@"[URES_GPU_GRADIENTS] ERROR: Circle gradient pipeline not initialized");
        return;
    }

    if (bufferIndex < 0 || bufferIndex >= 8) {
        NSLog(@"[URES_GPU_GRADIENTS] ERROR: Invalid buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->uresTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[URES_GPU_GRADIENTS] ERROR: Texture[%d] not available", bufferIndex);
        return;
    }

    // Get or create command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->uresPrimitiveCircleGradientPipeline];
    [encoder setTexture:texture atIndex:0];

    int center[2] = {cx, cy};
    float rad = (float)radius;

    [encoder setBytes:&center length:sizeof(center) atIndex:0];
    [encoder setBytes:&rad length:sizeof(float) atIndex:1];
    [encoder setBytes:&centerColor length:sizeof(uint16_t) atIndex:2];
    [encoder setBytes:&edgeColor length:sizeof(uint16_t) atIndex:3];

    MTLSize gridSize = MTLSizeMake(1280, 720, 1);  // URES size
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::uresCircleFillGradientAA(int bufferIndex, int cx, int cy, int radius,
                                             uint16_t centerColor, uint16_t edgeColor) {
    if (!m_impl || !m_impl->uresPrimitiveCircleGradientAAPipeline) {
        NSLog(@"[URES_GPU_GRADIENTS] ERROR: Circle gradient AA pipeline not initialized");
        return;
    }

    if (bufferIndex < 0 || bufferIndex >= 8) {
        NSLog(@"[URES_GPU_GRADIENTS] ERROR: Invalid buffer index: %d", bufferIndex);
        return;
    }

    id<MTLTexture> texture = m_impl->uresTextures[bufferIndex];
    if (!texture) {
        NSLog(@"[URES_GPU_GRADIENTS] ERROR: Texture[%d] not available", bufferIndex);
        return;
    }

    // Get or create command buffer
    id<MTLCommandBuffer> commandBuffer;
    id<MTLComputeCommandEncoder> encoder;
    bool usingBatch = m_impl->inBlitBatch;

    if (usingBatch) {
        encoder = m_impl->batchEncoder;
    } else {
        commandBuffer = [m_impl->commandQueue commandBuffer];
        encoder = [commandBuffer computeCommandEncoder];
    }

    [encoder setComputePipelineState:m_impl->uresPrimitiveCircleGradientAAPipeline];
    [encoder setTexture:texture atIndex:0];

    int center[2] = {cx, cy};
    float rad = (float)radius;

    [encoder setBytes:&center length:sizeof(center) atIndex:0];
    [encoder setBytes:&rad length:sizeof(float) atIndex:1];
    [encoder setBytes:&centerColor length:sizeof(uint16_t) atIndex:2];
    [encoder setBytes:&edgeColor length:sizeof(uint16_t) atIndex:3];

    MTLSize gridSize = MTLSizeMake(1280, 720, 1);  // URES size
    MTLSize threadgroupSize = MTLSizeMake(16, 16, 1);

    [encoder dispatchThreads:gridSize threadsPerThreadgroup:threadgroupSize];

    if (!usingBatch) {
        [encoder endEncoding];
        [commandBuffer commit];
    }
}

void MetalRenderer::uresGPUFlip() {
    if (!m_impl) {
        NSLog(@"[URES] ERROR: Renderer not initialized");
        return;
    }

    // Swap GPU texture pointers (buffers 0 and 1)
    std::swap(m_impl->uresTextures[0], m_impl->uresTextures[1]);

    // NSLog(@"[URES] GPU buffers flipped (swapped textures 0 and 1)");
}

void MetalRenderer::presGPUFlip() {
    if (!m_impl) {
        NSLog(@"[PRES] ERROR: Renderer not initialized");
        return;
    }

    // Swap GPU texture pointers (buffers 0 and 1)
    std::swap(m_impl->presTextures[0], m_impl->presTextures[1]);
}

} // namespace SuperTerminal
