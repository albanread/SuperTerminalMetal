//
//  LineManager.mm
//  SuperTerminal Framework - GPU-Accelerated Line Rendering
//
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "LineManager.h"
#include <algorithm>

namespace SuperTerminal {

// =============================================================================
// Constructor / Destructor
// =============================================================================

LineManager::LineManager()
    : m_device(nil)
    , m_pipelineState(nil)
    , m_instanceBuffer(nil)
    , m_uniformBuffer(nil)
    , m_nextId(1)
    , m_maxLines(10000)
    , m_bufferNeedsUpdate(false)
    , m_screenWidth(1920)
    , m_screenHeight(1080)
{
}

LineManager::~LineManager() {
    // Metal objects are reference counted, they'll clean up automatically
}

// =============================================================================
// Initialization
// =============================================================================

bool LineManager::initialize(id<MTLDevice> device, int screenWidth, int screenHeight) {
    m_device = device;
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;

    // Create the render pipeline
    if (!createPipeline()) {
        NSLog(@"[LineManager] ERROR: Failed to create pipeline");
        return false;
    }

    // Create instance buffer (will be resized as needed)
    m_instanceBuffer = [m_device newBufferWithLength:m_maxLines * sizeof(LineInstance)
                                             options:MTLResourceStorageModeShared];
    if (!m_instanceBuffer) {
        NSLog(@"[LineManager] ERROR: Failed to create instance buffer");
        return false;
    }

    // Create uniform buffer
    m_uniformBuffer = [m_device newBufferWithLength:sizeof(LineUniforms)
                                            options:MTLResourceStorageModeShared];
    if (!m_uniformBuffer) {
        NSLog(@"[LineManager] ERROR: Failed to create uniform buffer");
        return false;
    }

    // Initialize uniforms
    updateUniforms();

    NSLog(@"[LineManager] Initialized successfully (max lines: %zu, screen: %dx%d)",
          m_maxLines, m_screenWidth, m_screenHeight);
    return true;
}

bool LineManager::createPipeline() {
    NSError* error = nil;

    // Compile shaders from inline source
    NSString* shaderSource = @R"(
        #include <metal_stdlib>
        using namespace metal;

        // Line instance data (matches C++ LineInstance)
        struct LineInstance {
            float2 start;
            float2 end;
            float thickness;
            float padding1;
            uint color1;
            uint color2;
            uint mode;
            float dashLength;
            float gapLength;
            float padding2;
        };

        // Uniform data
        struct LineUniforms {
            float screenWidth;
            float screenHeight;
            float2 padding;
        };

        // Vertex output / Fragment input
        struct VertexOut {
            float4 position [[position]];
            float2 start;
            float2 end;
            float thickness;
            float2 fragCoord;
            uint mode;
            float4 color1;
            float4 color2;
            float dashLength;
            float gapLength;
        };

        // Line modes (must match C++ enum)
        constant uint LINE_SOLID = 0;
        constant uint LINE_GRADIENT = 1;
        constant uint LINE_DASHED = 2;
        constant uint LINE_DOTTED = 3;

        // Unpack ARGB8888 to float4
        // Format: 0xAARRGGBB
        float4 unpackColor(uint packedColor) {
            float a = float((packedColor >> 24) & 0xFF) / 255.0;
            float r = float((packedColor >> 16) & 0xFF) / 255.0;
            float g = float((packedColor >> 8) & 0xFF) / 255.0;
            float b = float(packedColor & 0xFF) / 255.0;
            return float4(r, g, b, a);
        }

        // Distance from point to line segment
        float distanceToLineSegment(float2 p, float2 a, float2 b) {
            float2 pa = p - a;
            float2 ba = b - a;
            float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
            return length(pa - ba * h);
        }

        // Vertex shader
        vertex VertexOut line_vertex(
            uint vertexID [[vertex_id]],
            uint instanceID [[instance_id]],
            constant LineInstance *instances [[buffer(0)]],
            constant LineUniforms &uniforms [[buffer(1)]]
        ) {
            LineInstance line = instances[instanceID];

            // Calculate line vector and perpendicular
            float2 lineVec = line.end - line.start;
            float lineLen = length(lineVec);
            float2 lineDir = (lineLen > 0.0) ? lineVec / lineLen : float2(1, 0);
            float2 perpDir = float2(-lineDir.y, lineDir.x);

            // Expand by thickness + anti-alias padding
            float expand = line.thickness * 0.5 + 2.0;

            // Generate quad vertices (2 triangles = 6 vertices)
            // Vertices arranged to encompass the line with padding
            float2 offsets[6];
            offsets[0] = -lineDir * expand - perpDir * expand;  // Top-left
            offsets[1] = lineVec + lineDir * expand + perpDir * expand;  // Bottom-right
            offsets[2] = lineVec + lineDir * expand - perpDir * expand;  // Top-right
            offsets[3] = -lineDir * expand - perpDir * expand;  // Top-left
            offsets[4] = -lineDir * expand + perpDir * expand;  // Bottom-left
            offsets[5] = lineVec + lineDir * expand + perpDir * expand;  // Bottom-right

            float2 offset = offsets[vertexID];

            // Calculate vertex position in pixels
            float2 pixelPos = line.start + offset;

            // Transform to clip space
            float x = (pixelPos.x / uniforms.screenWidth) * 2.0 - 1.0;
            float y = 1.0 - (pixelPos.y / uniforms.screenHeight) * 2.0;

            VertexOut out;
            out.position = float4(x, y, 0.0, 1.0);
            out.start = line.start;
            out.end = line.end;
            out.thickness = line.thickness;
            out.fragCoord = pixelPos;
            out.mode = line.mode;
            out.color1 = unpackColor(line.color1);
            out.color2 = unpackColor(line.color2);
            out.dashLength = line.dashLength;
            out.gapLength = line.gapLength;

            return out;
        }

        // Fragment shader
        fragment float4 line_fragment(VertexOut in [[stage_in]]) {
            // Calculate distance from fragment to line segment
            float dist = distanceToLineSegment(in.fragCoord, in.start, in.end);

            // Calculate half thickness
            float halfThickness = in.thickness * 0.5;

            // Anti-aliased edge (1 pixel gradient)
            float alpha = 1.0 - smoothstep(halfThickness - 0.5, halfThickness + 0.5, dist);

            // Discard fragments outside the line
            if (alpha < 0.01) {
                discard_fragment();
            }

            float4 finalColor = in.color1;

            // Solid color
            if (in.mode == LINE_SOLID) {
                finalColor = in.color1;
            }
            // Gradient from start to end
            else if (in.mode == LINE_GRADIENT) {
                float2 lineVec = in.end - in.start;
                float lineLen = length(lineVec);
                if (lineLen > 0.0) {
                    float2 toFrag = in.fragCoord - in.start;
                    float t = clamp(dot(toFrag, lineVec) / (lineLen * lineLen), 0.0, 1.0);
                    finalColor = mix(in.color1, in.color2, t);
                }
            }
            // Dashed line
            else if (in.mode == LINE_DASHED) {
                float2 lineVec = in.end - in.start;
                float lineLen = length(lineVec);
                if (lineLen > 0.0) {
                    float2 toFrag = in.fragCoord - in.start;
                    float distAlong = dot(toFrag, lineVec) / lineLen;
                    float cycleLen = in.dashLength + in.gapLength;
                    float cyclePos = fmod(distAlong, cycleLen);
                    if (cyclePos > in.dashLength) {
                        discard_fragment();
                    }
                }
                finalColor = in.color1;
            }
            // Dotted line
            else if (in.mode == LINE_DOTTED) {
                float2 lineVec = in.end - in.start;
                float lineLen = length(lineVec);
                if (lineLen > 0.0) {
                    float2 toFrag = in.fragCoord - in.start;
                    float distAlong = dot(toFrag, lineVec) / lineLen;
                    float dotSpacing = in.dashLength;
                    float nearestDot = round(distAlong / dotSpacing) * dotSpacing;
                    float2 dotPos = in.start + normalize(lineVec) * nearestDot;
                    float distToDot = length(in.fragCoord - dotPos);
                    float dotRadius = in.thickness * 0.5;

                    float dotAlpha = 1.0 - smoothstep(dotRadius - 0.5, dotRadius + 0.5, distToDot);
                    if (dotAlpha < 0.01) {
                        discard_fragment();
                    }
                    alpha = dotAlpha;
                }
                finalColor = in.color1;
            }

            // Apply alpha from anti-aliasing
            finalColor.a *= alpha;

            // Round caps at endpoints
            float distToStart = length(in.fragCoord - in.start);
            float distToEnd = length(in.fragCoord - in.end);
            float capRadius = in.thickness * 0.5;

            if (distToStart > capRadius && distToEnd > capRadius) {
                // Check if we're outside the rectangular body of the line
                float2 lineVec = in.end - in.start;
                float lineLen = length(lineVec);
                if (lineLen > 0.0) {
                    float2 toFrag = in.fragCoord - in.start;
                    float t = dot(toFrag, lineVec) / (lineLen * lineLen);
                    if (t < 0.0 || t > 1.0) {
                        discard_fragment();
                    }
                }
            }

            return finalColor;
        }
    )";

    id<MTLLibrary> library = [m_device newLibraryWithSource:shaderSource
                                                     options:nil
                                                       error:&error];
    if (!library) {
        NSLog(@"[LineManager] ERROR: Failed to compile shaders: %@", error);
        return false;
    }

    id<MTLFunction> vertexFunction = [library newFunctionWithName:@"line_vertex"];
    id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"line_fragment"];

    if (!vertexFunction || !fragmentFunction) {
        NSLog(@"[LineManager] ERROR: Failed to find shader functions");
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
    pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

    // Create pipeline state
    m_pipelineState = [m_device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
    if (!m_pipelineState) {
        NSLog(@"[LineManager] ERROR: Failed to create pipeline state: %@", error);
        return false;
    }

    return true;
}

// =============================================================================
// Line Creation
// =============================================================================

int LineManager::createLine(float x1, float y1, float x2, float y2, uint32_t color, float thickness) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedLines.size() >= m_maxLines) {
        NSLog(@"[LineManager] ERROR: Maximum line count (%zu) reached", m_maxLines);
        return -1;
    }

    int id = m_nextId++;
    ManagedLine& ml = m_managedLines[id];
    ml.data.x1 = x1;
    ml.data.y1 = y1;
    ml.data.x2 = x2;
    ml.data.y2 = y2;
    ml.data.thickness = thickness;
    ml.data.color1 = color;
    ml.data.color2 = color;
    ml.data.mode = static_cast<uint32_t>(LineMode::SOLID);
    ml.visible = true;

    m_bufferNeedsUpdate = true;

    NSLog(@"[LineManager] Created line ID=%d from (%.1f, %.1f) to (%.1f, %.1f) thickness=%.1f color=0x%08X",
          id, x1, y1, x2, y2, thickness, color);

    return id;
}

int LineManager::createGradientLine(float x1, float y1, float x2, float y2,
                                   uint32_t color1, uint32_t color2, float thickness) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedLines.size() >= m_maxLines) {
        NSLog(@"[LineManager] ERROR: Maximum line count (%zu) reached", m_maxLines);
        return -1;
    }

    int id = m_nextId++;
    ManagedLine& ml = m_managedLines[id];
    ml.data.x1 = x1;
    ml.data.y1 = y1;
    ml.data.x2 = x2;
    ml.data.y2 = y2;
    ml.data.thickness = thickness;
    ml.data.color1 = color1;
    ml.data.color2 = color2;
    ml.data.mode = static_cast<uint32_t>(LineMode::GRADIENT);
    ml.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

int LineManager::createDashedLine(float x1, float y1, float x2, float y2,
                                 uint32_t color, float thickness,
                                 float dashLength, float gapLength) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedLines.size() >= m_maxLines) {
        NSLog(@"[LineManager] ERROR: Maximum line count (%zu) reached", m_maxLines);
        return -1;
    }

    int id = m_nextId++;
    ManagedLine& ml = m_managedLines[id];
    ml.data.x1 = x1;
    ml.data.y1 = y1;
    ml.data.x2 = x2;
    ml.data.y2 = y2;
    ml.data.thickness = thickness;
    ml.data.color1 = color;
    ml.data.color2 = color;
    ml.data.mode = static_cast<uint32_t>(LineMode::DASHED);
    ml.data.dashLength = dashLength;
    ml.data.gapLength = gapLength;
    ml.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

int LineManager::createDottedLine(float x1, float y1, float x2, float y2,
                                 uint32_t color, float thickness,
                                 float dotSpacing) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_managedLines.size() >= m_maxLines) {
        NSLog(@"[LineManager] ERROR: Maximum line count (%zu) reached", m_maxLines);
        return -1;
    }

    int id = m_nextId++;
    ManagedLine& ml = m_managedLines[id];
    ml.data.x1 = x1;
    ml.data.y1 = y1;
    ml.data.x2 = x2;
    ml.data.y2 = y2;
    ml.data.thickness = thickness;
    ml.data.color1 = color;
    ml.data.color2 = color;
    ml.data.mode = static_cast<uint32_t>(LineMode::DOTTED);
    ml.data.dashLength = dotSpacing;  // Reuse dashLength for dot spacing
    ml.visible = true;

    m_bufferNeedsUpdate = true;
    return id;
}

// =============================================================================
// Line Updates
// =============================================================================

bool LineManager::setEndpoints(int id, float x1, float y1, float x2, float y2) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_managedLines.find(id);
    if (it == m_managedLines.end()) return false;

    it->second.data.x1 = x1;
    it->second.data.y1 = y1;
    it->second.data.x2 = x2;
    it->second.data.y2 = y2;
    m_bufferNeedsUpdate = true;
    return true;
}

bool LineManager::setThickness(int id, float thickness) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_managedLines.find(id);
    if (it == m_managedLines.end()) return false;

    it->second.data.thickness = thickness;
    m_bufferNeedsUpdate = true;
    return true;
}

bool LineManager::setColor(int id, uint32_t color) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_managedLines.find(id);
    if (it == m_managedLines.end()) return false;

    it->second.data.color1 = color;
    it->second.data.color2 = color;
    m_bufferNeedsUpdate = true;
    return true;
}

bool LineManager::setColors(int id, uint32_t color1, uint32_t color2) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_managedLines.find(id);
    if (it == m_managedLines.end()) return false;

    it->second.data.color1 = color1;
    it->second.data.color2 = color2;
    m_bufferNeedsUpdate = true;
    return true;
}

bool LineManager::setDashPattern(int id, float dashLength, float gapLength) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_managedLines.find(id);
    if (it == m_managedLines.end()) return false;

    it->second.data.dashLength = dashLength;
    it->second.data.gapLength = gapLength;
    m_bufferNeedsUpdate = true;
    return true;
}

bool LineManager::setVisible(int id, bool visible) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_managedLines.find(id);
    if (it == m_managedLines.end()) return false;

    it->second.visible = visible;
    m_bufferNeedsUpdate = true;
    return true;
}

// =============================================================================
// Line Queries
// =============================================================================

bool LineManager::exists(int id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_managedLines.find(id) != m_managedLines.end();
}

bool LineManager::isVisible(int id) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_managedLines.find(id);
    if (it == m_managedLines.end()) return false;
    return it->second.visible;
}

size_t LineManager::getLineCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_managedLines.size();
}

bool LineManager::isEmpty() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_managedLines.empty();
}

// =============================================================================
// Line Deletion
// =============================================================================

bool LineManager::deleteLine(int id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_managedLines.find(id);
    if (it == m_managedLines.end()) return false;

    m_managedLines.erase(it);
    m_bufferNeedsUpdate = true;
    return true;
}

void LineManager::deleteAll() {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_managedLines.clear();
    m_bufferNeedsUpdate = true;
}

// =============================================================================
// Configuration
// =============================================================================

void LineManager::updateScreenSize(int width, int height) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_screenWidth = width;
    m_screenHeight = height;
    updateUniforms();
}

void LineManager::setMaxLines(size_t max) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_maxLines = max;

    // Recreate instance buffer with new size
    m_instanceBuffer = [m_device newBufferWithLength:m_maxLines * sizeof(LineInstance)
                                             options:MTLResourceStorageModeShared];

    m_bufferNeedsUpdate = true;
}

// =============================================================================
// Rendering
// =============================================================================

void LineManager::updateInstanceBuffer() {
    if (!m_bufferNeedsUpdate) {
        return;
    }

    if (!m_instanceBuffer) {
        NSLog(@"[LineManager] ERROR: Instance buffer is nil");
        return;
    }

    void* bufferContents = [m_instanceBuffer contents];
    if (!bufferContents) {
        NSLog(@"[LineManager] ERROR: Buffer contents is nil");
        return;
    }

    LineInstance* buffer = static_cast<LineInstance*>(bufferContents);
    size_t index = 0;

    for (const auto& pair : m_managedLines) {
        if (pair.second.visible) {
            const LineInstance& inst = pair.second.data;
            buffer[index] = inst;

            // Debug log first line (disabled)
            // if (index == 0) {
            //     NSLog(@"[LineManager] Buffer update - Line 0: (%.1f,%.1f)->(%.1f,%.1f) thickness=%.1f color1=0x%08X color2=0x%08X mode=%u",
            //           inst.x1, inst.y1, inst.x2, inst.y2, inst.thickness, inst.color1, inst.color2, inst.mode);
            // }

            index++;
        }
    }

    m_bufferNeedsUpdate = false;
}

void LineManager::updateUniforms() {
    LineUniforms* uniforms = static_cast<LineUniforms*>([m_uniformBuffer contents]);
    uniforms->screenWidth = static_cast<float>(m_screenWidth);
    uniforms->screenHeight = static_cast<float>(m_screenHeight);
}

void LineManager::render(id<MTLRenderCommandEncoder> encoder) {
    std::lock_guard<std::mutex> lock(m_mutex);

    size_t totalLines = 0;
    for (const auto& pair : m_managedLines) {
        if (pair.second.visible) totalLines++;
    }

    static bool firstRender = true;
    if (firstRender && totalLines > 0) {
        NSLog(@"[LineManager] First render: %zu lines, pipelineState=%p, buffer=%p",
              totalLines, m_pipelineState, m_instanceBuffer);
        NSLog(@"[LineManager] Screen size: %dx%d", m_screenWidth, m_screenHeight);

        // Log the first line's data after buffer update
        updateInstanceBuffer();
        if (!m_instanceBuffer) {
            NSLog(@"[LineManager] ERROR: Instance buffer is nil in resizeBuffer");
            return;
        }

        void* bufferContents = [m_instanceBuffer contents];
        if (!bufferContents) {
            NSLog(@"[LineManager] ERROR: Buffer contents is nil in resizeBuffer");
            return;
        }

        LineInstance* buffer = static_cast<LineInstance*>(bufferContents);
        NSLog(@"[LineManager] First line in buffer: (%.1f,%.1f)->(%.1f,%.1f) thickness=%.1f color1=0x%08X",
              buffer[0].x1, buffer[0].y1, buffer[0].x2, buffer[0].y2, buffer[0].thickness, buffer[0].color1);

        firstRender = false;
    }

    if (totalLines == 0 || !m_pipelineState) {
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
              instanceCount:totalLines];
}

} // namespace SuperTerminal
