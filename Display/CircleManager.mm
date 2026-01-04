//
//  CircleManager.mm
//  SuperTerminal Framework - GPU-Accelerated Circle Rendering
//
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "CircleManager.h"
#include <algorithm>

namespace SuperTerminal {

// =============================================================================
// Constructor / Destructor
// =============================================================================

CircleManager::CircleManager()
    : m_device(nil)
    , m_pipelineState(nil)
    , m_instanceBuffer(nil)
    , m_uniformBuffer(nil)
    , m_nextId(1)
    , m_maxCircles(10000)
    , m_bufferNeedsUpdate(false)
    , m_screenWidth(1920)
    , m_screenHeight(1080)
{
}

CircleManager::~CircleManager() {
    // Metal objects are reference counted, they'll clean up automatically
}

// =============================================================================
// Initialization
// =============================================================================

bool CircleManager::initialize(id<MTLDevice> device, int screenWidth, int screenHeight) {
    m_device = device;
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;

    // Create the render pipeline
    if (!createPipeline()) {
        NSLog(@"[CircleManager] ERROR: Failed to create pipeline");
        return false;
    }

    // Create instance buffer (will be resized as needed)
    m_instanceBuffer = [m_device newBufferWithLength:m_maxCircles * sizeof(CircleInstance)
                                             options:MTLResourceStorageModeShared];
    if (!m_instanceBuffer) {
        NSLog(@"[CircleManager] ERROR: Failed to create instance buffer");
        return false;
    }

    // Create uniform buffer
    m_uniformBuffer = [m_device newBufferWithLength:sizeof(CircleUniforms)
                                            options:MTLResourceStorageModeShared];
    if (!m_uniformBuffer) {
        NSLog(@"[CircleManager] ERROR: Failed to create uniform buffer");
        return false;
    }

    // Initialize uniforms
    updateUniforms();

    NSLog(@"[CircleManager] Initialized successfully (max circles: %zu, screen: %dx%d)",
          m_maxCircles, m_screenWidth, m_screenHeight);
    return true;
}

bool CircleManager::createPipeline() {
    NSError* error = nil;

    // Compile shaders from inline source
    NSString* shaderSource = @R"(
        #include <metal_stdlib>
        using namespace metal;

        // Circle instance data (matches C++ CircleInstance)
        struct CircleInstance {
            float x;
            float y;
            float radius;
            float padding1;
            uint color1;
            uint color2;
            uint color3;
            uint color4;
            uint mode;
            float param1;
            float param2;
            float param3;
        };

        // Uniform data
        struct CircleUniforms {
            float screenWidth;
            float screenHeight;
            float2 padding;
        };

        // Vertex output / Fragment input
        struct VertexOut {
            float4 position [[position]];
            float2 center;
            float radius;
            float2 fragCoord;
            uint mode;
            float4 color1;
            float4 color2;
            float4 color3;
            float4 color4;
            float3 params;
        };

        // Gradient modes (must match C++ enum)
        constant uint CIRCLE_SOLID = 0;
        constant uint CIRCLE_RADIAL = 1;
        constant uint CIRCLE_RADIAL_3 = 2;
        constant uint CIRCLE_RADIAL_4 = 3;
        constant uint CIRCLE_OUTLINE = 100;
        constant uint CIRCLE_DASHED_OUTLINE = 101;
        constant uint CIRCLE_RING = 102;
        constant uint CIRCLE_PIE = 103;
        constant uint CIRCLE_ARC = 104;
        constant uint CIRCLE_DOTS_RING = 105;
        constant uint CIRCLE_STAR_BURST = 106;

        // Unpack RGBA8888 to float4
        float4 unpackColor(uint packedColor) {
            float r = float((packedColor >> 24) & 0xFF) / 255.0;
            float g = float((packedColor >> 16) & 0xFF) / 255.0;
            float b = float((packedColor >> 8) & 0xFF) / 255.0;
            float a = float(packedColor & 0xFF) / 255.0;
            return float4(r, g, b, a);
        }

        // Vertex shader
        vertex VertexOut circle_vertex(
            uint vertexID [[vertex_id]],
            uint instanceID [[instance_id]],
            constant CircleInstance *instances [[buffer(0)]],
            constant CircleUniforms &uniforms [[buffer(1)]]
        ) {
            CircleInstance circle = instances[instanceID];

            // Generate quad vertices (2 triangles = 6 vertices)
            float2 positions[6] = {
                float2(-1, -1),  // Top-left
                float2( 1,  1),  // Bottom-right
                float2( 1, -1),  // Top-right
                float2(-1, -1),  // Top-left
                float2(-1,  1),  // Bottom-left
                float2( 1,  1)   // Bottom-right
            };

            float2 offset = positions[vertexID];

            // Calculate vertex position in pixels
            float2 pixelPos = float2(circle.x, circle.y) + offset * (circle.radius + 2.0);

            // Transform to clip space
            float x = (pixelPos.x / uniforms.screenWidth) * 2.0 - 1.0;
            float y = 1.0 - (pixelPos.y / uniforms.screenHeight) * 2.0;

            VertexOut out;
            out.position = float4(x, y, 0.0, 1.0);
            out.center = float2(circle.x, circle.y);
            out.radius = circle.radius;
            out.fragCoord = pixelPos;
            out.mode = circle.mode;
            out.color1 = unpackColor(circle.color1);
            out.color2 = unpackColor(circle.color2);
            out.color3 = unpackColor(circle.color3);
            out.color4 = unpackColor(circle.color4);
            out.params = float3(circle.param1, circle.param2, circle.param3);

            return out;
        }

        // Fragment shader
        fragment float4 circle_fragment(VertexOut in [[stage_in]]) {
            float2 diff = in.fragCoord - in.center;
            float dist = length(diff);

            float4 finalColor = in.color1;

            // Solid color
            if (in.mode == CIRCLE_SOLID) {
                finalColor = in.color1;
            }
            // Radial gradient (2 colors)
            else if (in.mode == CIRCLE_RADIAL) {
                float t = clamp(dist / in.radius, 0.0, 1.0);
                finalColor = mix(in.color1, in.color2, t);
            }
            // Radial gradient (3 colors)
            else if (in.mode == CIRCLE_RADIAL_3) {
                float t = clamp(dist / in.radius, 0.0, 1.0);
                if (t < 0.5) {
                    finalColor = mix(in.color1, in.color2, t * 2.0);
                } else {
                    finalColor = mix(in.color2, in.color3, (t - 0.5) * 2.0);
                }
            }
            // Radial gradient (4 colors)
            else if (in.mode == CIRCLE_RADIAL_4) {
                float t = clamp(dist / in.radius, 0.0, 1.0);
                if (t < 0.33) {
                    finalColor = mix(in.color1, in.color2, t * 3.0);
                } else if (t < 0.66) {
                    finalColor = mix(in.color2, in.color3, (t - 0.33) * 3.0);
                } else {
                    finalColor = mix(in.color3, in.color4, (t - 0.66) * 3.0);
                }
            }
            // Outline
            else if (in.mode == CIRCLE_OUTLINE) {
                float lineWidth = in.params.x;
                float innerRadius = in.radius - lineWidth;
                if (dist < innerRadius) {
                    finalColor = in.color1;  // Fill
                } else {
                    finalColor = in.color2;  // Outline
                }
            }
            // Dashed outline
            else if (in.mode == CIRCLE_DASHED_OUTLINE) {
                float lineWidth = in.params.x;
                float dashLength = in.params.y;
                float innerRadius = in.radius - lineWidth;

                float angle = atan2(diff.y, diff.x);
                float circumference = 2.0 * 3.14159265359 * in.radius;
                float arcLength = angle * in.radius;
                float dashCycle = fmod(abs(arcLength), dashLength * 2.0);

                if (dist < innerRadius) {
                    finalColor = in.color1;  // Fill
                } else if (dashCycle < dashLength) {
                    finalColor = in.color2;  // Dash
                } else {
                    discard_fragment();
                }
            }
            // Ring
            else if (in.mode == CIRCLE_RING) {
                float innerRadius = in.params.x;
                if (dist < innerRadius || dist > in.radius) {
                    discard_fragment();
                }
                finalColor = in.color1;
            }
            // Pie slice
            else if (in.mode == CIRCLE_PIE) {
                float startAngle = in.params.x;
                float endAngle = in.params.y;
                float angle = atan2(diff.y, diff.x);

                // Normalize angles
                if (angle < startAngle || angle > endAngle) {
                    discard_fragment();
                }
                finalColor = in.color1;
            }
            // Arc
            else if (in.mode == CIRCLE_ARC) {
                float startAngle = in.params.x;
                float endAngle = in.params.y;
                float lineWidth = in.params.z;
                float angle = atan2(diff.y, diff.x);

                float innerRadius = in.radius - lineWidth * 0.5;
                float outerRadius = in.radius + lineWidth * 0.5;

                if (dist < innerRadius || dist > outerRadius) {
                    discard_fragment();
                }
                if (angle < startAngle || angle > endAngle) {
                    discard_fragment();
                }
                finalColor = in.color1;
            }
            // Dots ring
            else if (in.mode == CIRCLE_DOTS_RING) {
                float dotRadius = in.params.x;
                int numDots = int(in.params.y);

                float angle = atan2(diff.y, diff.x);
                float angleStep = 2.0 * 3.14159265359 / float(numDots);

                bool onDot = false;
                for (int i = 0; i < numDots; i++) {
                    float dotAngle = float(i) * angleStep;
                    float2 dotPos = in.center + float2(cos(dotAngle), sin(dotAngle)) * in.radius;
                    float dotDist = length(in.fragCoord - dotPos);
                    if (dotDist < dotRadius) {
                        onDot = true;
                        break;
                    }
                }

                if (onDot) {
                    finalColor = in.color1;
                } else {
                    finalColor = in.color2;
                }
            }
            // Star burst
            else if (in.mode == CIRCLE_STAR_BURST) {
                int numRays = int(in.params.x);
                float angle = atan2(diff.y, diff.x);
                float angleStep = 2.0 * 3.14159265359 / float(numRays);
                float rayAngle = fmod(angle + 3.14159265359, angleStep);

                if (rayAngle < angleStep * 0.5) {
                    finalColor = in.color1;
                } else {
                    finalColor = in.color2;
                }
            }

            // Apply smooth anti-aliasing at circle edge
            float edgeSoftness = 1.0;
            float alpha = 1.0 - smoothstep(in.radius - edgeSoftness, in.radius + edgeSoftness, dist);

            return float4(finalColor.rgb, finalColor.a * alpha);
        }
    )";

    id<MTLLibrary> library = [m_device newLibraryWithSource:shaderSource
                                                     options:nil
                                                       error:&error];
    if (!library) {
        NSLog(@"[CircleManager] ERROR: Failed to create shader library: %@", error);
        return false;
    }

    id<MTLFunction> vertexFunction = [library newFunctionWithName:@"circle_vertex"];
    id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"circle_fragment"];

    if (!vertexFunction || !fragmentFunction) {
        NSLog(@"[CircleManager] ERROR: Failed to find shader functions");
        return false;
    }

    // Create pipeline descriptor
    MTLRenderPipelineDescriptor* pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDescriptor.vertexFunction = vertexFunction;
    pipelineDescriptor.fragmentFunction = fragmentFunction;
    pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

    // Enable blending for transparency
    pipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
    pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
    pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;

    m_pipelineState = [m_device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
    if (!m_pipelineState) {
        NSLog(@"[CircleManager] ERROR: Failed to create pipeline state: %@", error);
        return false;
    }

    return true;
}

// =============================================================================
// Circle Creation
// =============================================================================

int CircleManager::createCircle(float x, float y, float radius, uint32_t color) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedCircles.size() >= m_maxCircles) {
        NSLog(@"[CircleManager] ERROR: Maximum circle count (%zu) reached", m_maxCircles);
        return -1;
    }

    int id = m_nextId++;
    ManagedCircle& mc = m_managedCircles[id];
    mc.data.x = x;
    mc.data.y = y;
    mc.data.radius = radius;
    mc.data.color1 = color;
    mc.data.mode = static_cast<uint32_t>(CircleGradientMode::SOLID);
    mc.visible = true;

    m_bufferNeedsUpdate = true;

    NSLog(@"[CircleManager] Created circle ID=%d at (%.1f, %.1f) radius=%.1f color=0x%08X",
          id, x, y, radius, color);

    return id;
}

int CircleManager::createRadialGradient(float x, float y, float radius,
                                        uint32_t centerColor, uint32_t edgeColor) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedCircles.size() >= m_maxCircles) {
        return -1;
    }

    int id = m_nextId++;
    ManagedCircle& mc = m_managedCircles[id];
    mc.data.x = x;
    mc.data.y = y;
    mc.data.radius = radius;
    mc.data.color1 = centerColor;
    mc.data.color2 = edgeColor;
    mc.data.mode = static_cast<uint32_t>(CircleGradientMode::RADIAL);
    mc.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

int CircleManager::createRadialGradient3(float x, float y, float radius,
                                         uint32_t color1, uint32_t color2, uint32_t color3) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedCircles.size() >= m_maxCircles) {
        return -1;
    }

    int id = m_nextId++;
    ManagedCircle& mc = m_managedCircles[id];
    mc.data.x = x;
    mc.data.y = y;
    mc.data.radius = radius;
    mc.data.color1 = color1;
    mc.data.color2 = color2;
    mc.data.color3 = color3;
    mc.data.mode = static_cast<uint32_t>(CircleGradientMode::RADIAL_3);
    mc.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

int CircleManager::createRadialGradient4(float x, float y, float radius,
                                         uint32_t color1, uint32_t color2,
                                         uint32_t color3, uint32_t color4) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedCircles.size() >= m_maxCircles) {
        return -1;
    }

    int id = m_nextId++;
    ManagedCircle& mc = m_managedCircles[id];
    mc.data.x = x;
    mc.data.y = y;
    mc.data.radius = radius;
    mc.data.color1 = color1;
    mc.data.color2 = color2;
    mc.data.color3 = color3;
    mc.data.color4 = color4;
    mc.data.mode = static_cast<uint32_t>(CircleGradientMode::RADIAL_4);
    mc.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

int CircleManager::createOutline(float x, float y, float radius,
                                  uint32_t fillColor, uint32_t outlineColor, float lineWidth) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedCircles.size() >= m_maxCircles) {
        return -1;
    }

    int id = m_nextId++;
    ManagedCircle& mc = m_managedCircles[id];
    mc.data.x = x;
    mc.data.y = y;
    mc.data.radius = radius;
    mc.data.color1 = fillColor;
    mc.data.color2 = outlineColor;
    mc.data.mode = static_cast<uint32_t>(CircleGradientMode::OUTLINE);
    mc.data.param1 = lineWidth;
    mc.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

int CircleManager::createDashedOutline(float x, float y, float radius,
                                        uint32_t fillColor, uint32_t outlineColor,
                                        float lineWidth, float dashLength) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedCircles.size() >= m_maxCircles) {
        return -1;
    }

    int id = m_nextId++;
    ManagedCircle& mc = m_managedCircles[id];
    mc.data.x = x;
    mc.data.y = y;
    mc.data.radius = radius;
    mc.data.color1 = fillColor;
    mc.data.color2 = outlineColor;
    mc.data.mode = static_cast<uint32_t>(CircleGradientMode::DASHED_OUTLINE);
    mc.data.param1 = lineWidth;
    mc.data.param2 = dashLength;
    mc.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

int CircleManager::createRing(float x, float y, float outerRadius, float innerRadius,
                               uint32_t color) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedCircles.size() >= m_maxCircles) {
        return -1;
    }

    int id = m_nextId++;
    ManagedCircle& mc = m_managedCircles[id];
    mc.data.x = x;
    mc.data.y = y;
    mc.data.radius = outerRadius;
    mc.data.color1 = color;
    mc.data.mode = static_cast<uint32_t>(CircleGradientMode::RING);
    mc.data.param1 = innerRadius;
    mc.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

int CircleManager::createPieSlice(float x, float y, float radius,
                                   float startAngle, float endAngle, uint32_t color) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedCircles.size() >= m_maxCircles) {
        return -1;
    }

    int id = m_nextId++;
    ManagedCircle& mc = m_managedCircles[id];
    mc.data.x = x;
    mc.data.y = y;
    mc.data.radius = radius;
    mc.data.color1 = color;
    mc.data.mode = static_cast<uint32_t>(CircleGradientMode::PIE);
    mc.data.param1 = startAngle;
    mc.data.param2 = endAngle;
    mc.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

int CircleManager::createArc(float x, float y, float radius,
                              float startAngle, float endAngle,
                              uint32_t color, float lineWidth) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedCircles.size() >= m_maxCircles) {
        return -1;
    }

    int id = m_nextId++;
    ManagedCircle& mc = m_managedCircles[id];
    mc.data.x = x;
    mc.data.y = y;
    mc.data.radius = radius;
    mc.data.color1 = color;
    mc.data.mode = static_cast<uint32_t>(CircleGradientMode::ARC);
    mc.data.param1 = startAngle;
    mc.data.param2 = endAngle;
    mc.data.param3 = lineWidth;
    mc.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

int CircleManager::createDotsRing(float x, float y, float radius,
                                   uint32_t dotColor, uint32_t backgroundColor,
                                   float dotRadius, int numDots) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedCircles.size() >= m_maxCircles) {
        return -1;
    }

    int id = m_nextId++;
    ManagedCircle& mc = m_managedCircles[id];
    mc.data.x = x;
    mc.data.y = y;
    mc.data.radius = radius;
    mc.data.color1 = dotColor;
    mc.data.color2 = backgroundColor;
    mc.data.mode = static_cast<uint32_t>(CircleGradientMode::DOTS_RING);
    mc.data.param1 = dotRadius;
    mc.data.param2 = static_cast<float>(numDots);
    mc.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

int CircleManager::createStarBurst(float x, float y, float radius,
                                    uint32_t color1, uint32_t color2, int numRays) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedCircles.size() >= m_maxCircles) {
        return -1;
    }

    int id = m_nextId++;
    ManagedCircle& mc = m_managedCircles[id];
    mc.data.x = x;
    mc.data.y = y;
    mc.data.radius = radius;
    mc.data.color1 = color1;
    mc.data.color2 = color2;
    mc.data.mode = static_cast<uint32_t>(CircleGradientMode::STAR_BURST);
    mc.data.param1 = static_cast<float>(numRays);
    mc.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

// =============================================================================
// Circle Updates
// =============================================================================

bool CircleManager::updatePosition(int id, float x, float y) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_managedCircles.find(id);
    if (it == m_managedCircles.end()) {
        return false;
    }

    it->second.data.x = x;
    it->second.data.y = y;
    m_bufferNeedsUpdate = true;
    return true;
}

bool CircleManager::updateRadius(int id, float radius) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_managedCircles.find(id);
    if (it == m_managedCircles.end()) {
        return false;
    }

    it->second.data.radius = radius;
    m_bufferNeedsUpdate = true;
    return true;
}

bool CircleManager::updateColor(int id, uint32_t color) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_managedCircles.find(id);
    if (it == m_managedCircles.end()) {
        return false;
    }

    it->second.data.color1 = color;
    m_bufferNeedsUpdate = true;
    return true;
}

bool CircleManager::updateColors(int id, uint32_t color1, uint32_t color2,
                                  uint32_t color3, uint32_t color4) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_managedCircles.find(id);
    if (it == m_managedCircles.end()) {
        return false;
    }

    it->second.data.color1 = color1;
    it->second.data.color2 = color2;
    it->second.data.color3 = color3;
    it->second.data.color4 = color4;
    m_bufferNeedsUpdate = true;
    return true;
}

bool CircleManager::updateMode(int id, CircleGradientMode mode) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_managedCircles.find(id);
    if (it == m_managedCircles.end()) {
        return false;
    }

    it->second.data.mode = static_cast<uint32_t>(mode);
    m_bufferNeedsUpdate = true;
    return true;
}

bool CircleManager::updateParameters(int id, float param1, float param2, float param3) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_managedCircles.find(id);
    if (it == m_managedCircles.end()) {
        return false;
    }

    it->second.data.param1 = param1;
    it->second.data.param2 = param2;
    it->second.data.param3 = param3;
    m_bufferNeedsUpdate = true;
    return true;
}

bool CircleManager::setVisible(int id, bool visible) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_managedCircles.find(id);
    if (it == m_managedCircles.end()) {
        return false;
    }

    it->second.visible = visible;
    m_bufferNeedsUpdate = true;
    return true;
}

// =============================================================================
// Query
// =============================================================================

bool CircleManager::exists(int id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_managedCircles.find(id) != m_managedCircles.end();
}

bool CircleManager::isVisible(int id) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_managedCircles.find(id);
    if (it == m_managedCircles.end()) {
        return false;
    }

    return it->second.visible;
}

// =============================================================================
// Deletion
// =============================================================================

bool CircleManager::deleteCircle(int id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_managedCircles.find(id);
    if (it == m_managedCircles.end()) {
        return false;
    }

    m_managedCircles.erase(it);
    m_bufferNeedsUpdate = true;
    return true;
}

void CircleManager::deleteAll() {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_managedCircles.clear();
    m_bufferNeedsUpdate = true;
}

// =============================================================================
// Statistics
// =============================================================================

size_t CircleManager::getCircleCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_managedCircles.size();
}

bool CircleManager::isEmpty() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_managedCircles.empty();
}

void CircleManager::setMaxCircles(size_t max) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (max == m_maxCircles) {
        return;
    }

    m_maxCircles = max;

    // Recreate instance buffer with new size
    m_instanceBuffer = [m_device newBufferWithLength:m_maxCircles * sizeof(CircleInstance)
                                             options:MTLResourceStorageModeShared];

    m_bufferNeedsUpdate = true;
}

// =============================================================================
// Rendering
// =============================================================================

void CircleManager::updateInstanceBuffer() {
    if (!m_bufferNeedsUpdate) {
        return;
    }

    if (!m_instanceBuffer) {
        NSLog(@"[CircleManager] ERROR: Instance buffer is nil");
        return;
    }

    void* bufferContents = [m_instanceBuffer contents];
    if (!bufferContents) {
        NSLog(@"[CircleManager] ERROR: Buffer contents is nil");
        return;
    }

    CircleInstance* buffer = static_cast<CircleInstance*>(bufferContents);
    size_t index = 0;

    for (const auto& pair : m_managedCircles) {
        if (pair.second.visible) {
            buffer[index++] = pair.second.data;
        }
    }

    m_bufferNeedsUpdate = false;
}

void CircleManager::updateUniforms() {
    CircleUniforms* uniforms = static_cast<CircleUniforms*>([m_uniformBuffer contents]);
    uniforms->screenWidth = static_cast<float>(m_screenWidth);
    uniforms->screenHeight = static_cast<float>(m_screenHeight);
}

void CircleManager::render(id<MTLRenderCommandEncoder> encoder) {
    std::lock_guard<std::mutex> lock(m_mutex);

    size_t totalCircles = 0;
    for (const auto& pair : m_managedCircles) {
        if (pair.second.visible) totalCircles++;
    }

    static bool firstRender = true;
    if (firstRender && totalCircles > 0) {
        NSLog(@"[CircleManager] First render: %zu circles, pipelineState=%p, buffer=%p",
              totalCircles, m_pipelineState, m_instanceBuffer);
        NSLog(@"[CircleManager] Screen size: %dx%d", m_screenWidth, m_screenHeight);
        firstRender = false;
    }

    if (totalCircles == 0 || !m_pipelineState) {
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
              instanceCount:totalCircles];
}

// =============================================================================
// Screen Size Updates
// =============================================================================

void CircleManager::updateScreenSize(int width, int height) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_screenWidth = width;
    m_screenHeight = height;
    updateUniforms();
}

} // namespace SuperTerminal
