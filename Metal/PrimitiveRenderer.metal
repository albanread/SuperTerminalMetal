//
// PrimitiveRenderer.metal
// SuperTerminal v2
//
// Metal shaders for 2D primitive rendering (rectangles, circles, lines, pixels)
// For direct composition of simple objects at high speed.

#include <metal_stdlib>
using namespace metal;

// Vertex structure for primitive rendering
struct PrimitiveVertex {
    float2 position;
    float4 color;
};

// Vertex output / Fragment input
struct PrimitiveVertexOut {
    float4 position [[position]];
    float4 color;
    float2 center;      // For circles
    float radius;       // For circles
    float2 fragCoord;   // For distance calculations
};

// Uniforms for primitive rendering
struct PrimitiveUniforms {
    float4x4 projectionMatrix;
    float2 viewportSize;
};

// ============================================================================
// Vertex Shaders
// ============================================================================

// Simple vertex shader for filled primitives (rects, circles)
vertex PrimitiveVertexOut primitive_vertex(
    uint vertexID [[vertex_id]],
    const device PrimitiveVertex* vertices [[buffer(0)]],
    constant PrimitiveUniforms& uniforms [[buffer(1)]]
) {
    PrimitiveVertexOut out;

    PrimitiveVertex vert = vertices[vertexID];

    // Apply projection
    float4 projected = uniforms.projectionMatrix * float4(vert.position, 0.0, 1.0);
    out.position = projected;
    out.color = vert.color;

    return out;
}

// Vertex shader for circles (includes center and radius for distance calculation)
vertex PrimitiveVertexOut circle_vertex(
    uint vertexID [[vertex_id]],
    const device PrimitiveVertex* vertices [[buffer(0)]],
    constant PrimitiveUniforms& uniforms [[buffer(1)]],
    constant float2& center [[buffer(2)]],
    constant float& radius [[buffer(3)]]
) {
    PrimitiveVertexOut out;

    PrimitiveVertex vert = vertices[vertexID];

    // Apply projection
    float4 projected = uniforms.projectionMatrix * float4(vert.position, 0.0, 1.0);
    out.position = projected;
    out.color = vert.color;
    out.center = center;
    out.radius = radius;
    out.fragCoord = vert.position;

    return out;
}

// ============================================================================
// Fragment Shaders
// ============================================================================

// Simple fragment shader for filled primitives
fragment float4 primitive_fragment(
    PrimitiveVertexOut in [[stage_in]]
) {
    return in.color;
}

// Fragment shader for filled circles with smooth edges
fragment float4 circle_fragment(
    PrimitiveVertexOut in [[stage_in]]
) {
    // Calculate distance from center
    float2 diff = in.fragCoord - in.center;
    float dist = length(diff);

    // Smooth antialiasing at the edge
    float edge = in.radius;
    float smoothWidth = 1.0;
    float alpha = smoothstep(edge + smoothWidth, edge - smoothWidth, dist);

    return float4(in.color.rgb, in.color.a * alpha);
}

// Fragment shader for circle outline
fragment float4 circle_outline_fragment(
    PrimitiveVertexOut in [[stage_in]],
    constant float& lineWidth [[buffer(0)]]
) {
    // Calculate distance from center
    float2 diff = in.fragCoord - in.center;
    float dist = length(diff);

    // Calculate distance from the circle edge
    float outerRadius = in.radius + lineWidth * 0.5;
    float innerRadius = in.radius - lineWidth * 0.5;

    // Smooth antialiasing
    float smoothWidth = 1.0;
    float outerAlpha = smoothstep(outerRadius + smoothWidth, outerRadius - smoothWidth, dist);
    float innerAlpha = 1.0 - smoothstep(innerRadius - smoothWidth, innerRadius + smoothWidth, dist);
    float alpha = outerAlpha * innerAlpha;

    return float4(in.color.rgb, in.color.a * alpha);
}

// Fragment shader for rectangle outline
fragment float4 rect_outline_fragment(
    PrimitiveVertexOut in [[stage_in]],
    constant float4& bounds [[buffer(0)]],  // x, y, width, height
    constant float& lineWidth [[buffer(1)]]
) {
    // Get fragment position (from vertex position passed through)
    float2 pos = in.fragCoord;

    // Calculate distance to each edge
    float left = pos.x - bounds.x;
    float right = (bounds.x + bounds.z) - pos.x;
    float top = pos.y - bounds.y;
    float bottom = (bounds.y + bounds.w) - pos.y;

    // Find minimum distance to any edge
    float minDist = min(min(left, right), min(top, bottom));

    // Check if we're inside the rectangle
    bool inside = left > 0.0 && right > 0.0 && top > 0.0 && bottom > 0.0;

    if (!inside) {
        discard_fragment();
    }

    // Only draw if we're within lineWidth of an edge
    float halfWidth = lineWidth * 0.5;
    float smoothWidth = 1.0;
    float alpha = 1.0 - smoothstep(halfWidth - smoothWidth, halfWidth + smoothWidth, minDist);

    if (alpha < 0.01) {
        discard_fragment();
    }

    return float4(in.color.rgb, in.color.a * alpha);
}

// Fragment shader for lines with antialiasing
fragment float4 line_fragment(
    PrimitiveVertexOut in [[stage_in]],
    constant float2& lineStart [[buffer(0)]],
    constant float2& lineEnd [[buffer(1)]],
    constant float& lineWidth [[buffer(2)]]
) {
    float2 pos = in.fragCoord;

    // Calculate distance from point to line segment
    float2 pa = pos - lineStart;
    float2 ba = lineEnd - lineStart;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    float dist = length(pa - ba * h);

    // Smooth antialiasing
    float halfWidth = lineWidth * 0.5;
    float smoothWidth = 1.0;
    float alpha = 1.0 - smoothstep(halfWidth - smoothWidth, halfWidth + smoothWidth, dist);

    return float4(in.color.rgb, in.color.a * alpha);
}

// ============================================================================
// Specialized Shaders
// ============================================================================

// Vertex shader that passes through fragment coordinates for distance calculations
vertex PrimitiveVertexOut distance_vertex(
    uint vertexID [[vertex_id]],
    const device PrimitiveVertex* vertices [[buffer(0)]],
    constant PrimitiveUniforms& uniforms [[buffer(1)]]
) {
    PrimitiveVertexOut out;

    PrimitiveVertex vert = vertices[vertexID];

    // Apply projection
    float4 projected = uniforms.projectionMatrix * float4(vert.position, 0.0, 1.0);
    out.position = projected;
    out.color = vert.color;
    out.fragCoord = vert.position;

    return out;
}

// Fragment shader for single pixel rendering
fragment float4 pixel_fragment(
    PrimitiveVertexOut in [[stage_in]]
) {
    return in.color;
}

// Fragment shader with gamma correction for better color rendering
fragment float4 primitive_fragment_gamma(
    PrimitiveVertexOut in [[stage_in]]
) {
    // Simple gamma correction (gamma = 2.2)
    float3 linear = pow(in.color.rgb, float3(2.2));
    float3 corrected = pow(linear, float3(1.0 / 2.2));
    return float4(corrected, in.color.a);
}

// Debug fragment shader that shows the primitive bounds
fragment float4 debug_primitive_fragment(
    PrimitiveVertexOut in [[stage_in]]
) {
    // Show magenta for debugging
    return float4(1.0, 0.0, 1.0, 0.5);
}
