//
//  star_shaders.metal
//  SuperTerminal Framework - GPU-Accelerated Star Rendering
//
//  Metal shaders for rendering stars with procedural vertex generation
//  Not quite working to anyones satisfaction.
//  Copyright © 2024-2025 Alban Read, SuperTerminal. All rights reserved.
//

#include <metal_stdlib>
using namespace metal;

// =============================================================================
// Structures
// =============================================================================

struct StarInstance {
    float2 position;        // Center position in pixels
    float outerRadius;      // Outer radius (to points)
    float innerRadius;      // Inner radius (between points)
    uint color1;            // Primary color (RGBA8888)
    uint color2;            // Secondary color
    uint mode;              // Gradient mode
    uint numPoints;         // Number of points (3-12)
    float rotation;         // Rotation in radians
    float param1;           // Pattern parameter 1
    float param2;           // Pattern parameter 2
};

struct StarUniforms {
    float screenWidth;
    float screenHeight;
    float2 padding;
};

struct StarVertexOut {
    float4 position [[position]];
    float2 localCoord;      // Local coordinate from star center
    float distFromCenter;   // Distance from center (for gradients)
    float pointIndex;       // Which point (for alternating colors)
    float outerRadius;
    float innerRadius;
    uint color1;
    uint color2;
    uint mode;
    uint numPoints;
    float param1;
    float param2;
};

// =============================================================================
// Helper Functions
// =============================================================================

// Convert RGBA8888 to float4 (format: 0xRRGGBBAA)
float4 unpackColor(uint rgba) {
    float r = float((rgba >> 24) & 0xFF) / 255.0;
    float g = float((rgba >> 16) & 0xFF) / 255.0;
    float b = float((rgba >> 8) & 0xFF) / 255.0;
    float a = float((rgba >> 0) & 0xFF) / 255.0;
    return float4(r, g, b, a);
}

// Rotate a 2D point
float2 rotate2D(float2 p, float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return float2(
        p.x * c - p.y * s,
        p.x * s + p.y * c
    );
}

// Calculate distance from point to line segment
float distanceToSegment(float2 p, float2 a, float2 b) {
    float2 pa = p - a;
    float2 ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}

// =============================================================================
// Vertex Shader - Procedural Triangle Fan Generation
// =============================================================================
//
// For an n-point star, we generate a triangle fan with:
//   - 1 center vertex (vertex 0)
//   - n outer vertices (points)
//   - n inner vertices (valleys between points)
//   - Total: 1 + 2n vertices
//
// Vertices alternate: center, outer[0], inner[0], outer[1], inner[1], ...
// This creates 2n triangles with NO overlap (zero overdraw)
//
// Example for 5-point star:
//   Vertex 0: center
//   Vertex 1: outer point 0 (angle 0°)
//   Vertex 2: inner valley 0 (angle 36°)
//   Vertex 3: outer point 1 (angle 72°)
//   Vertex 4: inner valley 1 (angle 108°)
//   ...
//   Vertex 11: repeat vertex 1 to close the fan
//
// Triangle strip: (0,1,2), (0,2,3), (0,3,4), (0,4,5), ...
//
// =============================================================================

vertex StarVertexOut star_vertex(
    uint vertexID [[vertex_id]],
    uint instanceID [[instance_id]],
    const device StarInstance* instances [[buffer(0)]],
    constant StarUniforms& uniforms [[buffer(1)]]
) {
    StarVertexOut out;

    // Get the star instance
    StarInstance star = instances[instanceID];

    int n = int(star.numPoints);

    // Calculate vertex position in local space (before rotation)
    float2 localPos;
    float vertexDist;
    float vertexPointIndex = 0.0;

    if (vertexID == 0) {
        // Center vertex
        localPos = float2(0.0, 0.0);
        vertexDist = 0.0;
        vertexPointIndex = 0.0;
    }
    else {
        // For triangle fan, we need 2n+1 vertices total
        // vertexID 1 to 2n are the perimeter vertices
        // vertexID 2n+1 wraps back to vertex 1 to close the fan

        int perimeterIndex = int(vertexID - 1);

        // Wrap the last vertex back to the first to close the fan
        if (perimeterIndex >= 2 * n) {
            perimeterIndex = 0;
        }

        // Determine if this is an outer point or inner valley
        bool isOuter = (perimeterIndex % 2) == 0;
        int pointNum = perimeterIndex / 2;

        // Calculate angle (start at top: -90 degrees)
        float anglePerPoint = (2.0 * M_PI_F) / float(n);
        float baseAngle = float(pointNum) * anglePerPoint - (M_PI_F * 0.5);

        float angle;
        float radius;

        if (isOuter) {
            // Outer point - exactly at the point angle
            angle = baseAngle;
            radius = star.outerRadius;
            vertexPointIndex = float(pointNum);
        }
        else {
            // Inner valley - halfway between points
            angle = baseAngle + (anglePerPoint * 0.5);
            radius = star.innerRadius;
            vertexPointIndex = float(pointNum) + 0.5;
        }

        // Calculate position
        localPos = float2(cos(angle) * radius, sin(angle) * radius);
        vertexDist = radius;
    }

    // Apply rotation to local position
    float2 rotatedPos = localPos;
    if (star.rotation != 0.0) {
        rotatedPos = rotate2D(localPos, star.rotation);
    }

    // Transform to world space
    float2 worldPos = star.position + rotatedPos;

    // Convert to NDC (Normalized Device Coordinates)
    float2 ndc = float2(
        (worldPos.x / uniforms.screenWidth) * 2.0 - 1.0,
        1.0 - (worldPos.y / uniforms.screenHeight) * 2.0
    );

    out.position = float4(ndc, 0.0, 1.0);
    out.localCoord = localPos;
    out.distFromCenter = vertexDist;
    out.pointIndex = vertexPointIndex;
    out.outerRadius = star.outerRadius;
    out.innerRadius = star.innerRadius;
    out.color1 = star.color1;
    out.color2 = star.color2;
    out.mode = star.mode;
    out.numPoints = star.numPoints;
    out.param1 = star.param1;
    out.param2 = star.param2;

    return out;
}

// =============================================================================
// Fragment Shader
// =============================================================================

fragment float4 star_fragment(
    StarVertexOut in [[stage_in]]
) {
    // Unpack colors
    float4 color1 = unpackColor(in.color1);
    float4 color2 = unpackColor(in.color2);

    float4 finalColor = color1;

    // Calculate distance from center for effects
    float dist = length(in.localCoord);

    // Apply gradient mode
    if (in.mode == 0) {
        // SOLID - just use color1
        finalColor = color1;
    }
    else if (in.mode == 1) {
        // RADIAL gradient (center to points)
        float t = smoothstep(0.0, in.outerRadius, dist);
        finalColor = mix(color1, color2, t);
    }
    else if (in.mode == 2) {
        // ALTERNATING colors per point
        int pointIdx = int(floor(in.pointIndex));
        if (pointIdx % 2 == 0) {
            finalColor = color1;
        } else {
            finalColor = color2;
        }
    }
    else if (in.mode == 100) {
        // OUTLINE mode
        // We need to detect edges of the star
        // For now, use a simple distance from edge approximation

        float lineWidth = in.param1;
        if (lineWidth <= 0.0) {
            lineWidth = 2.0;
        }

        // Calculate distance to nearest edge
        // For triangle fan geometry, check distance from perimeter
        float edgeDist = in.outerRadius - dist;

        // If we're near the outer edge, use outline color
        if (dist > in.outerRadius - lineWidth) {
            finalColor = color2;  // Outline color
        } else {
            finalColor = color1;  // Fill color
        }
    }
    else if (in.mode == 101) {
        // DASHED_OUTLINE
        float lineWidth = in.param1;
        float dashLength = in.param2;

        if (lineWidth <= 0.0) lineWidth = 2.0;
        if (dashLength <= 0.0) dashLength = 0.5;

        float edgeDist = in.outerRadius - dist;

        if (dist > in.outerRadius - lineWidth) {
            // We're in the outline region
            float angle = atan2(in.localCoord.y, in.localCoord.x);
            float normalizedAngle = (angle + M_PI_F) / (2.0 * M_PI_F);

            float numDashes = float(in.numPoints) * 4.0;
            float dashPattern = fmod(normalizedAngle * numDashes, 1.0);

            if (dashPattern < dashLength) {
                finalColor = color2;  // Dash
            } else {
                finalColor = color1;  // Gap
            }
        } else {
            finalColor = color1;  // Fill color
        }
    }

    // Smooth antialiasing at the perimeter
    // Calculate distance to edge for smooth blending
    float outerEdgeDist = in.outerRadius - dist;
    float aaWidth = 1.5;

    // Smooth alpha transition at outer boundary
    float alpha = smoothstep(-aaWidth, aaWidth, outerEdgeDist);

    // Also smooth at inner valleys if needed
    // (inner points might be very close to center for some star types)
    if (in.innerRadius > 0.0 && dist < in.innerRadius + aaWidth) {
        float innerEdgeDist = dist - (in.innerRadius * 0.5);
        alpha *= smoothstep(0.0, aaWidth, innerEdgeDist);
    }

    finalColor.a *= alpha;

    // Premultiply alpha for correct blending
    finalColor.rgb *= finalColor.a;

    return finalColor;
}
