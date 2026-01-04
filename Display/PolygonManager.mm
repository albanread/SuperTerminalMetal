//
// PolygonManager.mm
// SuperTerminal v2 - GPU-Accelerated Regular Polygon Rendering
//
// Implementation of polygon rendering using Metal instanced rendering.
// Generates polygon vertices procedurally in vertex shader as triangle fan.
//

#import "PolygonManager.h"
#import <Foundation/Foundation.h>
#import <simd/simd.h>
#include <cmath>

using namespace SuperTerminal;

// Uniform data for shader
struct PolygonUniforms {
    float screenWidth;
    float screenHeight;
    simd::float2 padding;
};

@interface PolygonManagerImpl : NSObject
@property (nonatomic, strong) id<MTLDevice> device;
@property (nonatomic, strong) id<MTLRenderPipelineState> pipelineState;
@property (nonatomic, strong) id<MTLBuffer> instanceBuffer;
@property (nonatomic, strong) id<MTLBuffer> uniformBuffer;
@end

@implementation PolygonManagerImpl
@end

PolygonManager::PolygonManager()
    : m_device(nil)
    , m_pipelineState(nil)
    , m_instanceBuffer(nil)
    , m_uniformBuffer(nil)
    , m_nextId(1)
    , m_maxPolygons(10000)
    , m_bufferNeedsUpdate(false)
    , m_screenWidth(1920)
    , m_screenHeight(1080)
{
}

PolygonManager::~PolygonManager() {
    m_device = nil;
    m_pipelineState = nil;
    m_instanceBuffer = nil;
    m_uniformBuffer = nil;
}

bool PolygonManager::initialize(id<MTLDevice> device, int screenWidth, int screenHeight) {
    m_device = device;
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;

    // Create uniform buffer
    m_uniformBuffer = [m_device newBufferWithLength:sizeof(PolygonUniforms)
                                            options:MTLResourceStorageModeShared];
    if (!m_uniformBuffer) {
        NSLog(@"[PolygonManager] ERROR: Failed to create uniform buffer");
        return false;
    }

    PolygonUniforms* uniforms = (PolygonUniforms*)[m_uniformBuffer contents];
    uniforms->screenWidth = screenWidth;
    uniforms->screenHeight = screenHeight;

    // Create instance buffer
    m_instanceBuffer = [m_device newBufferWithLength:sizeof(PolygonInstance) * m_maxPolygons
                                             options:MTLResourceStorageModeShared];
    if (!m_instanceBuffer) {
        NSLog(@"[PolygonManager] ERROR: Failed to create instance buffer");
        return false;
    }

    // Compile shaders from inline source
    NSString* shaderSource = @R"(
        #include <metal_stdlib>
        using namespace metal;

        // Polygon instance data (matches C++ PolygonInstance)
        struct PolygonInstance {
            float x;
            float y;
            float radius;
            uint numSides;
            uint color1;
            uint color2;
            uint color3;
            uint color4;
            uint mode;
            float param1;
            float param2;
            float param3;
            float rotation;
        };

        // Uniform data
        struct PolygonUniforms {
            float screenWidth;
            float screenHeight;
            float2 padding;
        };

        // Vertex output / Fragment input
        struct VertexOut {
            float4 position [[position]];
            float4 color;
            float2 texCoord;
            float2 pixelCoord;
            uint mode;
            float4 color1;
            float4 color2;
            float4 color3;
            float4 color4;
            float param1;
            float param2;
            float param3;
            float polygonRadius;
        };

        // Gradient modes
        constant uint GRADIENT_SOLID = 0;
        constant uint GRADIENT_HORIZONTAL = 1;
        constant uint GRADIENT_VERTICAL = 2;
        constant uint GRADIENT_DIAGONAL_TL_BR = 3;
        constant uint GRADIENT_DIAGONAL_TR_BL = 4;
        constant uint GRADIENT_RADIAL = 5;
        constant uint GRADIENT_FOUR_CORNER = 6;
        constant uint GRADIENT_THREE_POINT = 7;

        // Procedural pattern modes
        constant uint PATTERN_OUTLINE = 100;
        constant uint PATTERN_DASHED_OUTLINE = 101;
        constant uint PATTERN_HORIZONTAL_STRIPES = 102;
        constant uint PATTERN_VERTICAL_STRIPES = 103;
        constant uint PATTERN_DIAGONAL_STRIPES = 104;
        constant uint PATTERN_CHECKERBOARD = 105;
        constant uint PATTERN_DOTS = 106;
        constant uint PATTERN_CROSSHATCH = 107;
        constant uint PATTERN_GRID = 109;

        // Unpack RGBA8888 to float4
        float4 unpackColor(uint packedColor) {
            float r = float((packedColor >> 24) & 0xFF) / 255.0;
            float g = float((packedColor >> 16) & 0xFF) / 255.0;
            float b = float((packedColor >> 8) & 0xFF) / 255.0;
            float a = float(packedColor & 0xFF) / 255.0;
            return float4(r, g, b, a);
        }

        float2 rotate2D(float2 p, float angle) {
            float s = sin(angle);
            float c = cos(angle);
            return float2(p.x * c - p.y * s, p.x * s + p.y * c);
        }

        vertex VertexOut polygon_vertex(
            uint vertexID [[vertex_id]],
            uint instanceID [[instance_id]],
            constant PolygonInstance *instances [[buffer(0)]],
            constant PolygonUniforms &uniforms [[buffer(1)]]
        ) {
            PolygonInstance poly = instances[instanceID];

            // Generate polygon as triangle fan
            // vertexID 0 = center, then vertices around edge
            uint numSides = poly.numSides;
            if (numSides < 3) numSides = 3;
            if (numSides > 12) numSides = 12;

            float2 localPos;
            float2 texCoord;

            if (vertexID == 0) {
                // Center vertex
                localPos = float2(0, 0);
                texCoord = float2(0.5, 0.5);
            } else {
                // Edge vertex
                // Map vertexID 1..N+1 to angles around circle
                uint edgeIndex = (vertexID - 1) % (numSides + 1);
                float angle = float(edgeIndex) * 2.0 * M_PI_F / float(numSides);
                angle -= M_PI_F * 0.5; // Start at top

                localPos = float2(cos(angle), sin(angle)) * poly.radius;
                texCoord = localPos / poly.radius * 0.5 + 0.5;
            }

            // Apply rotation if needed
            if (poly.rotation != 0.0) {
                localPos = rotate2D(localPos, poly.rotation);
            }

            // Translate to world position
            float2 worldPos = float2(poly.x, poly.y) + localPos;

            // Transform to clip space
            float x = (worldPos.x / uniforms.screenWidth) * 2.0 - 1.0;
            float y = 1.0 - (worldPos.y / uniforms.screenHeight) * 2.0;

            VertexOut out;
            out.position = float4(x, y, 0.0, 1.0);
            out.color = unpackColor(poly.color1);
            out.texCoord = texCoord;
            out.pixelCoord = localPos;
            out.mode = poly.mode;
            out.color1 = unpackColor(poly.color1);
            out.color2 = unpackColor(poly.color2);
            out.color3 = unpackColor(poly.color3);
            out.color4 = unpackColor(poly.color4);
            out.param1 = poly.param1;
            out.param2 = poly.param2;
            out.param3 = poly.param3;
            out.polygonRadius = poly.radius;

            return out;
        }

        fragment float4 polygon_fragment(VertexOut in [[stage_in]]) {
            float4 color = in.color1;

            // Basic gradients
            if (in.mode == GRADIENT_HORIZONTAL) {
                float t = in.texCoord.x;
                color = mix(in.color1, in.color2, t);
            }
            else if (in.mode == GRADIENT_VERTICAL) {
                float t = in.texCoord.y;
                color = mix(in.color1, in.color2, t);
            }
            else if (in.mode == GRADIENT_DIAGONAL_TL_BR) {
                float t = (in.texCoord.x + in.texCoord.y) * 0.5;
                color = mix(in.color1, in.color2, t);
            }
            else if (in.mode == GRADIENT_DIAGONAL_TR_BL) {
                float t = (in.texCoord.x + (1.0 - in.texCoord.y)) * 0.5;
                color = mix(in.color1, in.color2, t);
            }
            else if (in.mode == GRADIENT_RADIAL) {
                float2 centered = in.texCoord - float2(0.5, 0.5);
                float t = length(centered) * 2.0;
                t = clamp(t, 0.0, 1.0);
                color = mix(in.color1, in.color2, t);
            }
            else if (in.mode == GRADIENT_FOUR_CORNER) {
                float4 topColor = mix(in.color1, in.color2, in.texCoord.x);
                float4 bottomColor = mix(in.color4, in.color3, in.texCoord.x);
                color = mix(topColor, bottomColor, in.texCoord.y);
            }
            else if (in.mode == GRADIENT_THREE_POINT) {
                float t1 = in.texCoord.x;
                float t2 = in.texCoord.y;
                float t3 = 1.0 - t1 - t2;
                if (t3 < 0.0) {
                    color = mix(in.color2, in.color3, in.texCoord.y);
                } else {
                    color = in.color1 * t3 + in.color2 * t1 + in.color3 * t2;
                }
            }
            // Procedural patterns
            else if (in.mode == PATTERN_OUTLINE) {
                float lineWidth = in.param1 > 0.0 ? in.param1 : 2.0;
                float dist = length(in.pixelCoord);
                float edge = in.polygonRadius - lineWidth;
                if (dist > edge) {
                    color = in.color2; // outline color
                } else {
                    color = in.color1; // fill color
                }
            }
            else if (in.mode == PATTERN_DASHED_OUTLINE) {
                float lineWidth = in.param1 > 0.0 ? in.param1 : 2.0;
                float dashLength = in.param2 > 0.0 ? in.param2 : 10.0;
                float dist = length(in.pixelCoord);
                float edge = in.polygonRadius - lineWidth;

                if (dist > edge) {
                    float angle = atan2(in.pixelCoord.y, in.pixelCoord.x);
                    float arcLength = angle * in.polygonRadius;
                    if (fmod(abs(arcLength), dashLength * 2.0) < dashLength) {
                        color = in.color2; // dash color
                    } else {
                        color = in.color1; // fill color
                    }
                } else {
                    color = in.color1; // fill color
                }
            }
            else if (in.mode == PATTERN_HORIZONTAL_STRIPES) {
                float stripeHeight = in.param1 > 0.0 ? in.param1 : 10.0;
                float y = in.pixelCoord.y + in.polygonRadius;
                if (fmod(y, stripeHeight * 2.0) < stripeHeight) {
                    color = in.color1;
                } else {
                    color = in.color2;
                }
            }
            else if (in.mode == PATTERN_VERTICAL_STRIPES) {
                float stripeWidth = in.param1 > 0.0 ? in.param1 : 10.0;
                float x = in.pixelCoord.x + in.polygonRadius;
                if (fmod(x, stripeWidth * 2.0) < stripeWidth) {
                    color = in.color1;
                } else {
                    color = in.color2;
                }
            }
            else if (in.mode == PATTERN_DIAGONAL_STRIPES) {
                float stripeWidth = in.param1 > 0.0 ? in.param1 : 10.0;
                float angle = in.param3;
                float2 rotated = rotate2D(in.pixelCoord, -angle);
                if (fmod(rotated.x + in.polygonRadius, stripeWidth * 2.0) < stripeWidth) {
                    color = in.color1;
                } else {
                    color = in.color2;
                }
            }
            else if (in.mode == PATTERN_CHECKERBOARD) {
                float cellSize = in.param1 > 0.0 ? in.param1 : 10.0;
                float x = floor((in.pixelCoord.x + in.polygonRadius) / cellSize);
                float y = floor((in.pixelCoord.y + in.polygonRadius) / cellSize);
                if (fmod(x + y, 2.0) < 1.0) {
                    color = in.color1;
                } else {
                    color = in.color2;
                }
            }
            else if (in.mode == PATTERN_DOTS) {
                float dotRadius = in.param1 > 0.0 ? in.param1 : 3.0;
                float spacing = in.param2 > 0.0 ? in.param2 : 10.0;
                float2 gridPos = in.pixelCoord + float2(in.polygonRadius);
                float2 cellPos = fmod(gridPos + float2(spacing * 0.5), float2(spacing));
                float2 cellCenter = float2(spacing * 0.5);
                float dist = distance(cellPos, cellCenter);
                if (dist < dotRadius) {
                    color = in.color1; // dot color
                } else {
                    color = in.color2; // background color
                }
            }
            else if (in.mode == PATTERN_CROSSHATCH) {
                float lineWidth = in.param1 > 0.0 ? in.param1 : 1.0;
                float spacing = in.param2 > 0.0 ? in.param2 : 10.0;
                float x = fmod(abs(in.pixelCoord.x), spacing);
                float y = fmod(abs(in.pixelCoord.y), spacing);
                if (x < lineWidth || y < lineWidth) {
                    color = in.color1; // line color
                } else {
                    color = in.color2; // background color
                }
            }
            else if (in.mode == PATTERN_GRID) {
                float lineWidth = in.param1 > 0.0 ? in.param1 : 1.0;
                float cellSize = in.param2 > 0.0 ? in.param2 : 10.0;
                float x = fmod(in.pixelCoord.x + in.polygonRadius, cellSize);
                float y = fmod(in.pixelCoord.y + in.polygonRadius, cellSize);
                if (x < lineWidth || y < lineWidth) {
                    color = in.color1; // line color
                } else {
                    color = in.color2; // background color
                }
            }

            return color;
        }
    )";

    NSError* error = nil;
    id<MTLLibrary> library = [m_device newLibraryWithSource:shaderSource
                                                    options:nil
                                                      error:&error];
    if (!library) {
        NSLog(@"[PolygonManager] ERROR: Failed to compile shader library: %@", error);
        return false;
    }

    id<MTLFunction> vertexFunction = [library newFunctionWithName:@"polygon_vertex"];
    id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"polygon_fragment"];

    if (!vertexFunction || !fragmentFunction) {
        NSLog(@"[PolygonManager] ERROR: Failed to find shader functions");
        return false;
    }

    MTLRenderPipelineDescriptor* pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDescriptor.vertexFunction = vertexFunction;
    pipelineDescriptor.fragmentFunction = fragmentFunction;
    pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

    // Enable blending for transparency
    pipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
    pipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    pipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

    m_pipelineState = [m_device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
    if (!m_pipelineState) {
        NSLog(@"[PolygonManager] ERROR: Failed to create pipeline: %@", error);
        return false;
    }

    return true;
}

int PolygonManager::createPolygonInternal(const PolygonInstance& instance) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedPolygons.size() >= m_maxPolygons) {
        return -1;
    }

    int id = m_nextId++;
    ManagedPolygon managed;
    managed.data = instance;
    managed.visible = true;
    managed.id = id;

    m_managedPolygons[id] = managed;
    m_bufferNeedsUpdate = true;

    return id;
}

int PolygonManager::createPolygon(float x, float y, float radius, int numSides, uint32_t color) {
    PolygonInstance instance;
    instance.x = x;
    instance.y = y;
    instance.radius = radius;
    instance.numSides = numSides;
    instance.color1 = color;
    instance.mode = static_cast<uint32_t>(PolygonGradientMode::SOLID);
    return createPolygonInternal(instance);
}

int PolygonManager::createGradient(float x, float y, float radius, int numSides,
                                   uint32_t color1, uint32_t color2, PolygonGradientMode mode) {
    PolygonInstance instance;
    instance.x = x;
    instance.y = y;
    instance.radius = radius;
    instance.numSides = numSides;
    instance.color1 = color1;
    instance.color2 = color2;
    instance.mode = static_cast<uint32_t>(mode);
    return createPolygonInternal(instance);
}

int PolygonManager::createThreePointGradient(float x, float y, float radius, int numSides,
                                             uint32_t color1, uint32_t color2, uint32_t color3,
                                             PolygonGradientMode mode) {
    PolygonInstance instance;
    instance.x = x;
    instance.y = y;
    instance.radius = radius;
    instance.numSides = numSides;
    instance.color1 = color1;
    instance.color2 = color2;
    instance.color3 = color3;
    instance.mode = static_cast<uint32_t>(mode);
    return createPolygonInternal(instance);
}

int PolygonManager::createFourCornerGradient(float x, float y, float radius, int numSides,
                                             uint32_t topLeft, uint32_t topRight,
                                             uint32_t bottomRight, uint32_t bottomLeft) {
    PolygonInstance instance;
    instance.x = x;
    instance.y = y;
    instance.radius = radius;
    instance.numSides = numSides;
    instance.color1 = topLeft;
    instance.color2 = topRight;
    instance.color3 = bottomRight;
    instance.color4 = bottomLeft;
    instance.mode = static_cast<uint32_t>(PolygonGradientMode::FOUR_CORNER);
    return createPolygonInternal(instance);
}

int PolygonManager::createOutline(float x, float y, float radius, int numSides,
                                  uint32_t fillColor, uint32_t outlineColor, float lineWidth) {
    PolygonInstance instance;
    instance.x = x;
    instance.y = y;
    instance.radius = radius;
    instance.numSides = numSides;
    instance.color1 = fillColor;
    instance.color2 = outlineColor;
    instance.mode = static_cast<uint32_t>(PolygonGradientMode::OUTLINE);
    instance.param1 = lineWidth;
    return createPolygonInternal(instance);
}

int PolygonManager::createDashedOutline(float x, float y, float radius, int numSides,
                                        uint32_t fillColor, uint32_t outlineColor,
                                        float lineWidth, float dashLength) {
    PolygonInstance instance;
    instance.x = x;
    instance.y = y;
    instance.radius = radius;
    instance.numSides = numSides;
    instance.color1 = fillColor;
    instance.color2 = outlineColor;
    instance.mode = static_cast<uint32_t>(PolygonGradientMode::DASHED_OUTLINE);
    instance.param1 = lineWidth;
    instance.param2 = dashLength;
    return createPolygonInternal(instance);
}

int PolygonManager::createHorizontalStripes(float x, float y, float radius, int numSides,
                                            uint32_t color1, uint32_t color2, float stripeHeight) {
    PolygonInstance instance;
    instance.x = x;
    instance.y = y;
    instance.radius = radius;
    instance.numSides = numSides;
    instance.color1 = color1;
    instance.color2 = color2;
    instance.mode = static_cast<uint32_t>(PolygonGradientMode::HORIZONTAL_STRIPES);
    instance.param1 = stripeHeight;
    return createPolygonInternal(instance);
}

int PolygonManager::createVerticalStripes(float x, float y, float radius, int numSides,
                                          uint32_t color1, uint32_t color2, float stripeWidth) {
    PolygonInstance instance;
    instance.x = x;
    instance.y = y;
    instance.radius = radius;
    instance.numSides = numSides;
    instance.color1 = color1;
    instance.color2 = color2;
    instance.mode = static_cast<uint32_t>(PolygonGradientMode::VERTICAL_STRIPES);
    instance.param1 = stripeWidth;
    return createPolygonInternal(instance);
}

int PolygonManager::createDiagonalStripes(float x, float y, float radius, int numSides,
                                          uint32_t color1, uint32_t color2,
                                          float stripeWidth, float angle) {
    PolygonInstance instance;
    instance.x = x;
    instance.y = y;
    instance.radius = radius;
    instance.numSides = numSides;
    instance.color1 = color1;
    instance.color2 = color2;
    instance.mode = static_cast<uint32_t>(PolygonGradientMode::DIAGONAL_STRIPES);
    instance.param1 = stripeWidth;
    instance.param3 = angle;
    return createPolygonInternal(instance);
}

int PolygonManager::createCheckerboard(float x, float y, float radius, int numSides,
                                       uint32_t color1, uint32_t color2, float cellSize) {
    PolygonInstance instance;
    instance.x = x;
    instance.y = y;
    instance.radius = radius;
    instance.numSides = numSides;
    instance.color1 = color1;
    instance.color2 = color2;
    instance.mode = static_cast<uint32_t>(PolygonGradientMode::CHECKERBOARD);
    instance.param1 = cellSize;
    return createPolygonInternal(instance);
}

int PolygonManager::createDots(float x, float y, float radius, int numSides,
                               uint32_t dotColor, uint32_t backgroundColor,
                               float dotRadius, float spacing) {
    PolygonInstance instance;
    instance.x = x;
    instance.y = y;
    instance.radius = radius;
    instance.numSides = numSides;
    instance.color1 = dotColor;
    instance.color2 = backgroundColor;
    instance.mode = static_cast<uint32_t>(PolygonGradientMode::DOTS);
    instance.param1 = dotRadius;
    instance.param2 = spacing;
    return createPolygonInternal(instance);
}

int PolygonManager::createCrosshatch(float x, float y, float radius, int numSides,
                                     uint32_t lineColor, uint32_t backgroundColor,
                                     float lineWidth, float spacing) {
    PolygonInstance instance;
    instance.x = x;
    instance.y = y;
    instance.radius = radius;
    instance.numSides = numSides;
    instance.color1 = lineColor;
    instance.color2 = backgroundColor;
    instance.mode = static_cast<uint32_t>(PolygonGradientMode::CROSSHATCH);
    instance.param1 = lineWidth;
    instance.param2 = spacing;
    return createPolygonInternal(instance);
}

int PolygonManager::createGrid(float x, float y, float radius, int numSides,
                               uint32_t lineColor, uint32_t backgroundColor,
                               float lineWidth, float cellSize) {
    PolygonInstance instance;
    instance.x = x;
    instance.y = y;
    instance.radius = radius;
    instance.numSides = numSides;
    instance.color1 = lineColor;
    instance.color2 = backgroundColor;
    instance.mode = static_cast<uint32_t>(PolygonGradientMode::GRID);
    instance.param1 = lineWidth;
    instance.param2 = cellSize;
    return createPolygonInternal(instance);
}

bool PolygonManager::updatePosition(int id, float x, float y) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedPolygons.find(id);
    if (it == m_managedPolygons.end()) return false;

    it->second.data.x = x;
    it->second.data.y = y;
    m_bufferNeedsUpdate = true;
    return true;
}

bool PolygonManager::updateRadius(int id, float radius) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedPolygons.find(id);
    if (it == m_managedPolygons.end()) return false;

    it->second.data.radius = radius;
    m_bufferNeedsUpdate = true;
    return true;
}

bool PolygonManager::updateSides(int id, int numSides) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedPolygons.find(id);
    if (it == m_managedPolygons.end()) return false;

    it->second.data.numSides = numSides;
    m_bufferNeedsUpdate = true;
    return true;
}

bool PolygonManager::updateColor(int id, uint32_t color) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedPolygons.find(id);
    if (it == m_managedPolygons.end()) return false;

    it->second.data.color1 = color;
    m_bufferNeedsUpdate = true;
    return true;
}

bool PolygonManager::updateColors(int id, uint32_t color1, uint32_t color2,
                                  uint32_t color3, uint32_t color4) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedPolygons.find(id);
    if (it == m_managedPolygons.end()) return false;

    it->second.data.color1 = color1;
    it->second.data.color2 = color2;
    it->second.data.color3 = color3;
    it->second.data.color4 = color4;
    m_bufferNeedsUpdate = true;
    return true;
}

bool PolygonManager::updateMode(int id, PolygonGradientMode mode) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedPolygons.find(id);
    if (it == m_managedPolygons.end()) return false;

    it->second.data.mode = static_cast<uint32_t>(mode);
    m_bufferNeedsUpdate = true;
    return true;
}

bool PolygonManager::updateParameters(int id, float param1, float param2, float param3) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedPolygons.find(id);
    if (it == m_managedPolygons.end()) return false;

    it->second.data.param1 = param1;
    it->second.data.param2 = param2;
    it->second.data.param3 = param3;
    m_bufferNeedsUpdate = true;
    return true;
}

bool PolygonManager::setRotation(int id, float angleDegrees) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedPolygons.find(id);
    if (it == m_managedPolygons.end()) return false;

    it->second.data.rotation = angleDegrees * M_PI / 180.0f;
    m_bufferNeedsUpdate = true;
    return true;
}

bool PolygonManager::setVisible(int id, bool visible) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedPolygons.find(id);
    if (it == m_managedPolygons.end()) return false;

    it->second.visible = visible;
    m_bufferNeedsUpdate = true;
    return true;
}

bool PolygonManager::exists(int id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_managedPolygons.find(id) != m_managedPolygons.end();
}

bool PolygonManager::isVisible(int id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedPolygons.find(id);
    if (it == m_managedPolygons.end()) return false;
    return it->second.visible;
}

bool PolygonManager::deletePolygon(int id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedPolygons.find(id);
    if (it == m_managedPolygons.end()) return false;

    m_managedPolygons.erase(it);
    m_bufferNeedsUpdate = true;
    return true;
}

void PolygonManager::deleteAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_managedPolygons.clear();
    m_bufferNeedsUpdate = true;
}

size_t PolygonManager::count() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_managedPolygons.size();
}

bool PolygonManager::isEmpty() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_managedPolygons.empty();
}

void PolygonManager::updateInstanceBuffer() {
    if (!m_bufferNeedsUpdate || m_managedPolygons.empty()) {
        return;
    }

    std::vector<PolygonInstance> visiblePolygons;
    visiblePolygons.reserve(m_managedPolygons.size());

    for (const auto& pair : m_managedPolygons) {
        if (pair.second.visible) {
            visiblePolygons.push_back(pair.second.data);
        }
    }

    if (!visiblePolygons.empty()) {
        if (!m_instanceBuffer) {
            NSLog(@"[PolygonManager] ERROR: Instance buffer is nil");
            return;
        }

        void* bufferContents = [m_instanceBuffer contents];
        if (!bufferContents) {
            NSLog(@"[PolygonManager] ERROR: Buffer contents is nil");
            return;
        }

        size_t dataSize = sizeof(PolygonInstance) * visiblePolygons.size();
        memcpy(bufferContents, visiblePolygons.data(), dataSize);
    }

    m_bufferNeedsUpdate = false;
}

void PolygonManager::render(id<MTLRenderCommandEncoder> encoder) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedPolygons.empty() || !m_pipelineState) {
        return;
    }

    updateInstanceBuffer();

    size_t visibleCount = 0;
    for (const auto& pair : m_managedPolygons) {
        if (pair.second.visible) visibleCount++;
    }

    if (visibleCount == 0) return;

    [encoder setRenderPipelineState:m_pipelineState];
    [encoder setVertexBuffer:m_instanceBuffer offset:0 atIndex:0];
    [encoder setVertexBuffer:m_uniformBuffer offset:0 atIndex:1];

    // Draw each polygon as a triangle fan
    // For N-sided polygon: 1 center vertex + N+1 edge vertices (last wraps to first)
    for (const auto& pair : m_managedPolygons) {
        if (!pair.second.visible) continue;

        uint32_t numSides = pair.second.data.numSides;
        if (numSides < 3) numSides = 3;
        if (numSides > 12) numSides = 12;

        uint32_t vertexCount = 1 + numSides + 1; // center + edges + wraparound

        [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
                    vertexStart:0
                    vertexCount:vertexCount
                  instanceCount:1];
    }
}
