//
// SpriteManager.mm
// SuperTerminal v2
//
// Sprite management system implementation.
// 16 million colour sprites in their own layer.

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <simd/simd.h>
#include "SpriteManager.h"
#include "SpriteCompression.h"
#include "../Data/PaletteLibrary.h"
#include <unordered_map>
#include <set>
#include <cmath>

namespace SuperTerminal {

// =============================================================================
// Sprite Command Data
// =============================================================================

struct SpriteCommandData {
    SpriteCommand type;
    uint16_t spriteId;
    float x, y;           // Position or scale
    float value;          // Single value (rotation/alpha)
    float r, g, b, a;     // Tint color

    SpriteCommandData()
        : type(SpriteCommand::Show)
        , spriteId(INVALID_SPRITE_ID)
        , x(0.0f), y(0.0f)
        , value(0.0f)
        , r(1.0f), g(1.0f), b(1.0f), a(1.0f)
    {}
};

// =============================================================================
// Sprite Vertex and Uniforms
// =============================================================================

struct SpriteVertex {
    simd_float2 position;
    simd_float2 texCoord;
};

struct SpriteUniforms {
    simd_float4x4 modelMatrix;
    simd_float4x4 viewProjectionMatrix;
    simd_float4 color; // Tint color + alpha
};

// =============================================================================
// Internal Implementation
// =============================================================================

struct SpriteManager::Impl {
    id<MTLDevice> device;
    id<MTLRenderPipelineState> pipelineState;
    id<MTLRenderPipelineState> pipelineStateIndexed;  // For indexed sprites
    id<MTLBuffer> vertexBuffer;
    id<MTLBuffer> uniformsBuffer;
    id<MTLSamplerState> samplerState;
    id<MTLSamplerState> samplerStateIndexed;  // Nearest neighbor for index lookup

    // Standard palette textures (32 pre-loaded palettes)
    id<MTLTexture> standardPaletteTextures[32];

    // Sprite storage
    std::unordered_map<uint16_t, std::unique_ptr<Sprite>> sprites;
    std::unordered_map<uint16_t, std::unique_ptr<SpriteIndexed>> spritesIndexed;
    std::vector<uint16_t> renderOrder;

    // Command queue
    std::vector<SpriteCommandData> commandQueue;
    mutable std::mutex mutex;

    // Sprite ID management
    std::set<uint16_t> freeIds;
    uint16_t nextId;

    Impl(MTLDevicePtr devicePtr)
        : device(devicePtr)
        , pipelineState(nil)
        , pipelineStateIndexed(nil)
        , vertexBuffer(nil)
        , uniformsBuffer(nil)
        , samplerState(nil)
        , samplerStateIndexed(nil)
        , nextId(1)
    {
        // Initialize standard palette textures to nil
        for (int i = 0; i < 32; i++) {
            standardPaletteTextures[i] = nil;
        }
    }

    ~Impl() {
        sprites.clear();
        spritesIndexed.clear();
        renderOrder.clear();
        commandQueue.clear();
        freeIds.clear();

        vertexBuffer = nil;
        uniformsBuffer = nil;
        pipelineState = nil;
        pipelineStateIndexed = nil;
        samplerState = nil;
        samplerStateIndexed = nil;

        // Release standard palette textures
        for (int i = 0; i < 32; i++) {
            standardPaletteTextures[i] = nil;
        }

        device = nil;
    }
};

// =============================================================================
// Constructor / Destructor
// =============================================================================

SpriteManager::SpriteManager(MTLDevicePtr device)
    : m_impl(std::make_unique<Impl>(device))
    , m_dirty(true)  // Start dirty to ensure first render
    , m_dirtySprites(MAX_SPRITES, false)
    , m_batchUpdate(false)
{
    createVertexBuffer();
    createSamplerState();
    createRenderPipeline();

    NSLog(@"[SpriteManager] Creating indexed render pipeline...");
    bool indexedResult = createIndexedRenderPipeline();
    NSLog(@"[SpriteManager] Indexed pipeline creation: %s", indexedResult ? "SUCCESS" : "FAILED");
    if (m_impl->pipelineStateIndexed) {
        NSLog(@"[SpriteManager] pipelineStateIndexed = %p", m_impl->pipelineStateIndexed);
    } else {
        NSLog(@"[SpriteManager] ERROR: pipelineStateIndexed is NULL!");
    }

    // Initialize standard palette textures
    NSLog(@"[SpriteManager] Initializing standard palette textures...");
    initializeStandardPaletteTextures();
}

SpriteManager::~SpriteManager() = default;

SpriteManager::SpriteManager(SpriteManager&& other) noexcept
    : m_impl(std::move(other.m_impl))
    , m_dirty(other.m_dirty)
    , m_dirtySprites(std::move(other.m_dirtySprites))
    , m_batchUpdate(other.m_batchUpdate)
{
}

SpriteManager& SpriteManager::operator=(SpriteManager&& other) noexcept {
    if (this != &other) {
        m_impl = std::move(other.m_impl);
        m_dirty = other.m_dirty;
        m_dirtySprites = std::move(other.m_dirtySprites);
        m_batchUpdate = other.m_batchUpdate;
    }
    return *this;
}

// =============================================================================
// Pipeline Creation
// =============================================================================

bool SpriteManager::createRenderPipeline() {
    NSError* error = nil;

    // Inline Metal shader source
    NSString* shaderSource = @""
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "\n"
    "struct VertexIn {\n"
    "    float2 position [[attribute(0)]];\n"
    "    float2 texCoord [[attribute(1)]];\n"
    "};\n"
    "\n"
    "struct VertexOut {\n"
    "    float4 position [[position]];\n"
    "    float2 texCoord;\n"
    "    float4 color;\n"
    "};\n"
    "\n"
    "struct SpriteUniforms {\n"
    "    float4x4 modelMatrix;\n"
    "    float4x4 viewProjectionMatrix;\n"
    "    float4 color;\n"
    "};\n"
    "\n"
    "vertex VertexOut sprite_vertex(VertexIn in [[stage_in]],\n"
    "                              constant SpriteUniforms& uniforms [[buffer(1)]]) {\n"
    "    VertexOut out;\n"
    "    float4 worldPos = uniforms.modelMatrix * float4(in.position, 0.0, 1.0);\n"
    "    out.position = uniforms.viewProjectionMatrix * worldPos;\n"
    "    out.texCoord = in.texCoord;\n"
    "    out.color = uniforms.color;\n"
    "    return out;\n"
    "}\n"
    "\n"
    "fragment float4 sprite_fragment(VertexOut in [[stage_in]],\n"
    "                               texture2d<float> spriteTexture [[texture(0)]],\n"
    "                               sampler texSampler [[sampler(0)]]) {\n"
    "    float4 texColor = spriteTexture.sample(texSampler, in.texCoord);\n"
    "    return texColor * in.color;\n"
    "}\n";

    id<MTLLibrary> library = [m_impl->device newLibraryWithSource:shaderSource
                                                          options:nil
                                                            error:&error];
    if (error) {
        NSLog(@"SpriteManager: Failed to create shader library: %@", error);
        return false;
    }

    id<MTLFunction> vertexFunction = [library newFunctionWithName:@"sprite_vertex"];
    id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"sprite_fragment"];

    // Create pipeline descriptor
    MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];
    desc.label = @"Sprite Pipeline";
    desc.vertexFunction = vertexFunction;
    desc.fragmentFunction = fragmentFunction;
    desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

    // Enable alpha blending
    desc.colorAttachments[0].blendingEnabled = YES;
    desc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    desc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    desc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
    desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    desc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

    // Vertex descriptor
    MTLVertexDescriptor* vertexDesc = [[MTLVertexDescriptor alloc] init];
    vertexDesc.attributes[0].format = MTLVertexFormatFloat2;
    vertexDesc.attributes[0].offset = 0;
    vertexDesc.attributes[0].bufferIndex = 0;
    vertexDesc.attributes[1].format = MTLVertexFormatFloat2;
    vertexDesc.attributes[1].offset = sizeof(simd_float2);
    vertexDesc.attributes[1].bufferIndex = 0;
    vertexDesc.layouts[0].stride = sizeof(SpriteVertex);
    vertexDesc.layouts[0].stepRate = 1;
    vertexDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

    desc.vertexDescriptor = vertexDesc;

    m_impl->pipelineState = [m_impl->device newRenderPipelineStateWithDescriptor:desc error:&error];
    if (error) {
        NSLog(@"SpriteManager: Failed to create pipeline state: %@", error);
        return false;
    }

    return true;
}

void SpriteManager::createVertexBuffer() {
    // Create quad geometry: 128x128 centered at origin
    SpriteVertex vertices[] = {
        {{-64.0f, -64.0f}, {0.0f, 1.0f}}, // Bottom left
        {{ 64.0f, -64.0f}, {1.0f, 1.0f}}, // Bottom right
        {{-64.0f,  64.0f}, {0.0f, 0.0f}}, // Top left

        {{ 64.0f, -64.0f}, {1.0f, 1.0f}}, // Bottom right
        {{ 64.0f,  64.0f}, {1.0f, 0.0f}}, // Top right
        {{-64.0f,  64.0f}, {0.0f, 0.0f}}  // Top left
    };

    m_impl->vertexBuffer = [m_impl->device newBufferWithBytes:vertices
                                                       length:sizeof(vertices)
                                                      options:MTLResourceStorageModeShared];
    m_impl->vertexBuffer.label = @"Sprite Vertices";

    // Create uniforms buffer for multiple sprites
    NSUInteger uniformsSize = sizeof(SpriteUniforms) * MAX_SPRITES;
    m_impl->uniformsBuffer = [m_impl->device newBufferWithLength:uniformsSize
                                                         options:MTLResourceStorageModeShared];
    m_impl->uniformsBuffer.label = @"Sprite Uniforms";
}

void SpriteManager::createSamplerState() {
    MTLSamplerDescriptor* desc = [[MTLSamplerDescriptor alloc] init];
    desc.minFilter = MTLSamplerMinMagFilterLinear;
    desc.magFilter = MTLSamplerMinMagFilterLinear;
    desc.mipFilter = MTLSamplerMipFilterNotMipmapped;
    desc.sAddressMode = MTLSamplerAddressModeClampToEdge;
    desc.tAddressMode = MTLSamplerAddressModeClampToEdge;
    desc.normalizedCoordinates = YES;

    m_impl->samplerState = [m_impl->device newSamplerStateWithDescriptor:desc];

    // Create nearest-neighbor sampler for indexed sprites
    MTLSamplerDescriptor* nearestDesc = [[MTLSamplerDescriptor alloc] init];
    nearestDesc.minFilter = MTLSamplerMinMagFilterNearest;
    nearestDesc.magFilter = MTLSamplerMinMagFilterNearest;
    nearestDesc.mipFilter = MTLSamplerMipFilterNotMipmapped;
    nearestDesc.sAddressMode = MTLSamplerAddressModeClampToEdge;
    nearestDesc.tAddressMode = MTLSamplerAddressModeClampToEdge;
    nearestDesc.normalizedCoordinates = YES;

    m_impl->samplerStateIndexed = [m_impl->device newSamplerStateWithDescriptor:nearestDesc];
}

// =============================================================================
// Indexed Sprite Pipeline
// =============================================================================

bool SpriteManager::createIndexedRenderPipeline() {
    NSError* error = nil;

    // Inline Metal shader source for indexed sprites
    NSString* shaderSource = @""
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "\n"
    "struct VertexIn {\n"
    "    float2 position [[attribute(0)]];\n"
    "    float2 texCoord [[attribute(1)]];\n"
    "};\n"
    "\n"
    "struct VertexOut {\n"
    "    float4 position [[position]];\n"
    "    float2 texCoord;\n"
    "    float alpha;\n"
    "};\n"
    "\n"
    "struct SpriteUniforms {\n"
    "    float4x4 modelMatrix;\n"
    "    float4x4 viewProjectionMatrix;\n"
    "    float4 color;\n"
    "};\n"
    "\n"
    "vertex VertexOut sprite_vertex_indexed(VertexIn in [[stage_in]],\n"
    "                                       constant SpriteUniforms& uniforms [[buffer(1)]]) {\n"
    "    VertexOut out;\n"
    "    float4 worldPos = uniforms.modelMatrix * float4(in.position, 0.0, 1.0);\n"
    "    out.position = uniforms.viewProjectionMatrix * worldPos;\n"
    "    out.texCoord = in.texCoord;\n"
    "    out.alpha = uniforms.color.a;\n"
    "    return out;\n"
    "}\n"
    "\n"
    "fragment float4 sprite_fragment_indexed(VertexOut in [[stage_in]],\n"
    "                                        texture2d<uint> indexTexture [[texture(0)]],\n"
    "                                        texture2d<float> paletteTexture [[texture(1)]],\n"
    "                                        sampler texSampler [[sampler(0)]]) {\n"
    "    // Sample index texture to get color index (0-15)\n"
    "    // Flip Y coordinate to correct upside-down rendering\n"
    "    float2 flippedTexCoord = float2(in.texCoord.x, 1.0 - in.texCoord.y);\n"
    "    uint2 indexCoord = uint2(flippedTexCoord * float2(indexTexture.get_width(), indexTexture.get_height()));\n"
    "    uint colorIndex = indexTexture.read(indexCoord).r;\n"
    "    \n"
    "    // Index 0 is transparent - do not draw anything\n"
    "    if (colorIndex == 0) {\n"
    "        discard_fragment();\n"
    "    }\n"
    "    \n"
    "    // Sample palette texture (16 colors in a 16x1 texture)\n"
    "    // Use direct read instead of sample for more accurate palette lookup\n"
    "    uint2 paletteCoord = uint2(colorIndex, 0);\n"
    "    float4 color = paletteTexture.read(paletteCoord);\n"
    "    \n"
    "    // Apply alpha from uniforms\n"
    "    color.a *= in.alpha;\n"
    "    \n"
    "    // Discard fully transparent pixels\n"
    "    if (color.a < 0.01) {\n"
    "        discard_fragment();\n"
    "    }\n"
    "    \n"
    "    return color;\n"
    "}\n";

    id<MTLLibrary> library = [m_impl->device newLibraryWithSource:shaderSource
                                                          options:nil
                                                            error:&error];
    if (error) {
        NSLog(@"SpriteManager: Failed to create indexed shader library: %@", error);
        return false;
    }

    id<MTLFunction> vertexFunction = [library newFunctionWithName:@"sprite_vertex_indexed"];
    id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"sprite_fragment_indexed"];

    // Create pipeline descriptor
    MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];
    desc.label = @"Indexed Sprite Pipeline";
    desc.vertexFunction = vertexFunction;
    desc.fragmentFunction = fragmentFunction;
    desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

    // Enable alpha blending
    desc.colorAttachments[0].blendingEnabled = YES;
    desc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    desc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    desc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
    desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    desc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

    // Vertex descriptor (same as regular sprites)
    MTLVertexDescriptor* vertexDesc = [[MTLVertexDescriptor alloc] init];
    vertexDesc.attributes[0].format = MTLVertexFormatFloat2;
    vertexDesc.attributes[0].offset = 0;
    vertexDesc.attributes[0].bufferIndex = 0;
    vertexDesc.attributes[1].format = MTLVertexFormatFloat2;
    vertexDesc.attributes[1].offset = sizeof(simd_float2);
    vertexDesc.attributes[1].bufferIndex = 0;
    vertexDesc.layouts[0].stride = sizeof(SpriteVertex);
    vertexDesc.layouts[0].stepRate = 1;
    vertexDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

    desc.vertexDescriptor = vertexDesc;

    m_impl->pipelineStateIndexed = [m_impl->device newRenderPipelineStateWithDescriptor:desc error:&error];
    if (error) {
        NSLog(@"SpriteManager: Failed to create indexed pipeline state: %@", error);
        return false;
    }

    NSLog(@"[SpriteManager] Indexed pipeline state created successfully: %p", m_impl->pipelineStateIndexed);

    return true;
}

// =============================================================================
// Standard Palette Texture Initialization
// =============================================================================

void SpriteManager::initializeStandardPaletteTextures() {
    if (!SuperTerminal::StandardPaletteLibrary::isInitialized()) {
        NSLog(@"[SpriteManager] StandardPaletteLibrary not initialized, cannot load palette textures");
        return;
    }

    NSLog(@"[SpriteManager] Loading 32 standard palette textures into GPU...");

    for (int paletteID = 0; paletteID < 32; paletteID++) {
        // Get palette from library
        uint8_t paletteData[SPRITE_PALETTE_SIZE];
        if (!SuperTerminal::StandardPaletteLibrary::copyPaletteRGBA(paletteID, paletteData)) {
            NSLog(@"[SpriteManager] WARNING: Failed to load standard palette %d", paletteID);
            continue;
        }

        // Create palette texture (16x1 RGBA8)
        MTLTextureDescriptor* paletteDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                                width:16
                                                                                               height:1
                                                                                            mipmapped:NO];
        paletteDesc.usage = MTLTextureUsageShaderRead;
        paletteDesc.storageMode = MTLStorageModeShared;

        id<MTLTexture> paletteTexture = [m_impl->device newTextureWithDescriptor:paletteDesc];
        if (!paletteTexture) {
            NSLog(@"[SpriteManager] ERROR: Failed to create palette texture for palette %d", paletteID);
            continue;
        }

        // Upload palette data
        MTLRegion region = MTLRegionMake2D(0, 0, 16, 1);
        [paletteTexture replaceRegion:region mipmapLevel:0 withBytes:paletteData bytesPerRow:16 * 4];

        // Store texture
        m_impl->standardPaletteTextures[paletteID] = paletteTexture;

        NSLog(@"[SpriteManager] Loaded standard palette %d into GPU", paletteID);
    }

    NSLog(@"[SpriteManager] Standard palette textures initialization complete");
}

// =============================================================================
// Indexed Sprite Texture Creation
// =============================================================================

MTLTexturePtr SpriteManager::createIndexTextureFromIndices(const uint8_t* indices, int width, int height) {
    MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Uint
                                                                                    width:width
                                                                                   height:height
                                                                                mipmapped:NO];
    desc.usage = MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModeShared;

    id<MTLTexture> texture = [m_impl->device newTextureWithDescriptor:desc];
    if (!texture) {
        NSLog(@"SpriteManager: Failed to create index texture");
        return nil;
    }

    MTLRegion region = MTLRegionMake2D(0, 0, width, height);
    [texture replaceRegion:region mipmapLevel:0 withBytes:indices bytesPerRow:width];

    return texture;
}

void SpriteManager::quantizeToIndexed(const uint8_t* pixels, int width, int height,
                                      uint8_t* outIndices, uint8_t outPalette[SPRITE_PALETTE_SIZE]) {
    // Simple quantization: extract up to 14 unique colors (indices 2-15)
    // Index 0 = transparent, Index 1 = opaque black

    // Initialize palette with convention
    memset(outPalette, 0, SPRITE_PALETTE_SIZE);
    outPalette[3] = 0;   // Index 0: transparent black
    outPalette[7] = 255; // Index 1: opaque black

    std::vector<uint32_t> uniqueColors;
    std::unordered_map<uint32_t, uint8_t> colorToIndex;

    // Reserve indices 0 and 1
    colorToIndex[0x00000000] = 0; // Transparent
    colorToIndex[0x000000FF] = 1; // Black

    uint8_t nextIndex = 2;

    // First pass: collect unique colors
    for (int i = 0; i < width * height; i++) {
        uint8_t r = pixels[i * 4 + 0];
        uint8_t g = pixels[i * 4 + 1];
        uint8_t b = pixels[i * 4 + 2];
        uint8_t a = pixels[i * 4 + 3];

        // If fully transparent, map to index 0
        if (a < 10) {
            outIndices[i] = 0;
            continue;
        }

        // If black, map to index 1
        if (r < 10 && g < 10 && b < 10) {
            outIndices[i] = 1;
            continue;
        }

        uint32_t colorKey = (r << 24) | (g << 16) | (b << 8) | a;

        if (colorToIndex.find(colorKey) == colorToIndex.end()) {
            if (nextIndex < 16) {
                colorToIndex[colorKey] = nextIndex;
                outPalette[nextIndex * 4 + 0] = r;
                outPalette[nextIndex * 4 + 1] = g;
                outPalette[nextIndex * 4 + 2] = b;
                outPalette[nextIndex * 4 + 3] = a;
                nextIndex++;
            }
        }
    }

    // Second pass: assign indices
    for (int i = 0; i < width * height; i++) {
        uint8_t r = pixels[i * 4 + 0];
        uint8_t g = pixels[i * 4 + 1];
        uint8_t b = pixels[i * 4 + 2];
        uint8_t a = pixels[i * 4 + 3];

        if (a < 10) {
            outIndices[i] = 0;
        } else if (r < 10 && g < 10 && b < 10) {
            outIndices[i] = 1;
        } else {
            uint32_t colorKey = (r << 24) | (g << 16) | (b << 8) | a;
            auto it = colorToIndex.find(colorKey);
            if (it != colorToIndex.end()) {
                outIndices[i] = it->second;
            } else {
                // Too many colors, use closest (or just use index 2 as fallback)
                outIndices[i] = 2;
            }
        }
    }
}

// =============================================================================
// Texture Loading
// =============================================================================

MTLTexturePtr SpriteManager::loadTextureFromFile(const std::string& filePath,
                                                  int* outWidth, int* outHeight) {
    NSLog(@"loadTextureFromFile: Starting, path=%s", filePath.c_str());
    @autoreleasepool {
        NSLog(@"loadTextureFromFile: Converting path to NSString...");
        NSString* path = [NSString stringWithUTF8String:filePath.c_str()];
        NSLog(@"loadTextureFromFile: Loading NSImage from: %@", path);
        NSImage* image = [[NSImage alloc] initWithContentsOfFile:path];

        if (!image) {
            NSLog(@"SpriteManager: Failed to load image: %@", path);
            return nil;
        }
        NSLog(@"loadTextureFromFile: NSImage loaded successfully");

        // Get bitmap representation
        NSLog(@"loadTextureFromFile: Getting bitmap representation...");
        NSBitmapImageRep* bitmap = nil;
        for (NSImageRep* rep in image.representations) {
            if ([rep isKindOfClass:[NSBitmapImageRep class]]) {
                bitmap = (NSBitmapImageRep*)rep;
                break;
            }
        }

        if (!bitmap) {
            NSLog(@"loadTextureFromFile: No bitmap rep found, converting from CGImage...");
            CGImageRef cgImage = [image CGImageForProposedRect:nil context:nil hints:nil];
            if (!cgImage) {
                NSLog(@"SpriteManager: Failed to get CGImage");
                return nil;
            }
            bitmap = [[NSBitmapImageRep alloc] initWithCGImage:cgImage];
        }
        NSLog(@"loadTextureFromFile: Got bitmap, size=%ldx%ld", bitmap.pixelsWide, bitmap.pixelsHigh);

        NSInteger width = bitmap.pixelsWide;
        NSInteger height = bitmap.pixelsHigh;

        if (outWidth) *outWidth = (int)width;
        if (outHeight) *outHeight = (int)height;

        // Convert to RGBA if needed
        NSBitmapImageRep* rgbaBitmap = bitmap;
        if (bitmap.bitsPerPixel != 32) {
            rgbaBitmap = [[NSBitmapImageRep alloc]
                initWithBitmapDataPlanes:NULL
                              pixelsWide:width
                              pixelsHigh:height
                           bitsPerSample:8
                         samplesPerPixel:4
                                hasAlpha:YES
                                isPlanar:NO
                          colorSpaceName:NSDeviceRGBColorSpace
                             bytesPerRow:width * 4
                            bitsPerPixel:32];

            [NSGraphicsContext saveGraphicsState];
            NSGraphicsContext* context = [NSGraphicsContext
                graphicsContextWithBitmapImageRep:rgbaBitmap];
            [NSGraphicsContext setCurrentContext:context];
            [image drawInRect:NSMakeRect(0, 0, width, height)];
            [NSGraphicsContext restoreGraphicsState];
        }

        const uint8_t* pixels = rgbaBitmap.bitmapData;
        NSLog(@"loadTextureFromFile: Creating Metal texture from pixels...");
        MTLTexturePtr texture = createTextureFromPixels(pixels, (int)width, (int)height);
        NSLog(@"loadTextureFromFile: Done, texture=%p", texture);
        return texture;
    }
}

MTLTexturePtr SpriteManager::createTextureFromPixels(const uint8_t* pixels,
                                                      int width, int height) {
    MTLTextureDescriptor* desc = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                     width:width
                                    height:height
                                 mipmapped:NO];
    desc.usage = MTLTextureUsageShaderRead;

    id<MTLTexture> texture = [m_impl->device newTextureWithDescriptor:desc];
    if (!texture) {
        return nil;
    }

    MTLRegion region = MTLRegionMake2D(0, 0, width, height);
    NSUInteger bytesPerRow = width * 4;
    [texture replaceRegion:region mipmapLevel:0 withBytes:pixels bytesPerRow:bytesPerRow];

    return texture;
}

// =============================================================================
// Sprite Loading
// =============================================================================

uint16_t SpriteManager::loadSprite(const std::string& filePath) {
    NSLog(@"SpriteManager::loadSprite: Starting, path=%s", filePath.c_str());

    NSLog(@"SpriteManager::loadSprite: Acquiring mutex...");
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    NSLog(@"SpriteManager::loadSprite: Mutex acquired");

    // Get next available ID
    NSLog(@"SpriteManager::loadSprite: Getting next ID...");
    uint16_t spriteId = getNextAvailableId();
    if (spriteId == INVALID_SPRITE_ID) {
        NSLog(@"SpriteManager: No available sprite IDs");
        return INVALID_SPRITE_ID;
    }
    NSLog(@"SpriteManager::loadSprite: Got sprite ID: %d", spriteId);

    // Load texture
    NSLog(@"SpriteManager::loadSprite: Loading texture from file...");
    int width, height;
    MTLTexturePtr texture = loadTextureFromFile(filePath, &width, &height);
    NSLog(@"SpriteManager::loadSprite: Texture loading returned, texture=%p", texture);
    if (!texture) {
        NSLog(@"SpriteManager::loadSprite: Failed to load texture");
        return INVALID_SPRITE_ID;
    }

    // Create sprite
    NSLog(@"SpriteManager::loadSprite: Creating sprite object...");
    auto sprite = std::make_unique<Sprite>();
    sprite->spriteId = spriteId;
    sprite->texture = texture;
    sprite->textureWidth = width;
    sprite->textureHeight = height;
    sprite->loaded = true;
    sprite->visible = false;

    m_impl->sprites[spriteId] = std::move(sprite);

    NSLog(@"SpriteManager::loadSprite: Success, returning ID %d", spriteId);
    return spriteId;
}

uint16_t SpriteManager::loadSpriteFromPixels(const uint8_t* pixels, int width, int height) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    uint16_t spriteId = getNextAvailableId();
    if (spriteId == INVALID_SPRITE_ID) {
        return INVALID_SPRITE_ID;
    }

    MTLTexturePtr texture = createTextureFromPixels(pixels, width, height);
    if (!texture) {
        return INVALID_SPRITE_ID;
    }

    auto sprite = std::make_unique<Sprite>();
    sprite->spriteId = spriteId;
    sprite->texture = texture;
    sprite->textureWidth = width;
    sprite->textureHeight = height;
    sprite->loaded = true;
    sprite->visible = false;

    m_impl->sprites[spriteId] = std::move(sprite);

    return spriteId;
}

bool SpriteManager::setSpriteTexture(uint16_t spriteId, const uint8_t* pixels, int width, int height) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    // Create texture from pixels
    MTLTexturePtr texture = createTextureFromPixels(pixels, width, height);
    if (!texture) {
        return false;
    }

    // Create new sprite entry or update existing one
    auto sprite = std::make_unique<Sprite>();
    sprite->spriteId = spriteId;
    sprite->texture = texture;
    sprite->textureWidth = width;
    sprite->textureHeight = height;
    sprite->loaded = true;
    sprite->visible = false;

    // Store sprite
    m_impl->sprites[spriteId] = std::move(sprite);

    return true;
}

uint16_t SpriteManager::loadSpriteIndexed(const uint8_t* indices, int width, int height,
                                          const uint8_t palette[SPRITE_PALETTE_SIZE]) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    // Get next available sprite ID
    uint16_t spriteId = getNextAvailableId();
    if (spriteId == INVALID_SPRITE_ID) {
        NSLog(@"SpriteManager: No available sprite IDs");
        return INVALID_SPRITE_ID;
    }

    // Create index texture (R8Uint)
    MTLTexturePtr indexTexture = createIndexTextureFromIndices(indices, width, height);
    if (!indexTexture) {
        return INVALID_SPRITE_ID;
    }

    // Create palette texture (16x1 RGBA8)
    MTLTextureDescriptor* paletteDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                            width:16
                                                                                           height:1
                                                                                        mipmapped:NO];
    paletteDesc.usage = MTLTextureUsageShaderRead;
    paletteDesc.storageMode = MTLStorageModeShared;

    id<MTLTexture> paletteTexture = [m_impl->device newTextureWithDescriptor:paletteDesc];
    if (!paletteTexture) {
        NSLog(@"SpriteManager: Failed to create palette texture");
        return INVALID_SPRITE_ID;
    }

    MTLRegion region = MTLRegionMake2D(0, 0, 16, 1);
    [paletteTexture replaceRegion:region mipmapLevel:0 withBytes:palette bytesPerRow:16 * 4];

    // Create indexed sprite
    auto sprite = std::make_unique<SpriteIndexed>();
    sprite->spriteId = spriteId;
    sprite->indexTexture = indexTexture;
    sprite->paletteTexture = paletteTexture;  // Store the palette texture!
    memcpy(sprite->palette, palette, SPRITE_PALETTE_SIZE);
    sprite->textureWidth = width;
    sprite->textureHeight = height;
    sprite->loaded = true;
    sprite->visible = false;

    // Debug: Log palette colors
    NSLog(@"[SpriteManager] Loaded indexed sprite %d (%dx%d) with palette:", spriteId, width, height);
    for (int i = 0; i < 16; i++) {
        int offset = i * 4;
        NSLog(@"  Color[%2d]: R=%3d G=%3d B=%3d A=%3d", i,
              palette[offset], palette[offset+1], palette[offset+2], palette[offset+3]);
    }

    m_impl->spritesIndexed[spriteId] = std::move(sprite);
    markDirty();

    return spriteId;
}

uint16_t SpriteManager::loadSpriteIndexedFromRGBA(const uint8_t* pixels, int width, int height,
                                                   uint8_t* outPalette) {
    // Quantize RGBA to indexed
    std::vector<uint8_t> indices(width * height);
    uint8_t palette[SPRITE_PALETTE_SIZE];

    quantizeToIndexed(pixels, width, height, indices.data(), palette);

    // Copy palette if requested
    if (outPalette) {
        memcpy(outPalette, palette, SPRITE_PALETTE_SIZE);
    }

    // Load as indexed sprite
    return loadSpriteIndexed(indices.data(), width, height, palette);
}

uint16_t SpriteManager::loadSpriteFromSPRTZ(const std::string& filePath) {
    NSLog(@"[SpriteManager::loadSpriteFromSPRTZ] Starting to load: %s", filePath.c_str());

    // Buffer for decompressed data
    uint8_t pixels[40 * 40];  // Max sprite size
    uint8_t palette[SPRITE_PALETTE_SIZE];
    int width, height;
    bool isStandardPalette;
    uint8_t standardPaletteID;

    NSLog(@"[SpriteManager::loadSpriteFromSPRTZ] Calling SpriteCompression::loadSPRTZv2...");

    // Load and decompress SPRTZ file (handles both v1 and v2 formats)
    if (!SPRED::SpriteCompression::loadSPRTZv2(filePath, width, height, pixels, palette,
                                                 isStandardPalette, standardPaletteID)) {
        NSLog(@"[SpriteManager::loadSpriteFromSPRTZ] ERROR: Failed to load SPRTZ file: %s", filePath.c_str());
        return 0;
    }

    NSLog(@"[SpriteManager::loadSpriteFromSPRTZ] Successfully loaded SPRTZ: %dx%d, standard=%d, paletteID=%d",
          width, height, isStandardPalette, standardPaletteID);

    // If using standard palette, load with reference to pre-created palette texture
    if (isStandardPalette && standardPaletteID < 32) {
        NSLog(@"[SpriteManager::loadSpriteFromSPRTZ] Using standard palette %d", standardPaletteID);
        uint16_t spriteId = loadSpriteIndexedWithStandardPalette(pixels, width, height, standardPaletteID);
        if (spriteId == INVALID_SPRITE_ID) {
            NSLog(@"[SpriteManager::loadSpriteFromSPRTZ] ERROR: loadSpriteIndexedWithStandardPalette failed");
        } else {
            NSLog(@"[SpriteManager::loadSpriteFromSPRTZ] SUCCESS: Sprite loaded with ID %d (standard palette %d)",
                  spriteId, standardPaletteID);
        }
        return spriteId;
    }

    // Custom palette - create new palette texture
    NSLog(@"[SpriteManager::loadSpriteFromSPRTZ] Using custom palette");
    uint16_t spriteId = loadSpriteIndexed(pixels, width, height, palette);

    if (spriteId == INVALID_SPRITE_ID) {
        NSLog(@"[SpriteManager::loadSpriteFromSPRTZ] ERROR: loadSpriteIndexed failed");
    } else {
        NSLog(@"[SpriteManager::loadSpriteFromSPRTZ] SUCCESS: Sprite loaded with ID %d", spriteId);
    }

    return spriteId;
}

uint16_t SpriteManager::loadSpriteIndexedWithStandardPalette(const uint8_t* indices, int width, int height,
                                                              uint8_t standardPaletteID) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    if (standardPaletteID >= 32) {
        NSLog(@"[SpriteManager] Invalid standard palette ID: %d", standardPaletteID);
        return INVALID_SPRITE_ID;
    }

    // Get next available sprite ID
    uint16_t spriteId = getNextAvailableId();
    if (spriteId == INVALID_SPRITE_ID) {
        NSLog(@"[SpriteManager] No available sprite IDs");
        return INVALID_SPRITE_ID;
    }

    // Debug: Log first 16 pixel indices
    NSLog(@"[SpriteManager] First 16 pixel indices:");
    for (int i = 0; i < 16 && i < width * height; i++) {
        NSLog(@"  Pixel %d: index=%d", i, indices[i]);
    }

    // Create index texture
    MTLTexturePtr indexTexture = createIndexTextureFromIndices(indices, width, height);
    if (!indexTexture) {
        return INVALID_SPRITE_ID;
    }

    // Use pre-created standard palette texture
    id<MTLTexture> paletteTexture = m_impl->standardPaletteTextures[standardPaletteID];
    if (!paletteTexture) {
        NSLog(@"[SpriteManager] ERROR: Standard palette texture %d not initialized!", standardPaletteID);
        return INVALID_SPRITE_ID;
    }

    // Get palette data for storage
    uint8_t paletteData[SPRITE_PALETTE_SIZE];
    if (!SuperTerminal::StandardPaletteLibrary::copyPaletteRGBA(standardPaletteID, paletteData)) {
        NSLog(@"[SpriteManager] ERROR: Failed to get palette data for standard palette %d", standardPaletteID);
        return INVALID_SPRITE_ID;
    }

    // Debug: Log first 4 palette colors
    NSLog(@"[SpriteManager] First 4 palette colors:");
    for (int i = 0; i < 4; i++) {
        int offset = i * 4;
        NSLog(@"  Color %d: R=%d G=%d B=%d A=%d", i,
              paletteData[offset], paletteData[offset+1],
              paletteData[offset+2], paletteData[offset+3]);
    }

    // Create indexed sprite
    auto sprite = std::make_unique<SpriteIndexed>();
    sprite->spriteId = spriteId;
    sprite->indexTexture = indexTexture;
    sprite->paletteTexture = paletteTexture;  // Use shared standard palette texture!
    memcpy(sprite->palette, paletteData, SPRITE_PALETTE_SIZE);
    sprite->textureWidth = width;
    sprite->textureHeight = height;
    sprite->loaded = true;
    sprite->visible = false;

    m_impl->spritesIndexed[spriteId] = std::move(sprite);
    markDirty();

    NSLog(@"[SpriteManager] Loaded indexed sprite %d (%dx%d) with standard palette %d",
          spriteId, width, height, standardPaletteID);

    return spriteId;
}

void SpriteManager::unloadSprite(uint16_t spriteId) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    // Check RGB sprites
    auto it = m_impl->sprites.find(spriteId);
    if (it != m_impl->sprites.end()) {
        // Remove from render order
        auto orderIt = std::find(m_impl->renderOrder.begin(),
                                 m_impl->renderOrder.end(),
                                 spriteId);
        if (orderIt != m_impl->renderOrder.end()) {
            m_impl->renderOrder.erase(orderIt);
        }

        // Remove sprite
        m_impl->sprites.erase(it);

        // Add ID back to free pool
        m_impl->freeIds.insert(spriteId);
        return;
    }

    // Check indexed sprites
    auto itIndexed = m_impl->spritesIndexed.find(spriteId);
    if (itIndexed != m_impl->spritesIndexed.end()) {
        // Remove from render order
        auto orderIt = std::find(m_impl->renderOrder.begin(),
                                 m_impl->renderOrder.end(),
                                 spriteId);
        if (orderIt != m_impl->renderOrder.end()) {
            m_impl->renderOrder.erase(orderIt);
        }

        // Remove sprite
        m_impl->spritesIndexed.erase(itIndexed);

        // Add ID back to free pool
        m_impl->freeIds.insert(spriteId);
    }
}

bool SpriteManager::isSpriteLoaded(uint16_t spriteId) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    NSLog(@"[SpriteManager] isSpriteLoaded: checking sprite %d", spriteId);
    NSLog(@"[SpriteManager] RGB sprites count: %zu", m_impl->sprites.size());
    NSLog(@"[SpriteManager] Indexed sprites count: %zu", m_impl->spritesIndexed.size());

    auto it = m_impl->sprites.find(spriteId);
    if (it != m_impl->sprites.end()) {
        NSLog(@"[SpriteManager] Found sprite %d in RGB map, loaded=%d", spriteId, it->second->loaded);
        return it->second->loaded;
    }

    auto itIndexed = m_impl->spritesIndexed.find(spriteId);
    if (itIndexed != m_impl->spritesIndexed.end()) {
        NSLog(@"[SpriteManager] Found sprite %d in Indexed map, loaded=%d", spriteId, itIndexed->second->loaded);
        return itIndexed->second->loaded;
    }

    NSLog(@"[SpriteManager] Sprite %d NOT FOUND in either map!", spriteId);
    return false;
}

bool SpriteManager::isSpriteIndexed(uint16_t spriteId) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    return m_impl->spritesIndexed.find(spriteId) != m_impl->spritesIndexed.end();
}

// =============================================================================
// Sprite Commands
// =============================================================================

void SpriteManager::showSprite(uint16_t spriteId, float x, float y) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    SpriteCommandData cmd;
    cmd.type = SpriteCommand::Show;
    cmd.spriteId = spriteId;
    cmd.x = x;
    cmd.y = y;
    m_impl->commandQueue.push_back(cmd);

    if (!m_batchUpdate && spriteId < MAX_SPRITES) {
        std::lock_guard<std::mutex> dirtyLock(m_dirtyMutex);
        m_dirtySprites[spriteId] = true;
        m_dirty = true;
    }
}

void SpriteManager::hideSprite(uint16_t spriteId) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    SpriteCommandData cmd;
    cmd.type = SpriteCommand::Hide;
    cmd.spriteId = spriteId;
    m_impl->commandQueue.push_back(cmd);

    if (!m_batchUpdate && spriteId < MAX_SPRITES) {
        std::lock_guard<std::mutex> dirtyLock(m_dirtyMutex);
        m_dirtySprites[spriteId] = true;
        m_dirty = true;
    }
}

void SpriteManager::moveSprite(uint16_t spriteId, float x, float y) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    SpriteCommandData cmd;
    cmd.type = SpriteCommand::Move;
    cmd.spriteId = spriteId;
    cmd.x = x;
    cmd.y = y;
    m_impl->commandQueue.push_back(cmd);

    if (!m_batchUpdate && spriteId < MAX_SPRITES) {
        std::lock_guard<std::mutex> dirtyLock(m_dirtyMutex);
        m_dirtySprites[spriteId] = true;
        m_dirty = true;
    }
}

bool SpriteManager::isSpriteVisible(uint16_t spriteId) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    auto it = m_impl->sprites.find(spriteId);
    return it != m_impl->sprites.end() && it->second->visible;
}

void SpriteManager::setScale(uint16_t spriteId, float scale) {
    setScale(spriteId, scale, scale);
}

void SpriteManager::setScale(uint16_t spriteId, float scaleX, float scaleY) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    SpriteCommandData cmd;
    cmd.type = SpriteCommand::Scale;
    cmd.spriteId = spriteId;
    cmd.x = scaleX;
    cmd.y = scaleY;
    m_impl->commandQueue.push_back(cmd);

    if (!m_batchUpdate && spriteId < MAX_SPRITES) {
        std::lock_guard<std::mutex> dirtyLock(m_dirtyMutex);
        m_dirtySprites[spriteId] = true;
        m_dirty = true;
    }
}

void SpriteManager::setRotation(uint16_t spriteId, float rotation) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    SpriteCommandData cmd;
    cmd.type = SpriteCommand::Rotate;
    cmd.spriteId = spriteId;
    cmd.value = rotation;
    m_impl->commandQueue.push_back(cmd);

    if (!m_batchUpdate && spriteId < MAX_SPRITES) {
        std::lock_guard<std::mutex> dirtyLock(m_dirtyMutex);
        m_dirtySprites[spriteId] = true;
        m_dirty = true;
    }
}

void SpriteManager::setRotationDegrees(uint16_t spriteId, float degrees) {
    setRotation(spriteId, degrees * M_PI / 180.0f);
}

void SpriteManager::setAlpha(uint16_t spriteId, float alpha) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    SpriteCommandData cmd;
    cmd.type = SpriteCommand::SetAlpha;
    cmd.spriteId = spriteId;
    cmd.value = alpha;
    m_impl->commandQueue.push_back(cmd);

    if (!m_batchUpdate && spriteId < MAX_SPRITES) {
        std::lock_guard<std::mutex> dirtyLock(m_dirtyMutex);
        m_dirtySprites[spriteId] = true;
        m_dirty = true;
    }
}

void SpriteManager::setTint(uint16_t spriteId, float r, float g, float b, float a) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    SpriteCommandData cmd;
    cmd.type = SpriteCommand::SetTint;
    cmd.spriteId = spriteId;
    cmd.r = r;
    cmd.g = g;
    cmd.b = b;
    cmd.a = a;
    m_impl->commandQueue.push_back(cmd);

    if (!m_batchUpdate && spriteId < MAX_SPRITES) {
        std::lock_guard<std::mutex> dirtyLock(m_dirtyMutex);
        m_dirtySprites[spriteId] = true;
        m_dirty = true;
    }
}

// =============================================================================
// Sprite Information
// =============================================================================

void SpriteManager::getSpriteSize(uint16_t spriteId, int* outWidth, int* outHeight) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    // Check RGB sprites
    auto it = m_impl->sprites.find(spriteId);
    if (it != m_impl->sprites.end() && it->second->loaded) {
        if (outWidth) *outWidth = it->second->textureWidth;
        if (outHeight) *outHeight = it->second->textureHeight;
        return;
    }

    // Check indexed sprites
    auto itIndexed = m_impl->spritesIndexed.find(spriteId);
    if (itIndexed != m_impl->spritesIndexed.end() && itIndexed->second->loaded) {
        if (outWidth) *outWidth = itIndexed->second->textureWidth;
        if (outHeight) *outHeight = itIndexed->second->textureHeight;
        return;
    }

    // Not found
    if (outWidth) *outWidth = 0;
    if (outHeight) *outHeight = 0;
}

void SpriteManager::getSpritePosition(uint16_t spriteId, float* outX, float* outY) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    // Check RGB sprites
    auto it = m_impl->sprites.find(spriteId);
    if (it != m_impl->sprites.end() && it->second->loaded) {
        if (outX) *outX = it->second->x;
        if (outY) *outY = it->second->y;
        return;
    }

    // Check indexed sprites
    auto itIndexed = m_impl->spritesIndexed.find(spriteId);
    if (itIndexed != m_impl->spritesIndexed.end() && itIndexed->second->loaded) {
        if (outX) *outX = itIndexed->second->x;
        if (outY) *outY = itIndexed->second->y;
        return;
    }

    // Not found
    if (outX) *outX = 0.0f;
    if (outY) *outY = 0.0f;
}

MTLTexturePtr SpriteManager::getSpriteTexture(uint16_t spriteId) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    auto it = m_impl->sprites.find(spriteId);
    if (it != m_impl->sprites.end() && it->second->loaded) {
        return it->second->texture;
    }
    return nil;
}

size_t SpriteManager::getLoadedSpriteCount() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->sprites.size();
}

// =============================================================================
// Z-Ordering
// =============================================================================

void SpriteManager::bringToFront(uint16_t spriteId) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    auto it = std::find(m_impl->renderOrder.begin(), m_impl->renderOrder.end(), spriteId);
    if (it != m_impl->renderOrder.end()) {
        m_impl->renderOrder.erase(it);
        m_impl->renderOrder.push_back(spriteId);
    }
}

void SpriteManager::sendToBack(uint16_t spriteId) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    auto it = std::find(m_impl->renderOrder.begin(), m_impl->renderOrder.end(), spriteId);
    if (it != m_impl->renderOrder.end()) {
        m_impl->renderOrder.erase(it);
        m_impl->renderOrder.insert(m_impl->renderOrder.begin(), spriteId);
    }
}

// =============================================================================
// Command Queue Processing
// =============================================================================

void SpriteManager::processCommandQueue() {
    // Move commands to local vector (minimize lock time)
    std::vector<SpriteCommandData> commands;
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        commands = std::move(m_impl->commandQueue);
        m_impl->commandQueue.clear();
    }

    // Process commands without lock
    for (const auto& cmd : commands) {
        // Check RGB sprites first
        auto it = m_impl->sprites.find(cmd.spriteId);
        if (it != m_impl->sprites.end()) {
            Sprite* sprite = it->second.get();

            switch (cmd.type) {
                case SpriteCommand::Show:
                    if (sprite->loaded) {
                        sprite->x = cmd.x;
                        sprite->y = cmd.y;
                        sprite->visible = true;

                        // Add to render order if not already there
                        auto orderIt = std::find(m_impl->renderOrder.begin(),
                                               m_impl->renderOrder.end(),
                                               cmd.spriteId);
                        if (orderIt == m_impl->renderOrder.end()) {
                            m_impl->renderOrder.push_back(cmd.spriteId);
                        }
                    }
                    break;

                case SpriteCommand::Hide:
                    sprite->visible = false;
                    {
                        auto orderIt = std::find(m_impl->renderOrder.begin(),
                                               m_impl->renderOrder.end(),
                                               cmd.spriteId);
                        if (orderIt != m_impl->renderOrder.end()) {
                            m_impl->renderOrder.erase(orderIt);
                        }
                    }
                    break;

                case SpriteCommand::Move:
                    sprite->x = cmd.x;
                    sprite->y = cmd.y;
                    break;

                case SpriteCommand::Scale:
                    sprite->scaleX = cmd.x;
                    sprite->scaleY = cmd.y;
                    break;

                case SpriteCommand::Rotate:
                    sprite->rotation = cmd.value;
                    break;

                case SpriteCommand::SetAlpha:
                    sprite->alpha = cmd.value;
                    break;

                case SpriteCommand::SetTint:
                    sprite->tintR = cmd.r;
                    sprite->tintG = cmd.g;
                    sprite->tintB = cmd.b;
                    sprite->tintA = cmd.a;
                    break;
            }
            continue;
        }

        // Check indexed sprites
        auto itIndexed = m_impl->spritesIndexed.find(cmd.spriteId);
        if (itIndexed != m_impl->spritesIndexed.end()) {
            SpriteIndexed* sprite = itIndexed->second.get();

            switch (cmd.type) {
                case SpriteCommand::Show:
                    if (sprite->loaded) {
                        sprite->x = cmd.x;
                        sprite->y = cmd.y;
                        sprite->visible = true;

                        // Add to render order if not already there
                        auto orderIt = std::find(m_impl->renderOrder.begin(),
                                               m_impl->renderOrder.end(),
                                               cmd.spriteId);
                        if (orderIt == m_impl->renderOrder.end()) {
                            m_impl->renderOrder.push_back(cmd.spriteId);
                        }
                    }
                    break;

                case SpriteCommand::Hide:
                    sprite->visible = false;
                    {
                        auto orderIt = std::find(m_impl->renderOrder.begin(),
                                               m_impl->renderOrder.end(),
                                               cmd.spriteId);
                        if (orderIt != m_impl->renderOrder.end()) {
                            m_impl->renderOrder.erase(orderIt);
                        }
                    }
                    break;

                case SpriteCommand::Move:
                    sprite->x = cmd.x;
                    sprite->y = cmd.y;
                    break;

                case SpriteCommand::Scale:
                    sprite->scaleX = cmd.x;
                    sprite->scaleY = cmd.y;
                    break;

                case SpriteCommand::Rotate:
                    sprite->rotation = cmd.value;
                    break;

                case SpriteCommand::SetAlpha:
                    sprite->alpha = cmd.value;
                    break;

                case SpriteCommand::SetTint:
                    // Indexed sprites don't support tinting (they use palettes)
                    // This command is ignored for indexed sprites
                    break;
            }
        }
    }
}

// =============================================================================
// Rendering
// =============================================================================

void SpriteManager::render(MTLRenderCommandEncoderPtr encoderPtr,
                           int viewportWidth, int viewportHeight) {
    id<MTLRenderCommandEncoder> encoder = encoderPtr;

    // Process pending commands
    processCommandQueue();

    if (m_impl->renderOrder.empty()) {
        return;  // Nothing to render
    }

    // Create view-projection matrix (fixed logical 1920x1080 coordinate space)
    // This ensures sprites always work in the same coordinate space regardless of
    // physical pixel density (retina/HiDPI displays)
    float width = 1920.0f;
    float height = 1080.0f;

    simd_float4x4 viewProjectionMatrix = simd_matrix(
        simd_make_float4(2.0f / width, 0, 0, 0),
        simd_make_float4(0, -2.0f / height, 0, 0),
        simd_make_float4(0, 0, 1, 0),
        simd_make_float4(-1, 1, 0, 1)
    );

    // Set vertex buffer (shared by both pipelines)
    [encoder setVertexBuffer:m_impl->vertexBuffer offset:0 atIndex:0];

    // Render each sprite
    size_t spriteIndex = 0;
    for (uint16_t spriteId : m_impl->renderOrder) {
        // Check RGB sprites first
        auto it = m_impl->sprites.find(spriteId);
        if (it != m_impl->sprites.end()) {
            const Sprite* sprite = it->second.get();
            if (!sprite->visible || !sprite->loaded) continue;

            // Set RGB pipeline state
            [encoder setRenderPipelineState:m_impl->pipelineState];
            [encoder setFragmentSamplerState:m_impl->samplerState atIndex:0];

            // Calculate transform matrix (TRS order)
            // Scale sprites appropriately for 1920x1080 logical space
            float textureScaleX = (float)sprite->textureWidth / 128.0f * 2.4f;
            float textureScaleY = (float)sprite->textureHeight / 128.0f * 2.4f;
            float finalScaleX = sprite->scaleX * textureScaleX;
            float finalScaleY = sprite->scaleY * textureScaleY;

            simd_float4x4 scaleMatrix = simd_matrix(
                simd_make_float4(finalScaleX, 0, 0, 0),
                simd_make_float4(0, finalScaleY, 0, 0),
                simd_make_float4(0, 0, 1, 0),
                simd_make_float4(0, 0, 0, 1)
            );

            float c = cosf(sprite->rotation);
            float s = sinf(sprite->rotation);
            simd_float4x4 rotationMatrix = simd_matrix(
                simd_make_float4(c, s, 0, 0),
                simd_make_float4(-s, c, 0, 0),
                simd_make_float4(0, 0, 1, 0),
                simd_make_float4(0, 0, 0, 1)
            );

            simd_float4x4 translationMatrix = simd_matrix(
                simd_make_float4(1, 0, 0, 0),
                simd_make_float4(0, 1, 0, 0),
                simd_make_float4(0, 0, 1, 0),
                simd_make_float4(sprite->x, sprite->y, 0, 1)
            );

            simd_float4x4 modelMatrix = simd_mul(translationMatrix, simd_mul(rotationMatrix, scaleMatrix));

            NSUInteger offset = spriteIndex * sizeof(SpriteUniforms);
            SpriteUniforms* uniforms = (SpriteUniforms*)((uint8_t*)m_impl->uniformsBuffer.contents + offset);
            uniforms->modelMatrix = modelMatrix;
            uniforms->viewProjectionMatrix = viewProjectionMatrix;
            uniforms->color = simd_make_float4(sprite->tintR, sprite->tintG, sprite->tintB, sprite->alpha * sprite->tintA);

            [encoder setVertexBuffer:m_impl->uniformsBuffer offset:offset atIndex:1];
            [encoder setFragmentTexture:sprite->texture atIndex:0];
            [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];

            spriteIndex++;
            continue;
        }

        // Check indexed sprites
        auto itIndexed = m_impl->spritesIndexed.find(spriteId);
        if (itIndexed != m_impl->spritesIndexed.end()) {
            const SpriteIndexed* sprite = itIndexed->second.get();
            if (!sprite->visible || !sprite->loaded) continue;

            // Set indexed sprite pipeline state
            if (!m_impl->pipelineStateIndexed) {
                NSLog(@"[SpriteManager] ERROR: Indexed pipeline state is NULL!");
                continue;
            }
            [encoder setRenderPipelineState:m_impl->pipelineStateIndexed];
            [encoder setFragmentSamplerState:m_impl->samplerStateIndexed atIndex:0];

            // Calculate transform matrix (TRS order)
            // Scale sprites appropriately for 1920x1080 logical space
            float textureScaleX = (float)sprite->textureWidth / 128.0f * 2.4f;
            float textureScaleY = (float)sprite->textureHeight / 128.0f * 2.4f;
            float finalScaleX = sprite->scaleX * textureScaleX;
            float finalScaleY = sprite->scaleY * textureScaleY;

            simd_float4x4 scaleMatrix = simd_matrix(
                simd_make_float4(finalScaleX, 0, 0, 0),
                simd_make_float4(0, finalScaleY, 0, 0),
                simd_make_float4(0, 0, 1, 0),
                simd_make_float4(0, 0, 0, 1)
            );

            float c = cosf(sprite->rotation);
            float s = sinf(sprite->rotation);
            simd_float4x4 rotationMatrix = simd_matrix(
                simd_make_float4(c, s, 0, 0),
                simd_make_float4(-s, c, 0, 0),
                simd_make_float4(0, 0, 1, 0),
                simd_make_float4(0, 0, 0, 1)
            );

            simd_float4x4 translationMatrix = simd_matrix(
                simd_make_float4(1, 0, 0, 0),
                simd_make_float4(0, 1, 0, 0),
                simd_make_float4(0, 0, 1, 0),
                simd_make_float4(sprite->x, sprite->y, 0, 1)
            );

            simd_float4x4 modelMatrix = simd_mul(translationMatrix, simd_mul(rotationMatrix, scaleMatrix));

            NSUInteger offset = spriteIndex * sizeof(SpriteUniforms);
            SpriteUniforms* uniforms = (SpriteUniforms*)((uint8_t*)m_impl->uniformsBuffer.contents + offset);
            uniforms->modelMatrix = modelMatrix;
            uniforms->viewProjectionMatrix = viewProjectionMatrix;
            uniforms->color = simd_make_float4(1.0f, 1.0f, 1.0f, sprite->alpha);

            [encoder setVertexBuffer:m_impl->uniformsBuffer offset:offset atIndex:1];

            [encoder setFragmentTexture:sprite->indexTexture atIndex:0];
            [encoder setFragmentTexture:sprite->paletteTexture atIndex:1];

            [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];

            spriteIndex++;
        }
    }
}

// =============================================================================
// Indexed Sprite Palette Operations
// =============================================================================

bool SpriteManager::setSpritePalette(uint16_t spriteId, const uint8_t palette[SPRITE_PALETTE_SIZE]) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    auto it = m_impl->spritesIndexed.find(spriteId);
    if (it == m_impl->spritesIndexed.end()) {
        return false;
    }

    // Create new custom palette texture (16x1 RGBA8)
    MTLTextureDescriptor* paletteDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                            width:16
                                                                                           height:1
                                                                                        mipmapped:NO];
    paletteDesc.usage = MTLTextureUsageShaderRead;
    paletteDesc.storageMode = MTLStorageModeShared;

    id<MTLTexture> paletteTexture = [m_impl->device newTextureWithDescriptor:paletteDesc];
    if (!paletteTexture) {
        NSLog(@"[SpriteManager] Failed to create custom palette texture");
        return false;
    }

    // Upload palette data to GPU
    MTLRegion region = MTLRegionMake2D(0, 0, 16, 1);
    [paletteTexture replaceRegion:region mipmapLevel:0 withBytes:palette bytesPerRow:16 * 4];

    // Update sprite (old palette texture will be released by ARC)
    it->second->paletteTexture = paletteTexture;
    memcpy(it->second->palette, palette, SPRITE_PALETTE_SIZE);

    markDirty();
    return true;
}

bool SpriteManager::setSpriteStandardPalette(uint16_t spriteId, uint8_t standardPaletteID) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    // Validate standard palette ID
    if (standardPaletteID >= 32) {
        NSLog(@"[SpriteManager] Invalid standard palette ID: %d", standardPaletteID);
        return false;
    }

    auto it = m_impl->spritesIndexed.find(spriteId);
    if (it == m_impl->spritesIndexed.end()) {
        return false;
    }

    // Get standard palette texture
    id<MTLTexture> paletteTexture = m_impl->standardPaletteTextures[standardPaletteID];
    if (!paletteTexture) {
        NSLog(@"[SpriteManager] Standard palette texture %d not initialized", standardPaletteID);
        return false;
    }

    // Get palette data for storage
    uint8_t paletteData[SPRITE_PALETTE_SIZE];
    if (!SuperTerminal::StandardPaletteLibrary::copyPaletteRGBA(standardPaletteID, paletteData)) {
        NSLog(@"[SpriteManager] Failed to get palette data for standard palette %d", standardPaletteID);
        return false;
    }

    // Update sprite to use standard palette (old palette texture will be released by ARC)
    it->second->paletteTexture = paletteTexture;
    memcpy(it->second->palette, paletteData, SPRITE_PALETTE_SIZE);

    markDirty();
    return true;
}

bool SpriteManager::getSpritePalette(uint16_t spriteId, uint8_t outPalette[SPRITE_PALETTE_SIZE]) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    auto it = m_impl->spritesIndexed.find(spriteId);
    if (it == m_impl->spritesIndexed.end()) {
        return false;
    }

    memcpy(outPalette, it->second->palette, SPRITE_PALETTE_SIZE);
    return true;
}

bool SpriteManager::setSpritePaletteColor(uint16_t spriteId, uint8_t colorIndex,
                                          uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (colorIndex >= SPRITE_PALETTE_COLORS) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_impl->mutex);

    auto it = m_impl->spritesIndexed.find(spriteId);
    if (it == m_impl->spritesIndexed.end()) {
        return false;
    }

    // Check if sprite is using a shared standard palette texture
    // by comparing against all standard palette textures
    bool isUsingStandardPalette = false;
    for (int i = 0; i < 32; i++) {
        if (it->second->paletteTexture == m_impl->standardPaletteTextures[i]) {
            isUsingStandardPalette = true;
            NSLog(@"[SpriteManager] Sprite %d is using shared standard palette %d, creating custom copy", spriteId, i);
            break;
        }
    }

    // If using standard palette, create a custom palette copy first
    if (isUsingStandardPalette) {
        MTLTextureDescriptor* paletteDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                                width:16
                                                                                               height:1
                                                                                            mipmapped:NO];
        paletteDesc.usage = MTLTextureUsageShaderRead;
        paletteDesc.storageMode = MTLStorageModeShared;

        id<MTLTexture> newPaletteTexture = [m_impl->device newTextureWithDescriptor:paletteDesc];
        if (!newPaletteTexture) {
            NSLog(@"[SpriteManager] Failed to create custom palette texture");
            return false;
        }

        // Copy current palette data to new texture
        MTLRegion region = MTLRegionMake2D(0, 0, 16, 1);
        [newPaletteTexture replaceRegion:region mipmapLevel:0 withBytes:it->second->palette bytesPerRow:16 * 4];

        // Replace the shared texture with our custom one
        it->second->paletteTexture = newPaletteTexture;
        NSLog(@"[SpriteManager] Created custom palette texture for sprite %d", spriteId);
    }

    uint8_t* palette = it->second->palette;
    palette[colorIndex * 4 + 0] = r;
    palette[colorIndex * 4 + 1] = g;
    palette[colorIndex * 4 + 2] = b;
    palette[colorIndex * 4 + 3] = a;

    // Upload palette changes to GPU texture
    MTLRegion region = MTLRegionMake2D(0, 0, 16, 1);
    [it->second->paletteTexture replaceRegion:region mipmapLevel:0 withBytes:palette bytesPerRow:16 * 4];

    markDirty();
    return true;
}

bool SpriteManager::lerpSpritePalette(uint16_t spriteId,
                                      const uint8_t paletteA[SPRITE_PALETTE_SIZE],
                                      const uint8_t paletteB[SPRITE_PALETTE_SIZE],
                                      float t) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    auto it = m_impl->spritesIndexed.find(spriteId);
    if (it == m_impl->spritesIndexed.end()) {
        return false;
    }

    // Check if sprite is using a shared standard palette texture
    bool isUsingStandardPalette = false;
    for (int i = 0; i < 32; i++) {
        if (it->second->paletteTexture == m_impl->standardPaletteTextures[i]) {
            isUsingStandardPalette = true;
            NSLog(@"[SpriteManager] Sprite %d is using shared standard palette %d, creating custom copy", spriteId, i);
            break;
        }
    }

    // If using standard palette, create a custom palette copy first
    if (isUsingStandardPalette) {
        MTLTextureDescriptor* paletteDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                                width:16
                                                                                               height:1
                                                                                            mipmapped:NO];
        paletteDesc.usage = MTLTextureUsageShaderRead;
        paletteDesc.storageMode = MTLStorageModeShared;

        id<MTLTexture> newPaletteTexture = [m_impl->device newTextureWithDescriptor:paletteDesc];
        if (!newPaletteTexture) {
            NSLog(@"[SpriteManager] Failed to create custom palette texture");
            return false;
        }

        // Copy current palette data to new texture
        MTLRegion region = MTLRegionMake2D(0, 0, 16, 1);
        [newPaletteTexture replaceRegion:region mipmapLevel:0 withBytes:it->second->palette bytesPerRow:16 * 4];

        // Replace the shared texture with our custom one
        it->second->paletteTexture = newPaletteTexture;
        NSLog(@"[SpriteManager] Created custom palette texture for sprite %d", spriteId);
    }

    t = std::max(0.0f, std::min(1.0f, t));

    uint8_t* palette = it->second->palette;
    for (size_t i = 0; i < SPRITE_PALETTE_SIZE; i++) {
        float a = paletteA[i];
        float b = paletteB[i];
        palette[i] = static_cast<uint8_t>(a + (b - a) * t);
    }

    // Upload palette changes to GPU texture
    MTLRegion region = MTLRegionMake2D(0, 0, 16, 1);
    [it->second->paletteTexture replaceRegion:region mipmapLevel:0 withBytes:palette bytesPerRow:16 * 4];

    markDirty();
    return true;
}

bool SpriteManager::copySpritePalette(uint16_t srcSpriteId, uint16_t dstSpriteId) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    auto itSrc = m_impl->spritesIndexed.find(srcSpriteId);
    auto itDst = m_impl->spritesIndexed.find(dstSpriteId);

    if (itSrc == m_impl->spritesIndexed.end() || itDst == m_impl->spritesIndexed.end()) {
        return false;
    }

    // Check if destination sprite is using a shared standard palette texture
    bool isUsingStandardPalette = false;
    for (int i = 0; i < 32; i++) {
        if (itDst->second->paletteTexture == m_impl->standardPaletteTextures[i]) {
            isUsingStandardPalette = true;
            NSLog(@"[SpriteManager] Destination sprite %d is using shared standard palette %d, creating custom copy", dstSpriteId, i);
            break;
        }
    }

    // If using standard palette, create a custom palette copy first
    if (isUsingStandardPalette) {
        MTLTextureDescriptor* paletteDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                                width:16
                                                                                               height:1
                                                                                            mipmapped:NO];
        paletteDesc.usage = MTLTextureUsageShaderRead;
        paletteDesc.storageMode = MTLStorageModeShared;

        id<MTLTexture> newPaletteTexture = [m_impl->device newTextureWithDescriptor:paletteDesc];
        if (!newPaletteTexture) {
            NSLog(@"[SpriteManager] Failed to create custom palette texture");
            return false;
        }

        // Copy current palette data to new texture
        MTLRegion region = MTLRegionMake2D(0, 0, 16, 1);
        [newPaletteTexture replaceRegion:region mipmapLevel:0 withBytes:itDst->second->palette bytesPerRow:16 * 4];

        // Replace the shared texture with our custom one
        itDst->second->paletteTexture = newPaletteTexture;
        NSLog(@"[SpriteManager] Created custom palette texture for destination sprite %d", dstSpriteId);
    }

    memcpy(itDst->second->palette, itSrc->second->palette, SPRITE_PALETTE_SIZE);

    // Upload palette changes to GPU texture
    MTLRegion region = MTLRegionMake2D(0, 0, 16, 1);
    [itDst->second->paletteTexture replaceRegion:region mipmapLevel:0 withBytes:itDst->second->palette bytesPerRow:16 * 4];

    markDirty();
    return true;
}

bool SpriteManager::adjustSpritePaletteBrightness(uint16_t spriteId, float brightness) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    auto it = m_impl->spritesIndexed.find(spriteId);
    if (it == m_impl->spritesIndexed.end()) {
        return false;
    }

    // Check if sprite is using a shared standard palette texture
    bool isUsingStandardPalette = false;
    for (int i = 0; i < 32; i++) {
        if (it->second->paletteTexture == m_impl->standardPaletteTextures[i]) {
            isUsingStandardPalette = true;
            NSLog(@"[SpriteManager] Sprite %d is using shared standard palette %d, creating custom copy", spriteId, i);
            break;
        }
    }

    // If using standard palette, create a custom palette copy first
    if (isUsingStandardPalette) {
        MTLTextureDescriptor* paletteDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                                width:16
                                                                                               height:1
                                                                                            mipmapped:NO];
        paletteDesc.usage = MTLTextureUsageShaderRead;
        paletteDesc.storageMode = MTLStorageModeShared;

        id<MTLTexture> newPaletteTexture = [m_impl->device newTextureWithDescriptor:paletteDesc];
        if (!newPaletteTexture) {
            NSLog(@"[SpriteManager] Failed to create custom palette texture");
            return false;
        }

        // Copy current palette data to new texture
        MTLRegion region = MTLRegionMake2D(0, 0, 16, 1);
        [newPaletteTexture replaceRegion:region mipmapLevel:0 withBytes:it->second->palette bytesPerRow:16 * 4];

        // Replace the shared texture with our custom one
        it->second->paletteTexture = newPaletteTexture;
        NSLog(@"[SpriteManager] Created custom palette texture for sprite %d", spriteId);
    }

    uint8_t* palette = it->second->palette;
    for (size_t i = 0; i < SPRITE_PALETTE_COLORS; i++) {
        int r = static_cast<int>(palette[i * 4 + 0] * brightness);
        int g = static_cast<int>(palette[i * 4 + 1] * brightness);
        int b = static_cast<int>(palette[i * 4 + 2] * brightness);

        palette[i * 4 + 0] = std::min(255, std::max(0, r));
        palette[i * 4 + 1] = std::min(255, std::max(0, g));
        palette[i * 4 + 2] = std::min(255, std::max(0, b));
        // Alpha unchanged
    }

    // Upload palette changes to GPU texture
    MTLRegion region = MTLRegionMake2D(0, 0, 16, 1);
    [it->second->paletteTexture replaceRegion:region mipmapLevel:0 withBytes:palette bytesPerRow:16 * 4];

    markDirty();
    return true;
}

bool SpriteManager::rotateSpritePalette(uint16_t spriteId, int startIndex, int endIndex, int amount) {
    if (startIndex < 0 || endIndex >= SPRITE_PALETTE_COLORS || startIndex >= endIndex) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_impl->mutex);

    auto it = m_impl->spritesIndexed.find(spriteId);
    if (it == m_impl->spritesIndexed.end()) {
        return false;
    }

    // Check if sprite is using a shared standard palette texture
    bool isUsingStandardPalette = false;
    for (int i = 0; i < 32; i++) {
        if (it->second->paletteTexture == m_impl->standardPaletteTextures[i]) {
            isUsingStandardPalette = true;
            NSLog(@"[SpriteManager] Sprite %d is using shared standard palette %d, creating custom copy", spriteId, i);
            break;
        }
    }

    // If using standard palette, create a custom palette copy first
    if (isUsingStandardPalette) {
        MTLTextureDescriptor* paletteDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                                width:16
                                                                                               height:1
                                                                                            mipmapped:NO];
        paletteDesc.usage = MTLTextureUsageShaderRead;
        paletteDesc.storageMode = MTLStorageModeShared;

        id<MTLTexture> newPaletteTexture = [m_impl->device newTextureWithDescriptor:paletteDesc];
        if (!newPaletteTexture) {
            NSLog(@"[SpriteManager] Failed to create custom palette texture");
            return false;
        }

        // Copy current palette data to new texture
        MTLRegion region = MTLRegionMake2D(0, 0, 16, 1);
        [newPaletteTexture replaceRegion:region mipmapLevel:0 withBytes:it->second->palette bytesPerRow:16 * 4];

        // Replace the shared texture with our custom one
        it->second->paletteTexture = newPaletteTexture;
        NSLog(@"[SpriteManager] Created custom palette texture for sprite %d", spriteId);
    }

    uint8_t* palette = it->second->palette;
    int rangeSize = endIndex - startIndex + 1;
    amount = ((amount % rangeSize) + rangeSize) % rangeSize;

    if (amount == 0) {
        return true;
    }

    // Copy the range
    uint8_t temp[SPRITE_PALETTE_SIZE];
    for (int i = 0; i < rangeSize; i++) {
        int srcIdx = startIndex + i;
        memcpy(&temp[i * 4], &palette[srcIdx * 4], 4);
    }

    // Rotate and write back
    for (int i = 0; i < rangeSize; i++) {
        int srcIdx = (i + amount) % rangeSize;
        int dstIdx = startIndex + i;
        memcpy(&palette[dstIdx * 4], &temp[srcIdx * 4], 4);
    }

    // Upload palette changes to GPU texture
    MTLRegion region = MTLRegionMake2D(0, 0, 16, 1);
    [it->second->paletteTexture replaceRegion:region mipmapLevel:0 withBytes:palette bytesPerRow:16 * 4];

    markDirty();
    return true;
}

// =============================================================================
// Utility
// =============================================================================

void SpriteManager::clearAll() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    m_impl->sprites.clear();
    m_impl->spritesIndexed.clear();
    m_impl->renderOrder.clear();
    m_impl->commandQueue.clear();
    m_impl->freeIds.clear();
    m_impl->nextId = 1;
}

void SpriteManager::getMemoryStats(size_t* outRGBBytes, size_t* outIndexedBytes,
                                   size_t* outRGBCount, size_t* outIndexedCount) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);

    size_t rgbBytes = 0;
    size_t indexedBytes = 0;

    // Calculate RGB sprite memory
    for (const auto& pair : m_impl->sprites) {
        const Sprite* sprite = pair.second.get();
        if (sprite && sprite->loaded) {
            // RGBA: 4 bytes per pixel
            rgbBytes += sprite->textureWidth * sprite->textureHeight * 4;
        }
    }

    // Calculate indexed sprite memory
    for (const auto& pair : m_impl->spritesIndexed) {
        const SpriteIndexed* sprite = pair.second.get();
        if (sprite && sprite->loaded) {
            // Index: 1 byte per pixel + 64 bytes for palette
            indexedBytes += sprite->textureWidth * sprite->textureHeight + SPRITE_PALETTE_SIZE;
        }
    }

    if (outRGBBytes) *outRGBBytes = rgbBytes;
    if (outIndexedBytes) *outIndexedBytes = indexedBytes;
    if (outRGBCount) *outRGBCount = m_impl->sprites.size();
    if (outIndexedCount) *outIndexedCount = m_impl->spritesIndexed.size();
}

uint16_t SpriteManager::getNextAvailableId() const {
    // Check free pool first
    if (!m_impl->freeIds.empty()) {
        uint16_t id = *m_impl->freeIds.begin();
        m_impl->freeIds.erase(m_impl->freeIds.begin());
        return id;
    }

    // Otherwise increment counter
    if (m_impl->nextId < MAX_SPRITES) {
        return m_impl->nextId++;
    }

    return INVALID_SPRITE_ID;
}

// =============================================================================
// Dirty Tracking
// =============================================================================

bool SpriteManager::isDirty() const {
    std::lock_guard<std::mutex> lock(m_dirtyMutex);
    return m_dirty;
}

void SpriteManager::markDirty() {
    std::lock_guard<std::mutex> lock(m_dirtyMutex);
    m_dirty = true;
}

void SpriteManager::clearDirty() {
    std::lock_guard<std::mutex> lock(m_dirtyMutex);
    m_dirty = false;
    std::fill(m_dirtySprites.begin(), m_dirtySprites.end(), false);
}

bool SpriteManager::isSpriteDirty(uint16_t spriteId) const {
    if (spriteId >= MAX_SPRITES) return false;
    std::lock_guard<std::mutex> lock(m_dirtyMutex);
    return m_dirtySprites[spriteId];
}

void SpriteManager::beginBatchUpdate() {
    std::lock_guard<std::mutex> lock(m_dirtyMutex);
    m_batchUpdate = true;
}

void SpriteManager::endBatchUpdate() {
    std::lock_guard<std::mutex> lock(m_dirtyMutex);
    m_batchUpdate = false;
    m_dirty = true;
}

} // namespace SuperTerminal
