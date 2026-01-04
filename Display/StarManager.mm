//
//  StarManager.mm
//  SuperTerminal Framework - GPU-Accelerated Star Rendering
//
//  Copyright © 2024-2025 SuperTerminal. All rights reserved.
//

#include "StarManager.h"
#include <algorithm>
#include <cmath>

namespace SuperTerminal {

// =============================================================================
// Constructor / Destructor
// =============================================================================

StarManager::StarManager()
    : m_device(nil)
    , m_pipelineState(nil)
    , m_instanceBuffer(nil)
    , m_uniformBuffer(nil)
    , m_nextId(1)
    , m_maxStars(10000)
    , m_bufferNeedsUpdate(false)
    , m_screenWidth(1920)
    , m_screenHeight(1080)
{
}

StarManager::~StarManager() {
    // Metal objects are reference counted, they'll clean up automatically
}

// =============================================================================
// Initialization
// =============================================================================

bool StarManager::initialize(id<MTLDevice> device, int screenWidth, int screenHeight) {
    m_device = device;
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;

    // Create the render pipeline
    if (!createPipeline()) {
        NSLog(@"[StarManager] ERROR: Failed to create pipeline");
        return false;
    }

    // Create instance buffer (will be resized as needed)
    m_instanceBuffer = [m_device newBufferWithLength:m_maxStars * sizeof(StarInstance)
                                             options:MTLResourceStorageModeShared];
    if (!m_instanceBuffer) {
        NSLog(@"[StarManager] ERROR: Failed to create instance buffer");
        return false;
    }

    // Create uniform buffer
    m_uniformBuffer = [m_device newBufferWithLength:sizeof(StarUniforms)
                                            options:MTLResourceStorageModeShared];
    if (!m_uniformBuffer) {
        NSLog(@"[StarManager] ERROR: Failed to create uniform buffer");
        return false;
    }

    // Initialize uniforms
    updateUniforms();

    NSLog(@"[StarManager] Initialized successfully (max stars: %zu, screen: %dx%d)",
          m_maxStars, m_screenWidth, m_screenHeight);
    return true;
}

bool StarManager::createPipeline() {
    NSError* error = nil;

    // Compile shaders from inline source
    NSString* shaderSource = @R"(
        #include <metal_stdlib>
        using namespace metal;

        struct StarInstance {
            float2 position;
            float outerRadius;
            float innerRadius;
            int numPoints;
            uint color1;
            uint color2;
            uint mode;
            float rotation;
            float param1;
            float param2;
        };

        struct StarUniforms {
            float screenWidth;
            float screenHeight;
            float2 padding;
        };

        struct StarVertexOut {
            float4 position [[position]];
            float2 localCoord;
            float outerRadius;
            float innerRadius;
            int numPoints;
            uint color1;
            uint color2;
            uint mode;
            float rotation;
            float param1;
        };

        float4 unpackColor(uint rgba) {
            // Color format is 0xRRGGBBAA
            float r = float((rgba >> 24) & 0xFF) / 255.0;
            float g = float((rgba >> 16) & 0xFF) / 255.0;
            float b = float((rgba >> 8) & 0xFF) / 255.0;
            float a = float((rgba >> 0) & 0xFF) / 255.0;
            return float4(r, g, b, a);
        }

        float2 rotate2D(float2 p, float angle) {
            float s = sin(angle);
            float c = cos(angle);
            return float2(p.x * c - p.y * s, p.x * s + p.y * c);
        }

        // Calculate star vertex positions
        float2 getStarVertex(int vertexIndex, int numPoints, float outerRadius, float innerRadius) {
            // Each point has 2 vertices: outer tip and inner valley
            // Total vertices = numPoints * 2
            // vertexIndex pattern: 0=outer, 1=inner, 2=outer, 3=inner, etc.
            bool isOuter = (vertexIndex % 2) == 0;

            // Calculate angle: outer points are at even indices, inner points at odd
            // Inner points should be exactly halfway between outer points
            float angleStep = (2.0 * M_PI_F / float(numPoints));
            float angle = float(vertexIndex / 2) * angleStep - M_PI_F * 0.5;

            if (!isOuter) {
                // Inner vertex is halfway between current and next outer vertex
                angle += angleStep * 0.5;
            }

            float radius = isOuter ? outerRadius : innerRadius;
            return float2(cos(angle) * radius, sin(angle) * radius);
        }

        // Simple triangle test - point is inside if all cross products have same sign
        bool isInsideTriangle(float2 p, float2 v0, float2 v1, float2 v2) {
            float2 e0 = v1 - v0;
            float2 e1 = v2 - v1;
            float2 e2 = v0 - v2;

            float2 p0 = p - v0;
            float2 p1 = p - v1;
            float2 p2 = p - v2;

            float c0 = e0.x * p0.y - e0.y * p0.x;
            float c1 = e1.x * p1.y - e1.y * p1.x;
            float c2 = e2.x * p2.y - e2.y * p2.x;

            return (c0 >= 0.0 && c1 >= 0.0 && c2 >= 0.0) || (c0 <= 0.0 && c1 <= 0.0 && c2 <= 0.0);
        }

        // Distance from point to line segment
        float distanceToSegment(float2 p, float2 a, float2 b) {
            float2 pa = p - a;
            float2 ba = b - a;
            float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
            return length(pa - ba * h);
        }

        // Star of David: two overlapping triangles
        // For n-pointed stars: draw n/2 triangles rotated around center
        bool isInsideStar(float2 p, int numPoints, float radius) {
            // Simple Star of David with two equilateral triangles
            float r = radius * 0.8; // Scale down slightly so it fits in the circle

            // Triangle 1: pointing UP (^)
            float2 t1_v0 = float2(0.0, r);              // top point
            float2 t1_v1 = float2(-r * 0.866, -r * 0.5); // bottom-left
            float2 t1_v2 = float2(r * 0.866, -r * 0.5);  // bottom-right

            // Triangle 2: pointing DOWN (v)
            float2 t2_v0 = float2(0.0, -r);             // bottom point
            float2 t2_v1 = float2(-r * 0.866, r * 0.5); // top-left
            float2 t2_v2 = float2(r * 0.866, r * 0.5);  // top-right

            return isInsideTriangle(p, t1_v0, t1_v1, t1_v2) ||
                   isInsideTriangle(p, t2_v0, t2_v1, t2_v2);
        }

        // Distance to star edge (simplified circular approximation)
        // Use distance from center to avoid showing internal seams
        float distanceToStarEdge(float2 p, int numPoints, float radius) {
            // Simple circular boundary - avoids showing triangle seams
            float r = radius * 0.8;
            float currentDist = length(p);

            // Return how far inside the boundary we are
            // Large value = deep inside, small value = near edge
            return r - currentDist;
        }

        vertex StarVertexOut star_vertex(
            uint vertexID [[vertex_id]],
            uint instanceID [[instance_id]],
            const device StarInstance* instances [[buffer(0)]],
            constant StarUniforms& uniforms [[buffer(1)]]
        ) {
            StarVertexOut out;
            StarInstance star = instances[instanceID];

            float2 quadVertices[4] = {
                float2(-1.0, -1.0),
                float2( 1.0, -1.0),
                float2(-1.0,  1.0),
                float2( 1.0,  1.0)
            };

            float2 quadVertex = quadVertices[vertexID];

            // Scale quad to cover the star plus a small margin for antialiasing
            float margin = 2.0;
            float2 scaledVertex = quadVertex * (star.outerRadius + margin);

            if (star.rotation != 0.0) {
                scaledVertex = rotate2D(scaledVertex, star.rotation);
            }

            float2 worldPos = star.position + scaledVertex;
            float2 ndc = float2(
                (worldPos.x / uniforms.screenWidth) * 2.0 - 1.0,
                1.0 - (worldPos.y / uniforms.screenHeight) * 2.0
            );

            out.position = float4(ndc, 0.0, 1.0);
            out.localCoord = quadVertex;
            out.outerRadius = star.outerRadius;
            out.innerRadius = star.innerRadius;
            out.numPoints = star.numPoints;
            out.color1 = star.color1;
            out.color2 = star.color2;
            out.mode = star.mode;
            out.rotation = star.rotation;
            out.param1 = star.param1;

            return out;
        }

        fragment float4 star_fragment(StarVertexOut in [[stage_in]]) {
            // Transform to local coordinates (quad is from -1 to +1)
            float2 p = in.localCoord * in.outerRadius;

            // Apply rotation if needed
            if (in.rotation != 0.0) {
                p = rotate2D(p, -in.rotation);
            }

            // Check if point is inside the star shape
            bool inside = isInsideStar(p, in.numPoints, in.outerRadius);
            if (!inside) {
                discard_fragment();
            }

            // Calculate distance to edge for antialiasing
            float edgeDist = distanceToStarEdge(p, in.numPoints, in.outerRadius);
            float edgeWidth = 1.5;

            // Antialiasing: fade alpha near edges for smooth appearance
            // edgeDist is small near edges, large in center
            // smoothstep maps: edgeDist=0 -> alpha=0, edgeDist>=edgeWidth -> alpha=1
            float alpha = smoothstep(0.0, edgeWidth, edgeDist);

            float4 color1 = unpackColor(in.color1);
            float4 color2 = unpackColor(in.color2);
            float4 finalColor = color1;

            // Apply color mode
            if (in.mode == 0) {
                // Solid color
                finalColor = color1;
            }
            else if (in.mode == 1) {
                // DEBUG: Just render solid green to test mode branching
                finalColor = float4(0.0, 1.0, 0.0, 1.0);
            }
            else if (in.mode == 2) {
                // Alternating colors based on angle
                float angle = atan2(p.y, p.x);
                float normalizedAngle = angle;
                if (normalizedAngle < 0.0) normalizedAngle += 2.0 * M_PI_F;
                float anglePerPoint = (2.0 * M_PI_F) / float(in.numPoints);
                int pointIndex = int(floor(normalizedAngle / anglePerPoint));
                finalColor = (pointIndex % 2 == 0) ? color1 : color2;
            }
            else if (in.mode == 100) {
                // Outline mode
                float lineWidth = in.param1;
                if (edgeDist < lineWidth) {
                    finalColor = color2;
                } else {
                    finalColor = color1;
                }
            }

            // Apply antialiasing alpha
            finalColor.a *= alpha;

            // Premultiply alpha
            finalColor.rgb *= finalColor.a;

            return finalColor;
        }
    )";

    id<MTLLibrary> library = [m_device newLibraryWithSource:shaderSource
                                                     options:nil
                                                       error:&error];
    if (!library) {
        NSLog(@"[StarManager] ERROR: Failed to compile shaders: %@", error);
        return false;
    }

    id<MTLFunction> vertexFunction = [library newFunctionWithName:@"star_vertex"];
    id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"star_fragment"];

    if (!vertexFunction || !fragmentFunction) {
        NSLog(@"[StarManager] ERROR: Failed to load shader functions");
        return false;
    }

    // Create pipeline descriptor
    MTLRenderPipelineDescriptor* pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDescriptor.vertexFunction = vertexFunction;
    pipelineDescriptor.fragmentFunction = fragmentFunction;
    pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

    // Enable blending for transparency
    pipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
    pipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    pipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
    pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
    pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

    // Create pipeline state
    m_pipelineState = [m_device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
    if (!m_pipelineState) {
        NSLog(@"[StarManager] ERROR: Failed to create pipeline state: %@", error);
        return false;
    }

    return true;
}

// =============================================================================
// Star Creation
// =============================================================================

int StarManager::createStar(float x, float y, float outerRadius, int numPoints, uint32_t color) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedStars.size() >= m_maxStars) {
        NSLog(@"[StarManager] WARNING: Maximum star count reached (%zu)", m_maxStars);
        return 0;
    }

    // Clamp numPoints to valid range
    numPoints = std::max(3, std::min(12, numPoints));

    // Calculate default inner radius (golden ratio for 5-point star)
    float innerRadius = outerRadius * 0.382f;

    int id = m_nextId++;
    ManagedStar& star = m_managedStars[id];
    star.data.x = x;
    star.data.y = y;
    star.data.outerRadius = outerRadius;
    star.data.innerRadius = innerRadius;
    star.data.color1 = color;
    star.data.color2 = color;
    star.data.mode = static_cast<uint32_t>(StarGradientMode::SOLID);
    star.data.numPoints = numPoints;
    star.data.rotation = 0.0f;
    star.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

int StarManager::createCustomStar(float x, float y, float outerRadius, float innerRadius,
                                  int numPoints, uint32_t color) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedStars.size() >= m_maxStars) {
        NSLog(@"[StarManager] WARNING: Maximum star count reached (%zu)", m_maxStars);
        return 0;
    }

    // Clamp numPoints to valid range
    numPoints = std::max(3, std::min(12, numPoints));

    int id = m_nextId++;
    ManagedStar& star = m_managedStars[id];
    star.data.x = x;
    star.data.y = y;
    star.data.outerRadius = outerRadius;
    star.data.innerRadius = innerRadius;
    star.data.color1 = color;
    star.data.color2 = color;
    star.data.mode = static_cast<uint32_t>(StarGradientMode::SOLID);
    star.data.numPoints = numPoints;
    star.data.rotation = 0.0f;
    star.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

int StarManager::createGradient(float x, float y, float outerRadius, int numPoints,
                                uint32_t color1, uint32_t color2,
                                StarGradientMode mode) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedStars.size() >= m_maxStars) {
        NSLog(@"[StarManager] WARNING: Maximum star count reached (%zu)", m_maxStars);
        return 0;
    }

    // Clamp numPoints to valid range
    numPoints = std::max(3, std::min(12, numPoints));

    // Calculate default inner radius
    float innerRadius = outerRadius * 0.382f;

    int id = m_nextId++;
    ManagedStar& star = m_managedStars[id];
    star.data.x = x;
    star.data.y = y;
    star.data.outerRadius = outerRadius;
    star.data.innerRadius = innerRadius;
    star.data.color1 = color1;
    star.data.color2 = color2;
    star.data.mode = static_cast<uint32_t>(mode);
    star.data.numPoints = numPoints;
    star.data.rotation = 0.0f;
    star.visible = true;

    NSLog(@"[StarManager] createGradient: id=%d, mode=%u, color1=0x%08X, color2=0x%08X",
          id, star.data.mode, color1, color2);

    m_bufferNeedsUpdate = true;
    return id;
}

int StarManager::createOutline(float x, float y, float outerRadius, int numPoints,
                               uint32_t fillColor, uint32_t outlineColor,
                               float lineWidth) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedStars.size() >= m_maxStars) {
        NSLog(@"[StarManager] WARNING: Maximum star count reached (%zu)", m_maxStars);
        return 0;
    }

    // Clamp numPoints to valid range
    numPoints = std::max(3, std::min(12, numPoints));

    // Calculate default inner radius
    float innerRadius = outerRadius * 0.382f;

    int id = m_nextId++;
    ManagedStar& star = m_managedStars[id];
    star.data.x = x;
    star.data.y = y;
    star.data.outerRadius = outerRadius;
    star.data.innerRadius = innerRadius;
    star.data.color1 = fillColor;
    star.data.color2 = outlineColor;
    star.data.mode = static_cast<uint32_t>(StarGradientMode::OUTLINE);
    star.data.numPoints = numPoints;
    star.data.rotation = 0.0f;
    star.data.param1 = lineWidth;
    star.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

// =============================================================================
// Star Updates
// =============================================================================

bool StarManager::setPosition(int id, float x, float y) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedStars.find(id);
    if (it == m_managedStars.end()) return false;

    it->second.data.x = x;
    it->second.data.y = y;
    m_bufferNeedsUpdate = true;
    return true;
}

bool StarManager::setRadius(int id, float outerRadius) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedStars.find(id);
    if (it == m_managedStars.end()) return false;

    it->second.data.outerRadius = outerRadius;
    // Maintain ratio for inner radius
    it->second.data.innerRadius = outerRadius * 0.382f;
    m_bufferNeedsUpdate = true;
    return true;
}

bool StarManager::setRadii(int id, float outerRadius, float innerRadius) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedStars.find(id);
    if (it == m_managedStars.end()) return false;

    it->second.data.outerRadius = outerRadius;
    it->second.data.innerRadius = innerRadius;
    m_bufferNeedsUpdate = true;
    return true;
}

bool StarManager::setPoints(int id, int numPoints) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedStars.find(id);
    if (it == m_managedStars.end()) return false;

    // Clamp to valid range
    numPoints = std::max(3, std::min(12, numPoints));
    it->second.data.numPoints = numPoints;
    m_bufferNeedsUpdate = true;
    return true;
}

bool StarManager::setColor(int id, uint32_t color) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedStars.find(id);
    if (it == m_managedStars.end()) return false;

    it->second.data.color1 = color;
    it->second.data.color2 = color;
    m_bufferNeedsUpdate = true;
    return true;
}

bool StarManager::setColors(int id, uint32_t color1, uint32_t color2) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedStars.find(id);
    if (it == m_managedStars.end()) return false;

    it->second.data.color1 = color1;
    it->second.data.color2 = color2;
    m_bufferNeedsUpdate = true;
    return true;
}

bool StarManager::setRotation(int id, float angleDegrees) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedStars.find(id);
    if (it == m_managedStars.end()) return false;

    // Convert degrees to radians
    it->second.data.rotation = angleDegrees * M_PI / 180.0f;
    m_bufferNeedsUpdate = true;
    return true;
}

bool StarManager::setVisible(int id, bool visible) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedStars.find(id);
    if (it == m_managedStars.end()) return false;

    bool changed = (it->second.visible != visible);
    it->second.visible = visible;
    if (changed) {
        m_bufferNeedsUpdate = true;
    }
    return true;
}

// =============================================================================
// Query and Management
// =============================================================================

bool StarManager::exists(int id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_managedStars.find(id) != m_managedStars.end();
}

bool StarManager::isVisible(int id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_managedStars.find(id);
    if (it == m_managedStars.end()) return false;
    return it->second.visible;
}

bool StarManager::deleteStar(int id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t removed = m_managedStars.erase(id);
    if (removed > 0) {
        m_bufferNeedsUpdate = true;
        return true;
    }
    return false;
}

void StarManager::deleteAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_managedStars.empty()) {
        m_managedStars.clear();
        m_bufferNeedsUpdate = true;
    }
}

size_t StarManager::getStarCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t count = 0;
    for (const auto& pair : m_managedStars) {
        if (pair.second.visible) count++;
    }
    return count;
}

bool StarManager::isEmpty() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_managedStars.empty();
}

void StarManager::setMaxStars(size_t max) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxStars = max;
}

// =============================================================================
// Rendering
// =============================================================================

void StarManager::updateInstanceBuffer() {
    if (!m_bufferNeedsUpdate) return;

    // Collect visible stars
    std::vector<StarInstance> visibleStars;
    visibleStars.reserve(m_managedStars.size());

    for (const auto& pair : m_managedStars) {
        if (pair.second.visible) {
            visibleStars.push_back(pair.second.data);
        }
    }

    // Update buffer if we have stars
    if (!visibleStars.empty()) {
        size_t requiredSize = visibleStars.size() * sizeof(StarInstance);
        if (m_instanceBuffer.length < requiredSize) {
            // Reallocate buffer if needed
            m_instanceBuffer = [m_device newBufferWithLength:requiredSize * 2
                                                     options:MTLResourceStorageModeShared];
        }

        // Copy data to buffer
        memcpy(m_instanceBuffer.contents, visibleStars.data(), requiredSize);
    }

    m_bufferNeedsUpdate = false;
}

void StarManager::updateUniforms() {
    StarUniforms* uniforms = (StarUniforms*)m_uniformBuffer.contents;
    uniforms->screenWidth = static_cast<float>(m_screenWidth);
    uniforms->screenHeight = static_cast<float>(m_screenHeight);
}

void StarManager::render(id<MTLRenderCommandEncoder> encoder) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Update buffer if needed
    updateInstanceBuffer();

    // Count visible stars
    size_t visibleCount = 0;
    for (const auto& pair : m_managedStars) {
        if (pair.second.visible) visibleCount++;
    }

    if (visibleCount == 0) return;

    // Set pipeline state
    [encoder setRenderPipelineState:m_pipelineState];

    // Bind buffers
    [encoder setVertexBuffer:m_instanceBuffer offset:0 atIndex:0];
    [encoder setVertexBuffer:m_uniformBuffer offset:0 atIndex:1];
    [encoder setFragmentBuffer:m_instanceBuffer offset:0 atIndex:0];
    [encoder setFragmentBuffer:m_uniformBuffer offset:0 atIndex:1];

    // Draw instanced triangle strips
    // For each star: we need 2*numPoints + 2 vertices (center + perimeter + closing vertex)
    // However, since each star can have different numPoints, we need to find the max
    // and draw that many vertices per instance (unused vertices will be clipped)

    // Find maximum numPoints across all visible stars
    uint32_t maxPoints = 5;  // Default
    for (const auto& pair : m_managedStars) {
        if (pair.second.visible && pair.second.data.numPoints > maxPoints) {
            maxPoints = pair.second.data.numPoints;
        }
    }

    // Calculate vertex count: 1 (center) + 2*n (perimeter) + 1 (closing)
    uint32_t vertexCount = 1 + (2 * maxPoints) + 1;

    // Draw as triangle strip: center connects to each perimeter vertex in sequence
    [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
                vertexStart:0
                vertexCount:vertexCount
              instanceCount:visibleCount];
}

void StarManager::updateScreenSize(int width, int height) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_screenWidth = width;
    m_screenHeight = height;
    updateUniforms();
}

} // namespace SuperTerminal
