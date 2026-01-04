//
//  RectangleManager.cpp
//  SuperTerminal Framework - GPU-Accelerated Rectangle Rendering
//
//  Copyright © 2024 SuperTerminal. All rights reserved.
//  Composite rectangles in their own layer.

#include "RectangleManager.h"
#include <algorithm>

namespace SuperTerminal {

// =============================================================================
// Constructor / Destructor
// =============================================================================

RectangleManager::RectangleManager()
    : m_device(nil)
    , m_pipelineState(nil)
    , m_instanceBuffer(nil)
    , m_uniformBuffer(nil)
    , m_nextId(1)
    , m_maxRectangles(10000)
    , m_bufferNeedsUpdate(false)
    , m_screenWidth(1920)
    , m_screenHeight(1080)
{
}

RectangleManager::~RectangleManager() {
    // Metal objects are reference counted, they'll clean up automatically
}

// =============================================================================
// Initialization
// =============================================================================

bool RectangleManager::initialize(id<MTLDevice> device, int screenWidth, int screenHeight) {
    m_device = device;
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;

    // Create the render pipeline
    if (!createPipeline()) {
        NSLog(@"[RectangleManager] ERROR: Failed to create pipeline");
        return false;
    }

    // Create instance buffer (will be resized as needed)
    m_instanceBuffer = [m_device newBufferWithLength:m_maxRectangles * sizeof(RectangleInstance)
                                             options:MTLResourceStorageModeShared];
    if (!m_instanceBuffer) {
        NSLog(@"[RectangleManager] ERROR: Failed to create instance buffer");
        return false;
    }

    // Create uniform buffer
    m_uniformBuffer = [m_device newBufferWithLength:sizeof(RectangleUniforms)
                                            options:MTLResourceStorageModeShared];
    if (!m_uniformBuffer) {
        NSLog(@"[RectangleManager] ERROR: Failed to create uniform buffer");
        return false;
    }

    // Initialize uniforms
    updateUniforms();

    NSLog(@"[RectangleManager] Initialized successfully (max rectangles: %zu)", m_maxRectangles);
    return true;
}

bool RectangleManager::createPipeline() {
    NSError* error = nil;

    // Compile shaders from inline source
    NSString* shaderSource = @R"(
        #include <metal_stdlib>
        using namespace metal;

        // Rectangle instance data (matches C++ RectangleInstance)
        struct RectangleInstance {
            float x;
            float y;
            float width;
            float height;
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
        struct RectangleUniforms {
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
            float rectWidth;
            float rectHeight;
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
        constant uint PATTERN_ROUNDED_CORNERS = 108;
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

        vertex VertexOut rectangle_vertex(
            uint vertexID [[vertex_id]],
            uint instanceID [[instance_id]],
            constant RectangleInstance *instances [[buffer(0)]],
            constant RectangleUniforms &uniforms [[buffer(1)]]
        ) {
            RectangleInstance rect = instances[instanceID];

            // Generate quad vertices relative to center (2 triangles = 6 vertices)
            float halfW = rect.width * 0.5;
            float halfH = rect.height * 0.5;

            float2 localPositions[6] = {
                float2(-halfW, -halfH),
                float2(halfW, halfH),
                float2(halfW, -halfH),
                float2(-halfW, -halfH),
                float2(-halfW, halfH),
                float2(halfW, halfH)
            };

            float2 texCoords[6] = {
                float2(0, 0), float2(1, 1), float2(1, 0),
                float2(0, 0), float2(0, 1), float2(1, 1)
            };

            float2 localPos = localPositions[vertexID];
            float2 texCoord = texCoords[vertexID];

            // Apply rotation if needed
            if (rect.rotation != 0.0) {
                localPos = rotate2D(localPos, rect.rotation);
            }

            // Translate to world position (rect.x, rect.y is center)
            float2 worldPos = float2(rect.x, rect.y) + localPos;

            // Transform to clip space
            float x = (worldPos.x / uniforms.screenWidth) * 2.0 - 1.0;
            float y = 1.0 - (worldPos.y / uniforms.screenHeight) * 2.0;

            VertexOut out;
            out.position = float4(x, y, 0.0, 1.0);
            out.texCoord = texCoord;
            out.pixelCoord = texCoord * float2(rect.width, rect.height);
            out.mode = rect.mode;
            out.color1 = unpackColor(rect.color1);
            out.color2 = unpackColor(rect.color2);
            out.color3 = unpackColor(rect.color3);
            out.color4 = unpackColor(rect.color4);
            out.param1 = rect.param1;
            out.param2 = rect.param2;
            out.param3 = rect.param3;
            out.rectWidth = rect.width;
            out.rectHeight = rect.height;

            if (rect.mode == GRADIENT_SOLID) {
                out.color = out.color1;
            } else {
                out.color = float4(0, 0, 0, 0);
            }

            return out;
        }

        fragment float4 rectangle_fragment(VertexOut in [[stage_in]]) {
            if (in.mode == GRADIENT_SOLID) {
                return in.color;
            }

            float2 uv = in.texCoord;
            float2 pixelCoord = in.pixelCoord;
            float4 color;

            // Standard gradients
            if (in.mode == GRADIENT_HORIZONTAL) {
                color = mix(in.color1, in.color2, uv.x);
            }
            else if (in.mode == GRADIENT_VERTICAL) {
                color = mix(in.color1, in.color2, uv.y);
            }
            else if (in.mode == GRADIENT_DIAGONAL_TL_BR) {
                float t = (uv.x + uv.y) * 0.5;
                color = mix(in.color1, in.color2, t);
            }
            else if (in.mode == GRADIENT_DIAGONAL_TR_BL) {
                float t = (1.0 - uv.x + uv.y) * 0.5;
                color = mix(in.color1, in.color2, t);
            }
            else if (in.mode == GRADIENT_RADIAL) {
                float2 center = float2(0.5, 0.5);
                float dist = distance(uv, center) * 1.41421356;
                color = mix(in.color1, in.color2, clamp(dist, 0.0, 1.0));
            }
            else if (in.mode == GRADIENT_FOUR_CORNER) {
                float4 top = mix(in.color1, in.color2, uv.x);
                float4 bottom = mix(in.color4, in.color3, uv.x);
                color = mix(top, bottom, uv.y);
            }
            else if (in.mode == GRADIENT_THREE_POINT) {
                if (uv.y < 0.5) {
                    color = mix(in.color1, in.color2, uv.x);
                } else {
                    float4 topColor = mix(in.color1, in.color2, uv.x);
                    float bottomBlend = (uv.y - 0.5) * 2.0;
                    color = mix(topColor, in.color3, bottomBlend);
                }
            }
            // Procedural patterns
            else if (in.mode == PATTERN_OUTLINE) {
                // param1 = lineWidth
                float lineWidth = in.param1;
                float distFromEdge = min(min(pixelCoord.x, pixelCoord.y),
                                        min(in.rectWidth - pixelCoord.x, in.rectHeight - pixelCoord.y));
                float edge = smoothstep(lineWidth - 1.0, lineWidth + 1.0, distFromEdge);
                color = mix(in.color2, in.color1, edge);
            }
            else if (in.mode == PATTERN_DASHED_OUTLINE) {
                // param1 = lineWidth, param2 = dashLength
                float lineWidth = in.param1;
                float dashLength = in.param2;
                float distFromEdge = min(min(pixelCoord.x, pixelCoord.y),
                                        min(in.rectWidth - pixelCoord.x, in.rectHeight - pixelCoord.y));

                // Calculate perimeter position for dashing
                float perimeterPos;
                if (pixelCoord.y < lineWidth) {
                    perimeterPos = pixelCoord.x; // Top edge
                } else if (pixelCoord.x > in.rectWidth - lineWidth) {
                    perimeterPos = in.rectWidth + pixelCoord.y; // Right edge
                } else if (pixelCoord.y > in.rectHeight - lineWidth) {
                    perimeterPos = in.rectWidth + in.rectHeight + (in.rectWidth - pixelCoord.x); // Bottom edge
                } else {
                    perimeterPos = 2.0 * in.rectWidth + in.rectHeight + (in.rectHeight - pixelCoord.y); // Left edge
                }

                float dashPattern = fmod(perimeterPos, dashLength * 2.0) < dashLength ? 1.0 : 0.0;
                float isOutline = distFromEdge < lineWidth ? 1.0 : 0.0;
                color = mix(in.color1, in.color2, isOutline * dashPattern);
            }
            else if (in.mode == PATTERN_HORIZONTAL_STRIPES) {
                // param1 = stripeHeight
                float stripeHeight = in.param1;
                float stripe = fmod(pixelCoord.y, stripeHeight * 2.0) < stripeHeight ? 0.0 : 1.0;
                color = mix(in.color1, in.color2, stripe);
            }
            else if (in.mode == PATTERN_VERTICAL_STRIPES) {
                // param1 = stripeWidth
                float stripeWidth = in.param1;
                float stripe = fmod(pixelCoord.x, stripeWidth * 2.0) < stripeWidth ? 0.0 : 1.0;
                color = mix(in.color1, in.color2, stripe);
            }
            else if (in.mode == PATTERN_DIAGONAL_STRIPES) {
                // param1 = stripeWidth, param2 = angle in degrees
                float stripeWidth = in.param1;
                float angle = in.param2 * 3.14159265 / 180.0;
                float2 rotated = float2(
                    pixelCoord.x * cos(angle) - pixelCoord.y * sin(angle),
                    pixelCoord.x * sin(angle) + pixelCoord.y * cos(angle)
                );
                float stripe = fmod(rotated.x, stripeWidth * 2.0) < stripeWidth ? 0.0 : 1.0;
                color = mix(in.color1, in.color2, stripe);
            }
            else if (in.mode == PATTERN_CHECKERBOARD) {
                // param1 = cellSize
                float cellSize = in.param1;
                float checkX = fmod(floor(pixelCoord.x / cellSize), 2.0);
                float checkY = fmod(floor(pixelCoord.y / cellSize), 2.0);
                float checker = checkX == checkY ? 0.0 : 1.0;
                color = mix(in.color1, in.color2, checker);
            }
            else if (in.mode == PATTERN_DOTS) {
                // param1 = dotRadius, param2 = spacing
                float dotRadius = in.param1;
                float spacing = in.param2;
                float2 cellPos = fmod(pixelCoord, float2(spacing));
                float2 cellCenter = float2(spacing * 0.5);
                float dist = distance(cellPos, cellCenter);
                float dot = smoothstep(dotRadius + 1.0, dotRadius - 1.0, dist);
                color = mix(in.color2, in.color1, dot);
            }
            else if (in.mode == PATTERN_CROSSHATCH) {
                // param1 = lineWidth, param2 = spacing
                float lineWidth = in.param1;
                float spacing = in.param2;
                float horizontal = fmod(pixelCoord.y, spacing) < lineWidth ? 1.0 : 0.0;
                float vertical = fmod(pixelCoord.x, spacing) < lineWidth ? 1.0 : 0.0;
                float crosshatch = max(horizontal, vertical);
                color = mix(in.color2, in.color1, crosshatch);
            }
            else if (in.mode == PATTERN_ROUNDED_CORNERS) {
                // param1 = cornerRadius
                float radius = in.param1;
                float2 pos = pixelCoord;
                float2 topLeft = float2(radius, radius);
                float2 topRight = float2(in.rectWidth - radius, radius);
                float2 bottomLeft = float2(radius, in.rectHeight - radius);
                float2 bottomRight = float2(in.rectWidth - radius, in.rectHeight - radius);

                float cornerDist = 1e10;
                if (pos.x < radius && pos.y < radius) {
                    cornerDist = distance(pos, topLeft);
                } else if (pos.x > in.rectWidth - radius && pos.y < radius) {
                    cornerDist = distance(pos, topRight);
                } else if (pos.x < radius && pos.y > in.rectHeight - radius) {
                    cornerDist = distance(pos, bottomLeft);
                } else if (pos.x > in.rectWidth - radius && pos.y > in.rectHeight - radius) {
                    cornerDist = distance(pos, bottomRight);
                }

                float alpha = smoothstep(radius + 1.0, radius - 1.0, cornerDist);
                color = in.color1;
                color.a *= alpha;
            }
            else if (in.mode == PATTERN_GRID) {
                // param1 = lineWidth, param2 = cellSize
                float lineWidth = in.param1;
                float cellSize = in.param2;
                float horizontal = fmod(pixelCoord.y, cellSize) < lineWidth ? 1.0 : 0.0;
                float vertical = fmod(pixelCoord.x, cellSize) < lineWidth ? 1.0 : 0.0;
                float grid = max(horizontal, vertical);
                color = mix(in.color2, in.color1, grid);
            }
            else {
                color = in.color1;
            }

            return color;
        }
    )";

    id<MTLLibrary> library = [m_device newLibraryWithSource:shaderSource options:nil error:&error];
    if (!library) {
        NSLog(@"[RectangleManager] ERROR: Failed to compile shader library: %@", error.localizedDescription);
        return false;
    }

    id<MTLFunction> vertexFunction = [library newFunctionWithName:@"rectangle_vertex"];
    id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"rectangle_fragment"];

    if (!vertexFunction || !fragmentFunction) {
        NSLog(@"[RectangleManager] ERROR: Failed to load shader functions from compiled library");
        return false;
    }

    // Create pipeline descriptor
    MTLRenderPipelineDescriptor* pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDescriptor.label = @"Rectangle Pipeline";
    pipelineDescriptor.vertexFunction = vertexFunction;
    pipelineDescriptor.fragmentFunction = fragmentFunction;
    pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

    // Enable alpha blending
    pipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
    pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
    pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;

    // Create pipeline state
    m_pipelineState = [m_device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
    if (!m_pipelineState) {
        NSLog(@"[RectangleManager] ERROR: Failed to create pipeline state: %@", error.localizedDescription);
        return false;
    }

    return true;
}

// =============================================================================
// ID-Based Rectangle Management
// =============================================================================

int RectangleManager::createRectangle(float x, float y, float width, float height, uint32_t color) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedRectangles.size() >= m_maxRectangles) {
        NSLog(@"[RectangleManager] WARNING: Max rectangles (%zu) reached", m_maxRectangles);
        return -1;
    }

    int id = m_nextId++;
    ManagedRectangle& rect = m_managedRectangles[id];
    rect.data.x = x;
    rect.data.y = y;
    rect.data.width = width;
    rect.data.height = height;
    rect.data.color1 = color;
    rect.data.color2 = color;
    rect.data.color3 = color;
    rect.data.color4 = color;
    rect.data.mode = static_cast<uint32_t>(RectangleGradientMode::SOLID);
    rect.data.param1 = 0.0f;
    rect.data.param2 = 0.0f;
    rect.data.param3 = 0.0f;
    rect.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

int RectangleManager::createGradient(float x, float y, float width, float height,
                                      uint32_t color1, uint32_t color2,
                                      RectangleGradientMode mode) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedRectangles.size() >= m_maxRectangles) {
        NSLog(@"[RectangleManager] WARNING: Max rectangles (%zu) reached", m_maxRectangles);
        return -1;
    }

    int id = m_nextId++;
    ManagedRectangle& rect = m_managedRectangles[id];
    rect.data.x = x;
    rect.data.y = y;
    rect.data.width = width;
    rect.data.height = height;
    rect.data.color1 = color1;
    rect.data.color2 = color2;
    rect.data.color3 = color2;
    rect.data.color4 = color2;
    rect.data.mode = static_cast<uint32_t>(mode);
    rect.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

int RectangleManager::createThreePointGradient(float x, float y, float width, float height,
                                                uint32_t color1, uint32_t color2, uint32_t color3,
                                                RectangleGradientMode mode) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedRectangles.size() >= m_maxRectangles) {
        NSLog(@"[RectangleManager] WARNING: Max rectangles (%zu) reached", m_maxRectangles);
        return -1;
    }

    int id = m_nextId++;
    ManagedRectangle& rect = m_managedRectangles[id];
    rect.data.x = x;
    rect.data.y = y;
    rect.data.width = width;
    rect.data.height = height;
    rect.data.color1 = color1;
    rect.data.color2 = color2;
    rect.data.color3 = color3;
    rect.data.color4 = color3;
    rect.data.mode = static_cast<uint32_t>(mode);
    rect.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

int RectangleManager::createFourCornerGradient(float x, float y, float width, float height,
                                                uint32_t topLeft, uint32_t topRight,
                                                uint32_t bottomRight, uint32_t bottomLeft) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedRectangles.size() >= m_maxRectangles) {
        NSLog(@"[RectangleManager] WARNING: Max rectangles (%zu) reached", m_maxRectangles);
        return -1;
    }

    int id = m_nextId++;
    ManagedRectangle& rect = m_managedRectangles[id];
    rect.data.x = x;
    rect.data.y = y;
    rect.data.width = width;
    rect.data.height = height;
    rect.data.color1 = topLeft;
    rect.data.color2 = topRight;
    rect.data.color3 = bottomRight;
    rect.data.color4 = bottomLeft;
    rect.data.mode = static_cast<uint32_t>(RectangleGradientMode::FOUR_CORNER);
    rect.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

// =============================================================================
// Procedural Pattern Creation Functions
// =============================================================================

int RectangleManager::createOutline(float x, float y, float width, float height,
                                     uint32_t fillColor, uint32_t outlineColor, float lineWidth) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedRectangles.size() >= m_maxRectangles) {
        NSLog(@"[RectangleManager] WARNING: Max rectangles (%zu) reached", m_maxRectangles);
        return -1;
    }

    int id = m_nextId++;
    ManagedRectangle& rect = m_managedRectangles[id];
    rect.data.x = x;
    rect.data.y = y;
    rect.data.width = width;
    rect.data.height = height;
    rect.data.color1 = fillColor;
    rect.data.color2 = outlineColor;
    rect.data.color3 = fillColor;
    rect.data.color4 = fillColor;
    rect.data.mode = static_cast<uint32_t>(RectangleGradientMode::OUTLINE);
    rect.data.param1 = lineWidth;
    rect.data.param2 = 0.0f;
    rect.data.param3 = 0.0f;
    rect.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

int RectangleManager::createDashedOutline(float x, float y, float width, float height,
                                           uint32_t fillColor, uint32_t outlineColor,
                                           float lineWidth, float dashLength) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedRectangles.size() >= m_maxRectangles) {
        NSLog(@"[RectangleManager] WARNING: Max rectangles (%zu) reached", m_maxRectangles);
        return -1;
    }

    int id = m_nextId++;
    ManagedRectangle& rect = m_managedRectangles[id];
    rect.data.x = x;
    rect.data.y = y;
    rect.data.width = width;
    rect.data.height = height;
    rect.data.color1 = fillColor;
    rect.data.color2 = outlineColor;
    rect.data.color3 = fillColor;
    rect.data.color4 = fillColor;
    rect.data.mode = static_cast<uint32_t>(RectangleGradientMode::DASHED_OUTLINE);
    rect.data.param1 = lineWidth;
    rect.data.param2 = dashLength;
    rect.data.param3 = 0.0f;
    rect.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

int RectangleManager::createHorizontalStripes(float x, float y, float width, float height,
                                               uint32_t color1, uint32_t color2, float stripeHeight) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedRectangles.size() >= m_maxRectangles) {
        NSLog(@"[RectangleManager] WARNING: Max rectangles (%zu) reached", m_maxRectangles);
        return -1;
    }

    int id = m_nextId++;
    ManagedRectangle& rect = m_managedRectangles[id];
    rect.data.x = x;
    rect.data.y = y;
    rect.data.width = width;
    rect.data.height = height;
    rect.data.color1 = color1;
    rect.data.color2 = color2;
    rect.data.color3 = color1;
    rect.data.color4 = color1;
    rect.data.mode = static_cast<uint32_t>(RectangleGradientMode::HORIZONTAL_STRIPES);
    rect.data.param1 = stripeHeight;
    rect.data.param2 = 0.0f;
    rect.data.param3 = 0.0f;
    rect.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

int RectangleManager::createVerticalStripes(float x, float y, float width, float height,
                                             uint32_t color1, uint32_t color2, float stripeWidth) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedRectangles.size() >= m_maxRectangles) {
        NSLog(@"[RectangleManager] WARNING: Max rectangles (%zu) reached", m_maxRectangles);
        return -1;
    }

    int id = m_nextId++;
    ManagedRectangle& rect = m_managedRectangles[id];
    rect.data.x = x;
    rect.data.y = y;
    rect.data.width = width;
    rect.data.height = height;
    rect.data.color1 = color1;
    rect.data.color2 = color2;
    rect.data.color3 = color1;
    rect.data.color4 = color1;
    rect.data.mode = static_cast<uint32_t>(RectangleGradientMode::VERTICAL_STRIPES);
    rect.data.param1 = stripeWidth;
    rect.data.param2 = 0.0f;
    rect.data.param3 = 0.0f;
    rect.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

int RectangleManager::createDiagonalStripes(float x, float y, float width, float height,
                                             uint32_t color1, uint32_t color2, float stripeWidth, float angle) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedRectangles.size() >= m_maxRectangles) {
        NSLog(@"[RectangleManager] WARNING: Max rectangles (%zu) reached", m_maxRectangles);
        return -1;
    }

    int id = m_nextId++;
    ManagedRectangle& rect = m_managedRectangles[id];
    rect.data.x = x;
    rect.data.y = y;
    rect.data.width = width;
    rect.data.height = height;
    rect.data.color1 = color1;
    rect.data.color2 = color2;
    rect.data.color3 = color1;
    rect.data.color4 = color1;
    rect.data.mode = static_cast<uint32_t>(RectangleGradientMode::DIAGONAL_STRIPES);
    rect.data.param1 = stripeWidth;
    rect.data.param2 = angle;
    rect.data.param3 = 0.0f;
    rect.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

int RectangleManager::createCheckerboard(float x, float y, float width, float height,
                                          uint32_t color1, uint32_t color2, float cellSize) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedRectangles.size() >= m_maxRectangles) {
        NSLog(@"[RectangleManager] WARNING: Max rectangles (%zu) reached", m_maxRectangles);
        return -1;
    }

    int id = m_nextId++;
    ManagedRectangle& rect = m_managedRectangles[id];
    rect.data.x = x;
    rect.data.y = y;
    rect.data.width = width;
    rect.data.height = height;
    rect.data.color1 = color1;
    rect.data.color2 = color2;
    rect.data.color3 = color1;
    rect.data.color4 = color1;
    rect.data.mode = static_cast<uint32_t>(RectangleGradientMode::CHECKERBOARD);
    rect.data.param1 = cellSize;
    rect.data.param2 = 0.0f;
    rect.data.param3 = 0.0f;
    rect.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

int RectangleManager::createDots(float x, float y, float width, float height,
                                  uint32_t dotColor, uint32_t backgroundColor,
                                  float dotRadius, float spacing) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedRectangles.size() >= m_maxRectangles) {
        NSLog(@"[RectangleManager] WARNING: Max rectangles (%zu) reached", m_maxRectangles);
        return -1;
    }

    int id = m_nextId++;
    ManagedRectangle& rect = m_managedRectangles[id];
    rect.data.x = x;
    rect.data.y = y;
    rect.data.width = width;
    rect.data.height = height;
    rect.data.color1 = dotColor;
    rect.data.color2 = backgroundColor;
    rect.data.color3 = dotColor;
    rect.data.color4 = dotColor;
    rect.data.mode = static_cast<uint32_t>(RectangleGradientMode::DOTS);
    rect.data.param1 = dotRadius;
    rect.data.param2 = spacing;
    rect.data.param3 = 0.0f;
    rect.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

int RectangleManager::createCrosshatch(float x, float y, float width, float height,
                                        uint32_t lineColor, uint32_t backgroundColor,
                                        float lineWidth, float spacing) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedRectangles.size() >= m_maxRectangles) {
        NSLog(@"[RectangleManager] WARNING: Max rectangles (%zu) reached", m_maxRectangles);
        return -1;
    }

    int id = m_nextId++;
    ManagedRectangle& rect = m_managedRectangles[id];
    rect.data.x = x;
    rect.data.y = y;
    rect.data.width = width;
    rect.data.height = height;
    rect.data.color1 = lineColor;
    rect.data.color2 = backgroundColor;
    rect.data.color3 = lineColor;
    rect.data.color4 = lineColor;
    rect.data.mode = static_cast<uint32_t>(RectangleGradientMode::CROSSHATCH);
    rect.data.param1 = lineWidth;
    rect.data.param2 = spacing;
    rect.data.param3 = 0.0f;
    rect.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

int RectangleManager::createRoundedCorners(float x, float y, float width, float height,
                                            uint32_t color, float cornerRadius) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedRectangles.size() >= m_maxRectangles) {
        NSLog(@"[RectangleManager] WARNING: Max rectangles (%zu) reached", m_maxRectangles);
        return -1;
    }

    int id = m_nextId++;
    ManagedRectangle& rect = m_managedRectangles[id];
    rect.data.x = x;
    rect.data.y = y;
    rect.data.width = width;
    rect.data.height = height;
    rect.data.color1 = color;
    rect.data.color2 = color;
    rect.data.color3 = color;
    rect.data.color4 = color;
    rect.data.mode = static_cast<uint32_t>(RectangleGradientMode::ROUNDED_CORNERS);
    rect.data.param1 = cornerRadius;
    rect.data.param2 = 0.0f;
    rect.data.param3 = 0.0f;
    rect.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

int RectangleManager::createGrid(float x, float y, float width, float height,
                                  uint32_t lineColor, uint32_t backgroundColor,
                                  float lineWidth, float cellSize) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedRectangles.size() >= m_maxRectangles) {
        NSLog(@"[RectangleManager] WARNING: Max rectangles (%zu) reached", m_maxRectangles);
        return -1;
    }

    int id = m_nextId++;
    ManagedRectangle& rect = m_managedRectangles[id];
    rect.data.x = x;
    rect.data.y = y;
    rect.data.width = width;
    rect.data.height = height;
    rect.data.color1 = lineColor;
    rect.data.color2 = backgroundColor;
    rect.data.color3 = lineColor;
    rect.data.color4 = lineColor;
    rect.data.mode = static_cast<uint32_t>(RectangleGradientMode::GRID);
    rect.data.param1 = lineWidth;
    rect.data.param2 = cellSize;
    rect.data.param3 = 0.0f;
    rect.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

// =============================================================================
// Update Functions
// =============================================================================

bool RectangleManager::updatePosition(int id, float x, float y) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedRectangles.find(id);
    if (it == m_managedRectangles.end()) return false;

    it->second.data.x = x;
    it->second.data.y = y;
    m_bufferNeedsUpdate = true;
    return true;
}

bool RectangleManager::updateSize(int id, float width, float height) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedRectangles.find(id);
    if (it == m_managedRectangles.end()) return false;

    it->second.data.width = width;
    it->second.data.height = height;
    m_bufferNeedsUpdate = true;
    return true;
}

bool RectangleManager::updateColor(int id, uint32_t color) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedRectangles.find(id);
    if (it == m_managedRectangles.end()) return false;

    it->second.data.color1 = color;
    it->second.data.color2 = color;
    it->second.data.color3 = color;
    it->second.data.color4 = color;
    m_bufferNeedsUpdate = true;
    return true;
}

bool RectangleManager::updateColors(int id, uint32_t color1, uint32_t color2, uint32_t color3, uint32_t color4) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedRectangles.find(id);
    if (it == m_managedRectangles.end()) return false;

    it->second.data.color1 = color1;
    it->second.data.color2 = color2;
    it->second.data.color3 = color3;
    it->second.data.color4 = color4;
    m_bufferNeedsUpdate = true;
    return true;
}

bool RectangleManager::updateMode(int id, RectangleGradientMode mode) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedRectangles.find(id);
    if (it == m_managedRectangles.end()) return false;

    it->second.data.mode = static_cast<uint32_t>(mode);
    m_bufferNeedsUpdate = true;
    return true;
}

bool RectangleManager::updateParameters(int id, float param1, float param2, float param3) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedRectangles.find(id);
    if (it == m_managedRectangles.end()) return false;

    it->second.data.param1 = param1;
    it->second.data.param2 = param2;
    it->second.data.param3 = param3;
    m_bufferNeedsUpdate = true;
    return true;
}

bool RectangleManager::setRotation(int id, float angleDegrees) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedRectangles.find(id);
    if (it == m_managedRectangles.end()) return false;

    // Convert degrees to radians
    it->second.data.rotation = angleDegrees * M_PI / 180.0f;
    m_bufferNeedsUpdate = true;
    return true;
}

bool RectangleManager::setVisible(int id, bool visible) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedRectangles.find(id);
    if (it == m_managedRectangles.end()) return false;

    it->second.visible = visible;
    m_bufferNeedsUpdate = true;
    return true;
}

bool RectangleManager::exists(int id) const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_mutex));
    return m_managedRectangles.find(id) != m_managedRectangles.end();
}

bool RectangleManager::isVisible(int id) const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_mutex));
    auto it = m_managedRectangles.find(id);
    if (it == m_managedRectangles.end()) return false;
    return it->second.visible;
}

bool RectangleManager::deleteRectangle(int id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedRectangles.find(id);
    if (it == m_managedRectangles.end()) return false;

    m_managedRectangles.erase(it);
    m_bufferNeedsUpdate = true;
    return true;
}

void RectangleManager::deleteAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_managedRectangles.clear();
    m_bufferNeedsUpdate = true;
}

// =============================================================================
// Statistics and Management
// =============================================================================

size_t RectangleManager::getRectangleCount() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_mutex));
    return m_managedRectangles.size();
}

bool RectangleManager::isEmpty() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_mutex));
    return m_managedRectangles.empty();
}

void RectangleManager::setMaxRectangles(size_t max) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxRectangles = max;

    // Recreate instance buffer with new size
    if (m_device) {
        m_instanceBuffer = [m_device newBufferWithLength:m_maxRectangles * sizeof(RectangleInstance)
                                                 options:MTLResourceStorageModeShared];
    }
}

// =============================================================================
// Rendering
// =============================================================================

void RectangleManager::rebuildRenderList() {
    // Rebuild the render list from managed rectangles only
    std::vector<RectangleInstance> renderList;
    renderList.reserve(m_managedRectangles.size());

    // Add visible managed rectangles
    for (const auto& pair : m_managedRectangles) {
        if (pair.second.visible) {
            renderList.push_back(pair.second.data);
        }
    }

    if (renderList.empty()) {
        return;
    }

    // Null check for device and buffer
    if (!m_device) {
        NSLog(@"[RectangleManager] ERROR: Device is nil, cannot rebuild render list");
        return;
    }

    // Check if we need a larger buffer
    size_t requiredSize = renderList.size() * sizeof(RectangleInstance);
    if (!m_instanceBuffer || requiredSize > [m_instanceBuffer length]) {
        size_t newSize = renderList.size() * sizeof(RectangleInstance) * 2;
        NSLog(@"[RectangleManager] %s instance buffer to %zu bytes",
              m_instanceBuffer ? "Resizing" : "Creating", newSize);
        m_instanceBuffer = [m_device newBufferWithLength:newSize
                                                 options:MTLResourceStorageModeShared];
        if (!m_instanceBuffer) {
            NSLog(@"[RectangleManager] ERROR: Failed to create instance buffer");
            return;
        }
    }

    // Copy to GPU buffer
    void* bufferContents = [m_instanceBuffer contents];
    if (bufferContents) {
        memcpy(bufferContents, renderList.data(), requiredSize);
    } else {
        NSLog(@"[RectangleManager] ERROR: Buffer contents is nil");
    }
}

void RectangleManager::updateInstanceBuffer() {
    if (!m_bufferNeedsUpdate) {
        return;
    }

    rebuildRenderList();
    m_bufferNeedsUpdate = false;
}

void RectangleManager::updateUniforms() {
    if (!m_uniformBuffer) return;

    void* bufferContents = [m_uniformBuffer contents];
    if (!bufferContents) {
        NSLog(@"[RectangleManager] ERROR: Uniform buffer contents is nil");
        return;
    }

    RectangleUniforms* uniforms = (RectangleUniforms*)bufferContents;
    uniforms->screenWidth = static_cast<float>(m_screenWidth);
    uniforms->screenHeight = static_cast<float>(m_screenHeight);
}

void RectangleManager::render(id<MTLRenderCommandEncoder> encoder) {
    std::lock_guard<std::mutex> lock(m_mutex);

    size_t totalRectangles = 0;
    for (const auto& pair : m_managedRectangles) {
        if (pair.second.visible) totalRectangles++;
    }

    // Null checks for Metal resources
    if (totalRectangles == 0 || !m_pipelineState || !m_instanceBuffer || !m_uniformBuffer || !encoder) {
        return;
    }

    // Update buffers if needed
    updateInstanceBuffer();

    // Set pipeline state
    [encoder setRenderPipelineState:m_pipelineState];

    // Bind buffers
    [encoder setVertexBuffer:m_instanceBuffer offset:0 atIndex:0];
    [encoder setVertexBuffer:m_uniformBuffer offset:0 atIndex:1];

    // Draw instanced quads
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                vertexStart:0
                vertexCount:6
              instanceCount:totalRectangles];
}

// =============================================================================
// Screen Size Updates
// =============================================================================

void RectangleManager::updateScreenSize(int width, int height) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_screenWidth = width;
    m_screenHeight = height;
    updateUniforms();
}

} // namespace SuperTerminal
