//
// TilemapRenderer.metal
// SuperTerminal Framework - Tilemap System
//
// Metal shaders for GPU-accelerated tilemap rendering with instancing
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#include <metal_stdlib>
using namespace metal;

// =============================================================================
// Structures
// =============================================================================

/// Per-instance data for each visible tile
struct TileInstance {
    float2 position;        // World position (top-left corner)
    float2 texCoordBase;    // Base UV coordinate (top-left of tile in atlas)
    float2 texCoordSize;    // UV size (width/height in atlas)
    float4 tintColor;       // RGBA tint/modulation color
    uint flags;             // Bit flags: flip X/Y, rotation
};

/// Vertex input (quad corners)
struct VertexIn {
    float2 position [[attribute(0)]];  // Local quad position (0-1)
    float2 texCoord [[attribute(1)]];  // Local texture coordinate (0-1)
};

/// Vertex output / Fragment input
struct VertexOut {
    float4 position [[position]];
    float2 texCoord;
    float4 tintColor;
};

/// Uniform data for the tilemap renderer
struct TilemapUniforms {
    float4x4 viewProjectionMatrix;  // Combined view-projection matrix
    float2 viewportSize;            // Viewport dimensions
    float2 tileSize;                // Tile size in pixels
    float time;                     // Elapsed time for animations
    float opacity;                  // Layer opacity
    uint flags;                     // Rendering flags
    uint padding;                   // Alignment
};

// =============================================================================
// Constants
// =============================================================================

constant float2 QUAD_POSITIONS[4] = {
    float2(0.0, 0.0),  // Top-left
    float2(1.0, 0.0),  // Top-right
    float2(0.0, 1.0),  // Bottom-left
    float2(1.0, 1.0)   // Bottom-right
};

constant float2 QUAD_TEXCOORDS[4] = {
    float2(0.0, 0.0),  // Top-left
    float2(1.0, 0.0),  // Top-right
    float2(0.0, 1.0),  // Bottom-left
    float2(1.0, 1.0)   // Bottom-right
};

// Flip flags
constant uint FLAG_FLIP_X = 0x02;
constant uint FLAG_FLIP_Y = 0x04;
constant uint FLAG_ROTATE_90 = 0x08;
constant uint FLAG_ROTATE_180 = 0x10;
constant uint FLAG_ROTATE_270 = 0x20;

// =============================================================================
// Vertex Shader
// =============================================================================

vertex VertexOut tilemapVertexShader(
    VertexIn in [[stage_in]],
    constant TilemapUniforms& uniforms [[buffer(0)]],
    constant TileInstance* instances [[buffer(1)]],
    uint instanceID [[instance_id]]
) {
    VertexOut out;

    // Get instance data
    TileInstance instance = instances[instanceID];

    // Start with base quad position (0-1)
    float2 localPos = in.position;
    float2 localTex = in.texCoord;

    // Apply transformations based on flags
    uint flags = instance.flags;

    // Handle rotation (apply before flips)
    if (flags & FLAG_ROTATE_90) {
        // 90° clockwise: (x,y) -> (1-y,x)
        float2 rotated = float2(1.0 - localPos.y, localPos.x);
        localPos = rotated;
        rotated = float2(1.0 - localTex.y, localTex.x);
        localTex = rotated;
    } else if (flags & FLAG_ROTATE_180) {
        // 180°: (x,y) -> (1-x,1-y)
        localPos = float2(1.0 - localPos.x, 1.0 - localPos.y);
        localTex = float2(1.0 - localTex.x, 1.0 - localTex.y);
    } else if (flags & FLAG_ROTATE_270) {
        // 270° clockwise (90° CCW): (x,y) -> (y,1-x)
        float2 rotated = float2(localPos.y, 1.0 - localPos.x);
        localPos = rotated;
        rotated = float2(localTex.y, 1.0 - localTex.x);
        localTex = rotated;
    }

    // Handle flips
    if (flags & FLAG_FLIP_X) {
        localTex.x = 1.0 - localTex.x;
    }
    if (flags & FLAG_FLIP_Y) {
        localTex.y = 1.0 - localTex.y;
    }

    // Scale local position by tile size
    float2 worldPos = instance.position + localPos * uniforms.tileSize;

    // Transform to clip space
    float4 clipPos = uniforms.viewProjectionMatrix * float4(worldPos, 0.0, 1.0);
    out.position = clipPos;

    // Calculate final texture coordinate (instance base + local offset * size)
    out.texCoord = instance.texCoordBase + localTex * instance.texCoordSize;

    // Pass through tint color with layer opacity
    out.tintColor = instance.tintColor;
    out.tintColor.a *= uniforms.opacity;

    return out;
}

// =============================================================================
// Fragment Shader
// =============================================================================

fragment float4 tilemapFragmentShader(
    VertexOut in [[stage_in]],
    texture2d<float> tilesetTexture [[texture(0)]],
    sampler textureSampler [[sampler(0)]]
) {
    // Sample the tileset texture
    float4 texColor = tilesetTexture.sample(textureSampler, in.texCoord);

    // Apply tint color (modulation)
    float4 finalColor = texColor * in.tintColor;

    // Discard fully transparent pixels (optional optimization)
    if (finalColor.a < 0.01) {
        discard_fragment();
    }

    return finalColor;
}

// =============================================================================
// Alternative Fragment Shader with Alpha Testing
// =============================================================================

fragment float4 tilemapFragmentShaderAlphaTest(
    VertexOut in [[stage_in]],
    texture2d<float> tilesetTexture [[texture(0)]],
    sampler textureSampler [[sampler(0)]],
    constant TilemapUniforms& uniforms [[buffer(0)]]
) {
    // Sample the tileset texture
    float4 texColor = tilesetTexture.sample(textureSampler, in.texCoord);

    // Alpha test threshold
    const float alphaThreshold = 0.1;
    if (texColor.a < alphaThreshold) {
        discard_fragment();
    }

    // Apply tint color (modulation)
    float4 finalColor = texColor * in.tintColor;

    return finalColor;
}

// =============================================================================
// Debug Fragment Shader (wireframe/grid overlay)
// =============================================================================

fragment float4 tilemapFragmentShaderDebug(
    VertexOut in [[stage_in]],
    texture2d<float> tilesetTexture [[texture(0)]],
    sampler textureSampler [[sampler(0)]],
    constant TilemapUniforms& uniforms [[buffer(0)]]
) {
    // Sample the tileset texture
    float4 texColor = tilesetTexture.sample(textureSampler, in.texCoord);

    // Apply tint color
    float4 finalColor = texColor * in.tintColor;

    // Add grid overlay
    float2 uv = in.texCoord;
    float2 gridUV = fract(uv * uniforms.tileSize);
    float gridLine = step(0.95, max(gridUV.x, gridUV.y));

    // Mix in yellow grid lines
    finalColor = mix(finalColor, float4(1.0, 1.0, 0.0, 1.0), gridLine * 0.3);

    return finalColor;
}

// =============================================================================
// Optimized Vertex Shader (no per-vertex attributes, procedural quad)
// =============================================================================

vertex VertexOut tilemapVertexShaderProceduralQuad(
    constant TilemapUniforms& uniforms [[buffer(0)]],
    constant TileInstance* instances [[buffer(1)]],
    uint vertexID [[vertex_id]],
    uint instanceID [[instance_id]]
) {
    VertexOut out;

    // Generate quad vertex procedurally (6 vertices per quad: 2 triangles)
    // Vertex order: 0,1,2, 2,1,3 (CCW winding)
    uint quadVertexIndex = vertexID % 6;
    uint remappedIndex;

    switch (quadVertexIndex) {
        case 0: remappedIndex = 0; break; // TL
        case 1: remappedIndex = 1; break; // TR
        case 2: remappedIndex = 2; break; // BL
        case 3: remappedIndex = 2; break; // BL
        case 4: remappedIndex = 1; break; // TR
        case 5: remappedIndex = 3; break; // BR
    }

    float2 localPos = QUAD_POSITIONS[remappedIndex];
    float2 localTex = QUAD_TEXCOORDS[remappedIndex];

    // Get instance data
    TileInstance instance = instances[instanceID];

    // Apply transformations (same as above)
    uint flags = instance.flags;

    if (flags & FLAG_ROTATE_90) {
        float2 rotated = float2(1.0 - localPos.y, localPos.x);
        localPos = rotated;
        rotated = float2(1.0 - localTex.y, localTex.x);
        localTex = rotated;
    } else if (flags & FLAG_ROTATE_180) {
        localPos = float2(1.0 - localPos.x, 1.0 - localPos.y);
        localTex = float2(1.0 - localTex.x, 1.0 - localTex.y);
    } else if (flags & FLAG_ROTATE_270) {
        float2 rotated = float2(localPos.y, 1.0 - localPos.x);
        localPos = rotated;
        rotated = float2(localTex.y, 1.0 - localTex.x);
        localTex = rotated;
    }

    if (flags & FLAG_FLIP_X) {
        localTex.x = 1.0 - localTex.x;
    }
    if (flags & FLAG_FLIP_Y) {
        localTex.y = 1.0 - localTex.y;
    }

    // Scale local position by tile size
    float2 worldPos = instance.position + localPos * uniforms.tileSize;

    // Transform to clip space
    float4 clipPos = uniforms.viewProjectionMatrix * float4(worldPos, 0.0, 1.0);
    out.position = clipPos;

    // Calculate final texture coordinate
    out.texCoord = instance.texCoordBase + localTex * instance.texCoordSize;

    // Pass through tint color with layer opacity
    out.tintColor = instance.tintColor;
    out.tintColor.a *= uniforms.opacity;

    return out;
}

// =============================================================================
// INDEXED TILE RENDERING (Palette-Driven)
// =============================================================================

/// Per-instance data for indexed tiles (adds palette index)
struct TileInstanceIndexed {
    float2 position;        // World position (top-left corner)
    float2 texCoordBase;    // Base UV coordinate (top-left of tile in atlas)
    float2 texCoordSize;    // UV size (width/height in atlas)
    float4 tintColor;       // RGBA tint/modulation color
    uint flags;             // Bit flags: flip X/Y, rotation
    uint paletteIndex;      // Palette index (0-31)
    uint padding[2];        // Alignment to 16 bytes
};

/// Vertex output for indexed rendering (adds palette index)
struct VertexOutIndexed {
    float4 position [[position]];
    float2 texCoord;
    float4 tintColor;
    uint paletteIndex;
};

// =============================================================================
// Indexed Vertex Shader
// =============================================================================

vertex VertexOutIndexed tilemapVertexShaderIndexed(
    VertexIn in [[stage_in]],
    constant TilemapUniforms& uniforms [[buffer(0)]],
    constant TileInstanceIndexed* instances [[buffer(1)]],
    uint instanceID [[instance_id]]
) {
    VertexOutIndexed out;

    // Get instance data
    TileInstanceIndexed instance = instances[instanceID];

    // Start with base quad position (0-1)
    float2 localPos = in.position;
    float2 localTex = in.texCoord;

    // Apply transformations based on flags
    uint flags = instance.flags;

    // Handle rotation (apply before flips)
    if (flags & FLAG_ROTATE_90) {
        float2 rotated = float2(1.0 - localPos.y, localPos.x);
        localPos = rotated;
        rotated = float2(1.0 - localTex.y, localTex.x);
        localTex = rotated;
    } else if (flags & FLAG_ROTATE_180) {
        localPos = float2(1.0 - localPos.x, 1.0 - localPos.y);
        localTex = float2(1.0 - localTex.x, 1.0 - localTex.y);
    } else if (flags & FLAG_ROTATE_270) {
        float2 rotated = float2(localPos.y, 1.0 - localPos.x);
        localPos = rotated;
        rotated = float2(localTex.y, 1.0 - localTex.x);
        localTex = rotated;
    }

    // Handle flips
    if (flags & FLAG_FLIP_X) {
        localTex.x = 1.0 - localTex.x;
    }
    if (flags & FLAG_FLIP_Y) {
        localTex.y = 1.0 - localTex.y;
    }

    // Scale local position by tile size
    float2 worldPos = instance.position + localPos * uniforms.tileSize;

    // Transform to clip space
    float4 clipPos = uniforms.viewProjectionMatrix * float4(worldPos, 0.0, 1.0);
    out.position = clipPos;

    // Calculate final texture coordinate (instance base + local offset * size)
    out.texCoord = instance.texCoordBase + localTex * instance.texCoordSize;

    // Pass through tint color with layer opacity
    out.tintColor = instance.tintColor;
    out.tintColor.a *= uniforms.opacity;

    // Pass through palette index
    out.paletteIndex = instance.paletteIndex;

    return out;
}

// =============================================================================
// Indexed Fragment Shader (Double Lookup)
// =============================================================================

fragment float4 tilemapFragmentShaderIndexed(
    VertexOutIndexed in [[stage_in]],
    texture2d<uint> indexTexture [[texture(0)]],     // R8Uint: tile indices (0-15)
    texture2d<float> paletteTexture [[texture(1)]],  // RGBA8: palette colors
    sampler textureSampler [[sampler(0)]]
) {
    // Step 1: Sample index texture to get color index (0-15)
    // Use nearest neighbor sampling for pixel-perfect index lookup
    uint2 indexCoord = uint2(in.texCoord * float2(indexTexture.get_width(), indexTexture.get_height()));
    uint colorIndex = indexTexture.read(indexCoord).r;

    // Step 2: Sample palette texture at (colorIndex, paletteIndex)
    // Palette texture layout: width = colorsPerPalette (16), height = paletteCount (32+)
    float2 paletteCoord;
    paletteCoord.x = (float(colorIndex) + 0.5) / 16.0;  // Color index (0-15)
    paletteCoord.y = (float(in.paletteIndex) + 0.5) / 32.0;  // Palette index (0-31)

    // Sample the final color from palette
    float4 finalColor = paletteTexture.sample(textureSampler, paletteCoord);

    // Apply tint color (modulation)
    finalColor *= in.tintColor;

    // Discard fully transparent pixels (palette[0] should have alpha=0)
    if (finalColor.a < 0.01) {
        discard_fragment();
    }

    return finalColor;
}

// =============================================================================
// Indexed Fragment Shader with Alpha Test
// =============================================================================

fragment float4 tilemapFragmentShaderIndexedAlphaTest(
    VertexOutIndexed in [[stage_in]],
    texture2d<uint> indexTexture [[texture(0)]],
    texture2d<float> paletteTexture [[texture(1)]],
    sampler textureSampler [[sampler(0)]],
    constant TilemapUniforms& uniforms [[buffer(0)]]
) {
    // Step 1: Sample index texture
    uint2 indexCoord = uint2(in.texCoord * float2(indexTexture.get_width(), indexTexture.get_height()));
    uint colorIndex = indexTexture.read(indexCoord).r;

    // Step 2: Sample palette texture
    float2 paletteCoord;
    paletteCoord.x = (float(colorIndex) + 0.5) / 16.0;
    paletteCoord.y = (float(in.paletteIndex) + 0.5) / 32.0;

    float4 paletteColor = paletteTexture.sample(textureSampler, paletteCoord);

    // Alpha test threshold
    const float alphaThreshold = 0.1;
    if (paletteColor.a < alphaThreshold) {
        discard_fragment();
    }

    // Apply tint color
    float4 finalColor = paletteColor * in.tintColor;

    return finalColor;
}

// =============================================================================
// Indexed Debug Fragment Shader
// =============================================================================

fragment float4 tilemapFragmentShaderIndexedDebug(
    VertexOutIndexed in [[stage_in]],
    texture2d<uint> indexTexture [[texture(0)]],
    texture2d<float> paletteTexture [[texture(1)]],
    sampler textureSampler [[sampler(0)]],
    constant TilemapUniforms& uniforms [[buffer(0)]]
) {
    // Step 1: Sample index texture
    uint2 indexCoord = uint2(in.texCoord * float2(indexTexture.get_width(), indexTexture.get_height()));
    uint colorIndex = indexTexture.read(indexCoord).r;

    // Step 2: Sample palette texture
    float2 paletteCoord;
    paletteCoord.x = (float(colorIndex) + 0.5) / 16.0;
    paletteCoord.y = (float(in.paletteIndex) + 0.5) / 32.0;

    float4 paletteColor = paletteTexture.sample(textureSampler, paletteCoord);

    // Apply tint color
    float4 finalColor = paletteColor * in.tintColor;

    // Add grid overlay
    float2 uv = in.texCoord;
    float2 gridUV = fract(uv * uniforms.tileSize);
    float gridLine = step(0.95, max(gridUV.x, gridUV.y));

    // Mix in yellow grid lines
    finalColor = mix(finalColor, float4(1.0, 1.0, 0.0, 1.0), gridLine * 0.3);

    // Add palette index indicator in corner (for debugging)
    float2 cornerUV = fract(uv * uniforms.tileSize);
    bool isCorner = cornerUV.x < 0.2 && cornerUV.y < 0.2;
    if (isCorner) {
        // Color-code palette index (cycle through hues)
        float hue = float(in.paletteIndex % 8) / 8.0;
        float3 debugColor = float3(
            abs(hue * 6.0 - 3.0) - 1.0,
            2.0 - abs(hue * 6.0 - 2.0),
            2.0 - abs(hue * 6.0 - 4.0)
        );
        debugColor = clamp(debugColor, 0.0, 1.0);
        finalColor.rgb = mix(finalColor.rgb, debugColor, 0.5);
    }

    return finalColor;
}

// =============================================================================
// Procedural Quad Indexed Vertex Shader
// =============================================================================

vertex VertexOutIndexed tilemapVertexShaderIndexedProceduralQuad(
    constant TilemapUniforms& uniforms [[buffer(0)]],
    constant TileInstanceIndexed* instances [[buffer(1)]],
    uint vertexID [[vertex_id]],
    uint instanceID [[instance_id]]
) {
    VertexOutIndexed out;

    // Generate quad vertex procedurally
    uint quadVertexIndex = vertexID % 6;
    uint remappedIndex;

    switch (quadVertexIndex) {
        case 0: remappedIndex = 0; break; // TL
        case 1: remappedIndex = 1; break; // TR
        case 2: remappedIndex = 2; break; // BL
        case 3: remappedIndex = 2; break; // BL
        case 4: remappedIndex = 1; break; // TR
        case 5: remappedIndex = 3; break; // BR
    }

    float2 localPos = QUAD_POSITIONS[remappedIndex];
    float2 localTex = QUAD_TEXCOORDS[remappedIndex];

    // Get instance data
    TileInstanceIndexed instance = instances[instanceID];

    // Apply transformations
    uint flags = instance.flags;

    if (flags & FLAG_ROTATE_90) {
        float2 rotated = float2(1.0 - localPos.y, localPos.x);
        localPos = rotated;
        rotated = float2(1.0 - localTex.y, localTex.x);
        localTex = rotated;
    } else if (flags & FLAG_ROTATE_180) {
        localPos = float2(1.0 - localPos.x, 1.0 - localPos.y);
        localTex = float2(1.0 - localTex.x, 1.0 - localTex.y);
    } else if (flags & FLAG_ROTATE_270) {
        float2 rotated = float2(localPos.y, 1.0 - localPos.x);
        localPos = rotated;
        rotated = float2(localTex.y, 1.0 - localTex.x);
        localTex = rotated;
    }

    if (flags & FLAG_FLIP_X) {
        localTex.x = 1.0 - localTex.x;
    }
    if (flags & FLAG_FLIP_Y) {
        localTex.y = 1.0 - localTex.y;
    }

    // Scale local position by tile size
    float2 worldPos = instance.position + localPos * uniforms.tileSize;

    // Transform to clip space
    float4 clipPos = uniforms.viewProjectionMatrix * float4(worldPos, 0.0, 1.0);
    out.position = clipPos;

    // Calculate final texture coordinate
    out.texCoord = instance.texCoordBase + localTex * instance.texCoordSize;

    // Pass through tint color with layer opacity
    out.tintColor = instance.tintColor;
    out.tintColor.a *= uniforms.opacity;

    // Pass through palette index
    out.paletteIndex = instance.paletteIndex;

    return out;
}
