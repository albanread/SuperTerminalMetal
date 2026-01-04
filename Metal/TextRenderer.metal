// TextRenderer.metal
// SuperTerminal v2.0 - Metal Shaders for Text Rendering
// GPU-accelerated character rendering
// "Look we dont need an entire web browser to display text"

#include <metal_stdlib>
using namespace metal;

// =============================================================================
// Shader Constants and Structures
// =============================================================================

/// Maximum number of characters per draw call
constant int MAX_CHARACTERS = 2048;

/// Vertex input from application
struct VertexIn {
    float2 position;        ///< Position in normalized device coordinates
    float2 texCoord;        ///< Texture coordinates for font atlas
    float4 foreground;      ///< Foreground color (RGBA)
    float4 background;      ///< Background color (RGBA)
};

/// Vertex output / Fragment input
struct VertexOut {
    float4 position [[position]];
    float2 texCoord;
    float4 foreground;
    float4 background;
};

/// Uniforms passed to shaders
struct Uniforms {
    float4x4 projectionMatrix;  ///< Orthographic projection matrix
    float2 cellSize;            ///< Size of each character cell in pixels
    float2 atlasSize;           ///< Size of font texture atlas
    float2 glyphSize;           ///< Size of each glyph in atlas
};

// =============================================================================
// Vertex Shader
// =============================================================================

/// Vertex shader for text rendering
/// Transforms character quad vertices to screen space
vertex VertexOut textVertexShader(
    uint vertexID [[vertex_id]],
    constant VertexIn* vertices [[buffer(0)]],
    constant Uniforms& uniforms [[buffer(1)]]
) {
    VertexOut out;

    // Get input vertex
    VertexIn in = vertices[vertexID];

    // Transform position to clip space
    out.position = uniforms.projectionMatrix * float4(in.position, 0.0, 1.0);

    // Pass through texture coordinates and colors
    out.texCoord = in.texCoord;
    out.foreground = in.foreground;
    out.background = in.background;

    return out;
}

// =============================================================================
// Fragment Shader
// =============================================================================

/// Fragment shader for text rendering
/// Samples font atlas and applies foreground/background colors
/// Supports both RGBA (antialiased with alpha) and grayscale (red channel) formats
fragment float4 textFragmentShader(
    VertexOut in [[stage_in]],
    texture2d<float> fontAtlas [[texture(0)]],
    sampler fontSampler [[sampler(0)]]
) {
    // Sample the font atlas
    float4 glyphSample = fontAtlas.sample(fontSampler, in.texCoord);

    // Detect format and use appropriate channel for alpha
    // For RGBA textures (new antialiased format): use alpha channel
    // For grayscale textures (legacy format): use red channel
    float glyphAlpha = glyphSample.a > 0.0 ? glyphSample.a : glyphSample.r;

    // Mix foreground and background based on glyph alpha
    // glyphAlpha = 1.0 means foreground (character)
    // glyphAlpha = 0.0 means background (empty space)
    float4 color = mix(in.background, in.foreground, glyphAlpha);

    return color;
}

// =============================================================================
// Alternative: Monochrome Fragment Shader
// =============================================================================

/// Alternative fragment shader for pure black/white fonts
/// Uses alpha channel for transparency
fragment float4 textFragmentShaderMonochrome(
    VertexOut in [[stage_in]],
    texture2d<float> fontAtlas [[texture(0)]],
    sampler fontSampler [[sampler(0)]]
) {
    // Sample the font atlas (alpha channel)
    float glyphAlpha = fontAtlas.sample(fontSampler, in.texCoord).a;

    // If no alpha, use red channel (for grayscale bitmaps)
    if (glyphAlpha == 0.0) {
        glyphAlpha = fontAtlas.sample(fontSampler, in.texCoord).r;
    }

    // Mix colors
    float4 color = mix(in.background, in.foreground, glyphAlpha);

    return color;
}

// =============================================================================
// Background-only shader (for optimization)
// =============================================================================

/// Fragment shader for rendering only backgrounds (no text)
/// Used for clearing or filling regions
fragment float4 backgroundFragmentShader(
    VertexOut in [[stage_in]]
) {
    return in.background;
}

// =============================================================================
// Debug Shaders
// =============================================================================

/// Debug vertex shader - shows grid
vertex VertexOut debugVertexShader(
    uint vertexID [[vertex_id]],
    constant VertexIn* vertices [[buffer(0)]],
    constant Uniforms& uniforms [[buffer(1)]]
) {
    VertexOut out;
    VertexIn in = vertices[vertexID];

    out.position = uniforms.projectionMatrix * float4(in.position, 0.0, 1.0);
    out.texCoord = in.texCoord;
    out.foreground = float4(1.0, 0.0, 1.0, 1.0);  // Magenta for debug
    out.background = float4(0.0, 0.0, 0.0, 1.0);

    return out;
}

/// Debug fragment shader - shows texture coordinates as colors
fragment float4 debugFragmentShader(
    VertexOut in [[stage_in]]
) {
    // Visualize texture coordinates
    return float4(in.texCoord.x, in.texCoord.y, 0.0, 1.0);
}

// =============================================================================
// Utility Functions
// =============================================================================

/// Convert RGBA8 (uint32_t) to float4
float4 rgba8ToFloat4(uint color) {
    float r = float((color >> 24) & 0xFF) / 255.0;
    float g = float((color >> 16) & 0xFF) / 255.0;
    float b = float((color >> 8) & 0xFF) / 255.0;
    float a = float(color & 0xFF) / 255.0;
    return float4(r, g, b, a);
}

/// Gamma correction for better text rendering
float4 gammaCorrect(float4 color, float gamma = 2.2) {
    return float4(
        pow(color.r, 1.0 / gamma),
        pow(color.g, 1.0 / gamma),
        pow(color.b, 1.0 / gamma),
        color.a
    );
}

// =============================================================================
// Advanced Fragment Shader with Subpixel Rendering
// =============================================================================

/// Advanced fragment shader with subpixel antialiasing
/// Provides better text clarity on LCD screens
fragment float4 textFragmentShaderSubpixel(
    VertexOut in [[stage_in]],
    texture2d<float> fontAtlas [[texture(0)]],
    sampler fontSampler [[sampler(0)]],
    constant Uniforms& uniforms [[buffer(1)]]
) {
    // Sample font atlas with slight offset for subpixel rendering
    float2 texelSize = 1.0 / uniforms.atlasSize;

    // Sample three times with subpixel offset (RGB channels)
    float r = fontAtlas.sample(fontSampler, in.texCoord + float2(-texelSize.x / 3.0, 0.0)).r;
    float g = fontAtlas.sample(fontSampler, in.texCoord).r;
    float b = fontAtlas.sample(fontSampler, in.texCoord + float2(texelSize.x / 3.0, 0.0)).r;

    // Create subpixel mask
    float3 subpixelAlpha = float3(r, g, b);

    // Mix with foreground/background per channel
    float4 color;
    color.r = mix(in.background.r, in.foreground.r, subpixelAlpha.r);
    color.g = mix(in.background.g, in.foreground.g, subpixelAlpha.g);
    color.b = mix(in.background.b, in.foreground.b, subpixelAlpha.b);
    color.a = 1.0;

    return color;
}

// =============================================================================
// Cursor Rendering
// =============================================================================

/// Fragment shader for rendering cursor
/// Inverts colors or uses specific cursor color
fragment float4 cursorFragmentShader(
    VertexOut in [[stage_in]],
    texture2d<float> fontAtlas [[texture(0)]],
    sampler fontSampler [[sampler(0)]],
    constant float& cursorAlpha [[buffer(2)]]
) {
    // Sample glyph
    float glyphAlpha = fontAtlas.sample(fontSampler, in.texCoord).r;
    float4 textColor = mix(in.background, in.foreground, glyphAlpha);

    // Cursor color (white with alpha for blinking)
    float4 cursorColor = float4(1.0, 1.0, 1.0, cursorAlpha);

    // Blend cursor over text
    float4 finalColor = mix(textColor, cursorColor, cursorColor.a);

    return finalColor;
}

// =============================================================================
// Effects Shaders
// =============================================================================

/// Fragment shader with CRT scanline effect
fragment float4 textFragmentShaderCRT(
    VertexOut in [[stage_in]],
    texture2d<float> fontAtlas [[texture(0)]],
    sampler fontSampler [[sampler(0)]],
    constant Uniforms& uniforms [[buffer(1)]]
) {
    // Sample glyph
    float glyphAlpha = fontAtlas.sample(fontSampler, in.texCoord).r;
    float4 color = mix(in.background, in.foreground, glyphAlpha);

    // Add scanlines (pixel position based)
    float scanline = sin(in.position.y * 3.14159 * 2.0) * 0.05 + 0.95;
    color.rgb *= scanline;

    // Add slight vignette
    float2 center = in.position.xy / float2(uniforms.cellSize * 80.0);  // Approximate screen size
    float vignette = 1.0 - length(center - 0.5) * 0.3;
    color.rgb *= vignette;

    return color;
}

/// Fragment shader with glow effect
fragment float4 textFragmentShaderGlow(
    VertexOut in [[stage_in]],
    texture2d<float> fontAtlas [[texture(0)]],
    sampler fontSampler [[sampler(0)]],
    constant Uniforms& uniforms [[buffer(1)]]
) {
    // Sample glyph
    float glyphAlpha = fontAtlas.sample(fontSampler, in.texCoord).r;

    // Sample surrounding pixels for glow
    float2 texelSize = uniforms.glyphSize / uniforms.atlasSize;
    float glow = 0.0;

    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float2 offset = float2(float(x), float(y)) * texelSize * 0.5;
            glow += fontAtlas.sample(fontSampler, in.texCoord + offset).r;
        }
    }
    glow /= 9.0;

    // Combine glyph and glow
    float finalAlpha = max(glyphAlpha, glow * 0.3);
    float4 color = mix(in.background, in.foreground, finalAlpha);

    return color;
}

// =============================================================================
// Particle Rendering Shaders
// =============================================================================

/// Particle instance data
struct ParticleInstance {
    float2 position;        ///< Particle position (x, y)
    float2 velocity;        ///< Particle velocity (vx, vy)
    float life;             ///< Remaining lifetime
    float maxLife;          ///< Initial lifetime
    uint color;             ///< RGBA color (packed)
    float size;             ///< Particle size (radius)
    bool active;            ///< Is particle active
};

/// Particle instance data for instanced quad rendering
struct ParticleInstanceData {
    float2 position;
    float2 texCoordMin;
    float2 texCoordMax;
    float4 color;
    float scale;
    float rotation;
    float alpha;
};

/// Particle vertex input (quad vertex)
struct ParticleVertex {
    float2 position;
    float2 texCoord;
};

/// Particle vertex output for instanced quads
struct ParticleVertexOut {
    float4 position [[position]];
    float2 texCoord;
    float4 color;
    float alpha;
};

/// Vertex shader for particle rendering (instanced quads)
vertex ParticleVertexOut particleVertexShader(
    uint vertexID [[vertex_id]],
    uint instanceID [[instance_id]],
    constant ParticleVertex* vertices [[buffer(0)]],
    constant Uniforms& uniforms [[buffer(1)]],
    constant ParticleInstanceData* instances [[buffer(2)]]
) {
    ParticleVertexOut out;

    ParticleVertex vert = vertices[vertexID];
    ParticleInstanceData instance = instances[instanceID];

    // Build model matrix exactly like sprites do (scale, rotate, translate)
    // Particle quad vertices are -1 to +1, scale by instance.scale
    float scale = instance.scale;
    float cosR = cos(instance.rotation);
    float sinR = sin(instance.rotation);

    // Build TRS matrix: Translation * Rotation * Scale
    float4x4 modelMatrix = float4x4(
        float4(cosR * scale, sinR * scale, 0, 0),
        float4(-sinR * scale, cosR * scale, 0, 0),
        float4(0, 0, 1, 0),
        float4(instance.position.x, instance.position.y, 0, 1)
    );

    // Apply model matrix to vertex (exactly like sprite_vertex does)
    float4 worldPos = modelMatrix * float4(vert.position, 0.0, 1.0);

    // Use same 1920x1080 logical coordinate space as sprites
    // But scale by 2x to account for Retina display (viewport is 3840x2116)
    float width = 1920.0f * 2.0f;
    float height = 1080.0f * 2.0f;
    float4x4 viewProjectionMatrix = float4x4(
        float4(2.0f / width, 0, 0, 0),
        float4(0, -2.0f / height, 0, 0),
        float4(0, 0, 1, 0),
        float4(-1, 1, 0, 1)
    );

    // Transform to clip space (exactly like sprite_vertex does)
    out.position = viewProjectionMatrix * worldPos;

    // Pass through texture coordinates
    out.texCoord = vert.texCoord;

    // Pass through color and alpha
    out.color = instance.color;
    out.alpha = instance.alpha;

    return out;
}

/// Fragment shader for particle rendering (instanced quads)
fragment float4 particleFragmentShader(
    ParticleVertexOut in [[stage_in]],
    texture2d<float> spriteTexture [[texture(0), function_constant(false)]],
    sampler spriteSampler [[sampler(0), function_constant(false)]]
) {
    // For POINT_SPRITE mode (no texture), just use the color
    // Create circular particle
    float2 coord = in.texCoord * 2.0 - 1.0;
    float dist = length(coord);

    // Smooth circle with antialiasing
    float circleAlpha = 1.0 - smoothstep(0.8, 1.0, dist);

    // Apply particle color and alpha
    float4 color = in.color;
    color.a *= circleAlpha * in.alpha;

    return color;
}
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
