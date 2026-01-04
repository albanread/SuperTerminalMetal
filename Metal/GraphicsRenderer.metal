//
// GraphicsRenderer.metal
// SuperTerminal v2
//
// Metal shaders for compositing the graphics layer RGBA texture.
// This shader properly samples RGBA textures (not alpha-only like text shader).
//

#include <metal_stdlib>
using namespace metal;

// Vertex structure for full-screen quad
struct GraphicsVertex {
    float2 position [[attribute(0)]];
    float2 texCoord [[attribute(1)]];
};

// Vertex output / Fragment input
struct GraphicsVertexOut {
    float4 position [[position]];
    float2 texCoord;
};

// Vertex shader - simple passthrough for full-screen quad
vertex GraphicsVertexOut graphics_vertex(
    uint vertexID [[vertex_id]],
    constant GraphicsVertex* vertices [[buffer(0)]]
) {
    GraphicsVertexOut out;
    GraphicsVertex vert = vertices[vertexID];

    out.position = float4(vert.position, 0.0, 1.0);
    out.texCoord = vert.texCoord;

    return out;
}

// Fragment shader - sample RGBA texture and output directly
fragment float4 graphics_fragment(
    GraphicsVertexOut in [[stage_in]],
    texture2d<float> graphicsTexture [[texture(0)]]
) {
    constexpr sampler textureSampler(
        mag_filter::linear,
        min_filter::linear,
        address::clamp_to_edge
    );

    // Sample the RGBA texture directly
    float4 color = graphicsTexture.sample(textureSampler, in.texCoord);

    // Return the color as-is (no alpha tricks like the text shader)
    return color;
}
