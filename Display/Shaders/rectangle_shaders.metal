//
//  rectangle_shaders.metal
//  SuperTerminal Framework - GPU-Accelerated Rectangle Rendering Shaders
//
//  High-performance instanced rectangle rendering with gradient support
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#include <metal_stdlib>
using namespace metal;

// =============================================================================
// Data Structures
// =============================================================================

// Rectangle instance data (matches C++ RectangleInstance)
struct RectangleInstance {
    float2 position;        // x, y
    float2 size;            // width, height
    uint color1;            // RGBA8888
    uint color2;            // RGBA8888
    uint color3;            // RGBA8888
    uint color4;            // RGBA8888
    uint mode;              // Gradient mode
    float param1;           // Pattern parameter 1
    float param2;           // Pattern parameter 2
    float param3;           // Pattern parameter 3
    float rotation;         // Rotation in radians
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
    float2 texCoord;        // 0-1 coords within rectangle for gradients
    uint mode;
    float4 color1;          // For multi-color gradients
    float4 color2;
    float4 color3;
    float4 color4;
};

// Gradient modes (must match C++ enum)
constant uint GRADIENT_SOLID = 0;
constant uint GRADIENT_HORIZONTAL = 1;
constant uint GRADIENT_VERTICAL = 2;
constant uint GRADIENT_DIAGONAL_TL_BR = 3;
constant uint GRADIENT_DIAGONAL_TR_BL = 4;
constant uint GRADIENT_RADIAL = 5;
constant uint GRADIENT_FOUR_CORNER = 6;
constant uint GRADIENT_THREE_POINT = 7;

// =============================================================================
// Helper Functions
// =============================================================================

// Unpack RGBA8888 to float4
float4 unpackColor(uint packedColor) {
    float r = float((packedColor >> 24) & 0xFF) / 255.0;
    float g = float((packedColor >> 16) & 0xFF) / 255.0;
    float b = float((packedColor >> 8) & 0xFF) / 255.0;
    float a = float(packedColor & 0xFF) / 255.0;
    return float4(r, g, b, a);
}

// =============================================================================
// Vertex Shader
// =============================================================================

vertex VertexOut rectangle_vertex(
    uint vertexID [[vertex_id]],
    uint instanceID [[instance_id]],
    constant RectangleInstance *instances [[buffer(0)]],
    constant RectangleUniforms &uniforms [[buffer(1)]]
) {
    // Get rectangle instance data
    RectangleInstance rect = instances[instanceID];

    // Generate quad vertices (2 triangles = 6 vertices)
    // Vertex order: TL, BR, TR, TL, BL, BR (CCW winding)
    float2 positions[6] = {
        rect.position,                                      // 0: Top-left
        rect.position + rect.size,                          // 1: Bottom-right
        rect.position + float2(rect.size.x, 0),            // 2: Top-right
        rect.position,                                      // 3: Top-left
        rect.position + float2(0, rect.size.y),            // 4: Bottom-left
        rect.position + rect.size                           // 5: Bottom-right
    };

    // Texture coordinates for gradient interpolation
    float2 texCoords[6] = {
        float2(0, 0),  // Top-left
        float2(1, 1),  // Bottom-right
        float2(1, 0),  // Top-right
        float2(0, 0),  // Top-left
        float2(0, 1),  // Bottom-left
        float2(1, 1)   // Bottom-right
    };

    float2 position = positions[vertexID];
    float2 texCoord = texCoords[vertexID];

    // Transform to clip space (-1 to 1)
    // Metal coordinate system: top-left is (0,0), bottom-right is (width, height)
    // Clip space: top-left is (-1, -1), bottom-right is (1, 1)
    float x = (position.x / uniforms.screenWidth) * 2.0 - 1.0;
    float y = 1.0 - (position.y / uniforms.screenHeight) * 2.0;

    VertexOut out;
    out.position = float4(x, y, 0.0, 1.0);
    out.texCoord = texCoord;
    out.mode = rect.mode;

    // Unpack all colors for fragment shader
    out.color1 = unpackColor(rect.color1);
    out.color2 = unpackColor(rect.color2);
    out.color3 = unpackColor(rect.color3);
    out.color4 = unpackColor(rect.color4);

    // For solid color, we can optimize by setting color here
    if (rect.mode == GRADIENT_SOLID) {
        out.color = out.color1;
    } else {
        out.color = float4(0, 0, 0, 0);  // Fragment shader will compute
    }

    return out;
}

// =============================================================================
// Fragment Shader
// =============================================================================

fragment float4 rectangle_fragment(VertexOut in [[stage_in]]) {
    // Solid color - already computed in vertex shader
    if (in.mode == GRADIENT_SOLID) {
        return in.color;
    }

    float2 uv = in.texCoord;
    float4 color;

    // Horizontal gradient: left to right
    if (in.mode == GRADIENT_HORIZONTAL) {
        color = mix(in.color1, in.color2, uv.x);
    }
    // Vertical gradient: top to bottom
    else if (in.mode == GRADIENT_VERTICAL) {
        color = mix(in.color1, in.color2, uv.y);
    }
    // Diagonal gradient: top-left to bottom-right
    else if (in.mode == GRADIENT_DIAGONAL_TL_BR) {
        float t = (uv.x + uv.y) * 0.5;
        color = mix(in.color1, in.color2, t);
    }
    // Diagonal gradient: top-right to bottom-left
    else if (in.mode == GRADIENT_DIAGONAL_TR_BL) {
        float t = (1.0 - uv.x + uv.y) * 0.5;
        color = mix(in.color1, in.color2, t);
    }
    // Radial gradient: center outward
    else if (in.mode == GRADIENT_RADIAL) {
        float2 center = float2(0.5, 0.5);
        float dist = distance(uv, center) * 1.41421356;  // sqrt(2) to reach corners
        color = mix(in.color1, in.color2, clamp(dist, 0.0, 1.0));
    }
    // Four-corner gradient: bilinear interpolation
    else if (in.mode == GRADIENT_FOUR_CORNER) {
        float4 top = mix(in.color1, in.color2, uv.x);
        float4 bottom = mix(in.color4, in.color3, uv.x);
        color = mix(top, bottom, uv.y);
    }
    // Three-point gradient: top-left, top-right, bottom-center
    else if (in.mode == GRADIENT_THREE_POINT) {
        if (uv.y < 0.5) {
            // Top half: blend color1 to color2
            color = mix(in.color1, in.color2, uv.x);
        } else {
            // Bottom half: blend toward color3
            float4 topColor = mix(in.color1, in.color2, uv.x);
            float bottomBlend = (uv.y - 0.5) * 2.0;
            color = mix(topColor, in.color3, bottomBlend);
        }
    }
    // Unknown mode - fallback to solid color1
    else {
        color = in.color1;
    }

    return color;
}
