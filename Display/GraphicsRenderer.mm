//
// GraphicsRenderer.mm
// SuperTerminal v2
//
// Graphics renderer with double buffering and dirty tracking for optimal performance.
//

#include "GraphicsRenderer.h"
#include "GraphicsLayer.h"
#include "../API/st_api_context.h"
#include <CoreGraphics/CoreGraphics.h>
#include <Metal/Metal.h>
#include <chrono>

namespace SuperTerminal {

/// GraphicsRenderer: Renders GraphicsLayer commands using CoreGraphics (retained mode)
class GraphicsRenderer {
public:
    GraphicsRenderer(id<MTLDevice> device, uint32_t width, uint32_t height, CGFloat contentScale = 1.0)
        : m_device(device)
        , m_width(width)
        , m_height(height)
        , m_contentScale(contentScale)
        , m_texture(nil)
        , m_context(nullptr)
        , m_bitmapData(nullptr)
        , m_isDirty(false)
        , m_hasContent(false)
        , m_dirtyRect(CGRectMake(0, 0, 0, 0))
        , m_commandsRendered(0)
        , m_commandsCulled(0)
        , m_totalFrames(0)
        , m_dirtyFrames(0)
        , m_totalRenderTime(0.0)
        , m_totalUploadTime(0.0)
    {
        NSLog(@"[GraphicsRenderer] Created with contentScale=%.1f (line widths will be scaled)", m_contentScale);
        createBuffers();
        clearBuffer();
    }

    ~GraphicsRenderer() {
        cleanup();
    }

    void resize(uint32_t width, uint32_t height) {
        if (width == m_width && height == m_height) {
            return;
        }

        cleanup();
        m_width = width;
        m_height = height;
        createBuffers();
        clearBuffer();
        m_isDirty = true;  // Force update after resize
    }

    void render(GraphicsLayer& layer) {
        // Check if we have any commands to render
        if (!m_context || layer.isEmpty()) {
            NSLog(@"[GraphicsRenderer] Skipping render: context=%p isEmpty=%d",
                  m_context, layer.isEmpty());
            return;
        }

        // Get commands (copy the vector)
        auto commands = layer.getCommands();

        if (commands.empty()) {
            NSLog(@"[GraphicsRenderer] No commands to render");
            return;
        }

        NSLog(@"[GraphicsRenderer] Rendering %zu commands", commands.size());

        // Performance metrics
        auto renderStartTime = std::chrono::high_resolution_clock::now();

        // Mark as dirty since we have new commands to render
        m_isDirty = true;
        m_hasContent = true;  // Now we have graphics content to display

        // Clear the context before rendering new commands
        CGContextClearRect(m_context, CGRectMake(0, 0, m_width, m_height));

        // Set default rendering properties
        CGContextSetBlendMode(m_context, kCGBlendModeNormal);
        CGContextSetShouldAntialias(m_context, true);
        CGContextSetAllowsAntialiasing(m_context, true);
        CGContextSetInterpolationQuality(m_context, kCGInterpolationHigh);

        // Save state once for all commands and set clipping region
        CGContextSaveGState(m_context);
        CGContextClipToRect(m_context, CGRectMake(0, 0, m_width, m_height));

        // Reset dirty rect tracking
        m_dirtyRect = CGRectMake(INFINITY, INFINITY, 0, 0);
        m_commandsRendered = 0;
        m_commandsCulled = 0;

        // Define viewport for culling
        CGRect viewport = CGRectMake(0, 0, m_width, m_height);

        // Render all commands (batched) with culling
        for (const auto& cmd : commands) {
            CGRect cmdBounds = getCommandBounds(cmd);

            // Cull offscreen commands (optimization)
            if (CGRectIntersectsRect(cmdBounds, viewport)) {
                renderCommand(cmd);

                // Expand dirty rect to include this command
                if (CGRectIsEmpty(m_dirtyRect) || CGRectEqualToRect(m_dirtyRect, CGRectMake(INFINITY, INFINITY, 0, 0))) {
                    m_dirtyRect = cmdBounds;
                } else {
                    m_dirtyRect = CGRectUnion(m_dirtyRect, cmdBounds);
                }

                m_commandsRendered++;
            } else {
                m_commandsCulled++;
            }
        }

        // Restore state once after all commands
        CGContextRestoreGState(m_context);

        // Upload the rendered content to the texture
        uploadTexture();

        // Clamp dirty rect to viewport bounds
        if (!CGRectIsEmpty(m_dirtyRect) && !CGRectEqualToRect(m_dirtyRect, CGRectMake(INFINITY, INFINITY, 0, 0))) {
            m_dirtyRect = CGRectIntersection(m_dirtyRect, viewport);
        }

        // Calculate render time
        auto renderEndTime = std::chrono::high_resolution_clock::now();
        double renderTimeMs = std::chrono::duration<double, std::milli>(renderEndTime - renderStartTime).count();
        m_totalRenderTime += renderTimeMs;

        // Log culling statistics periodically
        static int renderCount = 0;
        if (++renderCount % 60 == 0) {
            NSLog(@"[GraphicsRenderer] Rendered %d commands, culled %d (%.1f%% saved), avg render: %.3fms",
                  m_commandsRendered, m_commandsCulled,
                  m_commandsCulled * 100.0 / (m_commandsRendered + m_commandsCulled),
                  m_totalRenderTime / renderCount);
        }

        // Don't auto-swap - let SWAP command control when buffers are swapped

        NSLog(@"[GraphicsRenderer] Render complete: %d commands rendered, %d culled",
              m_commandsRendered, m_commandsCulled);

        // CRITICAL FIX: Upload the back buffer texture immediately after rendering
        // This ensures the GPU texture is up-to-date when Metal composites it
        NSLog(@"[GraphicsRenderer] Uploading back buffer to Metal texture...");
        uploadTexture();
        NSLog(@"[GraphicsRenderer] Upload complete");
    }

    void getMetrics(STGraphicsMetrics* metrics) const {
        if (!metrics) return;

        metrics->totalFrames = m_totalFrames;
        metrics->dirtyFrames = m_dirtyFrames;
        metrics->commandsRendered = m_commandsRendered;
        metrics->commandsCulled = m_commandsCulled;

        if (m_dirtyFrames > 0) {
            metrics->avgRenderTimeMs = m_totalRenderTime / m_dirtyFrames;
            metrics->avgUploadTimeMs = m_totalUploadTime / m_dirtyFrames;
        } else {
            metrics->avgRenderTimeMs = 0.0;
            metrics->avgUploadTimeMs = 0.0;
        }

        if (m_totalFrames > 0) {
            metrics->dirtyFrameRatio = (double)m_dirtyFrames / m_totalFrames;
        } else {
            metrics->dirtyFrameRatio = 0.0;
        }

        uint64_t totalCommands = m_commandsRendered + m_commandsCulled;
        if (totalCommands > 0) {
            metrics->cullRatio = (double)m_commandsCulled / totalCommands;
        } else {
            metrics->cullRatio = 0.0;
        }
    }

    id<MTLTexture> getTexture() const {
        // Return the back texture which contains the rendered graphics
        // (we render to back and upload it immediately, so back has the latest content)
        return m_texture;
    }

    bool isDirty() const {
        return m_isDirty;
    }

    void markClean() {
        m_isDirty = false;

    }

    bool hasContent() const {
        return m_hasContent;
    }

    void clear() {
        if (!m_context) return;

        // Clear buffer to transparent
        CGContextSaveGState(m_context);
        CGContextSetBlendMode(m_context, kCGBlendModeClear);
        CGContextFillRect(m_context, CGRectMake(0, 0, m_width, m_height));
        CGContextRestoreGState(m_context);

        m_isDirty = true;
        m_hasContent = false;  // Cleared, so no content to display
    }

    void swapBuffersExplicit() {
        // No-op: Single buffer mode, just ensure texture is uploaded
        if (!m_context) {
            NSLog(@"[GraphicsRenderer] Cannot swap - buffer not initialized");
            return;
        }

        // Track metrics
        m_totalFrames++;
        m_dirtyFrames++;

        // Upload texture
        uploadTexture();

        NSLog(@"[GraphicsRenderer] Explicit SWAP complete (single buffer mode)");

        // Mark as dirty
        m_isDirty = true;
    }

private:
    void createBuffers() {
        m_texture = createBuffer(&m_bitmapData, &m_context);

        if (!m_context) {
            NSLog(@"GraphicsRenderer: Failed to create buffer");
        }
    }

    id<MTLTexture> createBuffer(uint8_t** bitmapData, CGContextRef* context) {
        // Allocate bitmap data (RGBA, 4 bytes per pixel)
        size_t bytesPerRow = m_width * 4;
        size_t dataSize = bytesPerRow * m_height;
        *bitmapData = (uint8_t*)malloc(dataSize);
        // Initialize to transparent (all zeros for BGRA with premultiplied alpha)
        memset(*bitmapData, 0, dataSize);

        // Create bitmap context (BGRA format to match Metal texture)
        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        *context = CGBitmapContextCreate(
            *bitmapData,
            m_width,
            m_height,
            8,
            bytesPerRow,
            colorSpace,
            kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little
        );
        CGColorSpaceRelease(colorSpace);

        if (!*context) {
            NSLog(@"GraphicsRenderer: Failed to create CGContext");
            return nil;
        }

        // Flip coordinate system to match Metal (origin at top-left)
        CGContextTranslateCTM(*context, 0, m_height);
        CGContextScaleCTM(*context, 1.0, -1.0);

        // Set context to render with transparent background
        CGContextSetBlendMode(*context, kCGBlendModeNormal);
        CGContextClearRect(*context, CGRectMake(0, 0, m_width, m_height));

        // Create Metal texture (BGRA to match pipeline format)
        MTLTextureDescriptor* descriptor = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
            width:m_width
            height:m_height
            mipmapped:NO];
        descriptor.usage = MTLTextureUsageShaderRead;

        id<MTLTexture> texture = [m_device newTextureWithDescriptor:descriptor];
        if (!texture) {
            NSLog(@"GraphicsRenderer: Failed to create Metal texture");
        }
        return texture;
    }

    void swapBuffers() {
        // No-op: Single buffer mode, just upload texture
        uploadTexture();
    }

    void uploadTexture() {
        if (!m_texture || !m_bitmapData) {
            NSLog(@"[GraphicsRenderer] uploadTexture: SKIPPED - texture=%p, data=%p", m_texture, m_bitmapData);
            return;
        }

        static int uploadCount = 0;
        uploadCount++;

        auto uploadStartTime = std::chrono::high_resolution_clock::now();
        NSUInteger bytesPerRow = m_width * 4;

        // Sample first few pixels to verify data
        if (uploadCount <= 5 || uploadCount % 60 == 0) {
            const uint32_t* pixels = (const uint32_t*)m_bitmapData;
            NSLog(@"[GraphicsRenderer] uploadTexture #%d: First 4 pixels: 0x%08X 0x%08X 0x%08X 0x%08X",
                  uploadCount, pixels[0], pixels[1], pixels[2], pixels[3]);
            // Sample pixel at (100, 100) where rectangle should be
            uint32_t pixelAt100 = pixels[100 * m_width + 100];
            NSLog(@"[GraphicsRenderer] Pixel at (100,100): 0x%08X (should be magenta if rect drawn)", pixelAt100);
        }

        // Optimization: Upload only dirty region if it's significantly smaller than full texture
        if (!CGRectIsEmpty(m_dirtyRect) &&
            !CGRectEqualToRect(m_dirtyRect, CGRectMake(INFINITY, INFINITY, 0, 0))) {

            // Expand dirty rect slightly to account for antialiasing
            CGRect expandedRect = CGRectInset(m_dirtyRect, -2, -2);

            // Clamp to texture bounds
            expandedRect = CGRectIntersection(expandedRect, CGRectMake(0, 0, m_width, m_height));

            // Calculate region size
            CGFloat dirtyArea = expandedRect.size.width * expandedRect.size.height;
            CGFloat totalArea = m_width * m_height;
            CGFloat dirtyRatio = dirtyArea / totalArea;

            // Only use partial update if dirty region is < 50% of texture
            if (dirtyRatio < 0.5) {
                NSUInteger x = (NSUInteger)expandedRect.origin.x;
                NSUInteger y = (NSUInteger)expandedRect.origin.y;
                NSUInteger w = (NSUInteger)expandedRect.size.width;
                NSUInteger h = (NSUInteger)expandedRect.size.height;

                MTLRegion region = MTLRegionMake2D(x, y, w, h);

                // Calculate offset into bitmap data
                const uint8_t* basePtr = (const uint8_t*)m_bitmapData;
                const uint8_t* dirtyPtr = basePtr + (y * bytesPerRow) + (x * 4);

                [m_texture replaceRegion:region
                                 mipmapLevel:0
                                   withBytes:dirtyPtr
                                 bytesPerRow:bytesPerRow];

                // Calculate upload time
                auto uploadEndTime = std::chrono::high_resolution_clock::now();
                double uploadTimeMs = std::chrono::duration<double, std::milli>(uploadEndTime - uploadStartTime).count();
                m_totalUploadTime += uploadTimeMs;

                // Log partial update statistics
                static int partialUpdateCount = 0;
                if (++partialUpdateCount % 60 == 0) {
                    NSLog(@"[GraphicsRenderer] Partial update: %.1f%% of texture (%.0fx%.0f of %dx%d), avg upload: %.3fms",
                          dirtyRatio * 100.0, w, h, m_width, m_height, m_totalUploadTime / partialUpdateCount);
                }

                return;
            }
        }

        // Fallback: Upload full texture
        MTLRegion region = MTLRegionMake2D(0, 0, m_width, m_height);
        [m_texture replaceRegion:region
                         mipmapLevel:0
                           withBytes:m_bitmapData
                         bytesPerRow:bytesPerRow];

        static int fullUploadCount = 0;
        if (++fullUploadCount <= 5 || fullUploadCount % 60 == 0) {
            NSLog(@"[GraphicsRenderer] Full texture upload #%d: %dx%d pixels (%lu bytes)",
                  fullUploadCount, m_width, m_height, (unsigned long)(m_width * m_height * 4));
        }

        // Calculate upload time
        auto uploadEndTime = std::chrono::high_resolution_clock::now();
        double uploadTimeMs = std::chrono::duration<double, std::milli>(uploadEndTime - uploadStartTime).count();
        m_totalUploadTime += uploadTimeMs;
    }



    void clearBuffer() {
        if (m_context) {
            // Clear buffer to transparent
            CGContextSaveGState(m_context);
            CGContextSetBlendMode(m_context, kCGBlendModeClear);
            CGContextFillRect(m_context, CGRectMake(0, 0, m_width, m_height));
            CGContextRestoreGState(m_context);
        }
        m_isDirty = true;
        m_hasContent = false;  // Buffer cleared, no content
    }

    void cleanup() {
        if (m_context) {
            CGContextRelease(m_context);
            m_context = nullptr;
        }
        if (m_bitmapData) {
            free(m_bitmapData);
            m_bitmapData = nullptr;
        }
        m_texture = nil;
    }

    CGRect getCommandBounds(const GraphicsCommand& cmd) const {
        switch (cmd.type) {
            case DrawCommand::FillRect:
                return CGRectMake(cmd.x, cmd.y, cmd.width, cmd.height);

            case DrawCommand::DrawRect: {
                // Account for line width
                float halfWidth = cmd.lineWidth * 0.5f;
                return CGRectMake(cmd.x - halfWidth, cmd.y - halfWidth,
                                cmd.width + cmd.lineWidth, cmd.height + cmd.lineWidth);
            }

            case DrawCommand::FillCircle:
                return CGRectMake(cmd.x - cmd.radius, cmd.y - cmd.radius,
                                cmd.radius * 2, cmd.radius * 2);

            case DrawCommand::DrawCircle: {
                float halfWidth = cmd.lineWidth * 0.5f;
                float radius = cmd.radius + halfWidth;
                return CGRectMake(cmd.x - radius, cmd.y - radius,
                                radius * 2, radius * 2);
            }

            case DrawCommand::DrawLine: {
                float halfWidth = cmd.lineWidth * 0.5f;
                float minX = fmin(cmd.x, cmd.x2) - halfWidth;
                float minY = fmin(cmd.y, cmd.y2) - halfWidth;
                float maxX = fmax(cmd.x, cmd.x2) + halfWidth;
                float maxY = fmax(cmd.y, cmd.y2) + halfWidth;
                return CGRectMake(minX, minY, maxX - minX, maxY - minY);
            }

            case DrawCommand::DrawPixel:
                return CGRectMake(cmd.x, cmd.y, 1, 1);

            default:
                return CGRectMake(0, 0, m_width, m_height);
        }
    }

    void renderCommand(const GraphicsCommand& cmd) {
        if (!m_context) return;

        switch (cmd.type) {
            case DrawCommand::FillRect: {
                CGRect rect = CGRectMake(cmd.x, cmd.y, cmd.width, cmd.height);
                static int fillRectCount = 0;
                if (++fillRectCount <= 5 || fillRectCount % 60 == 0) {
                    NSLog(@"[GraphicsRenderer] FillRect: x=%.1f y=%.1f w=%.1f h=%.1f rgba=(%.2f,%.2f,%.2f,%.2f) count=%d",
                          cmd.x, cmd.y, cmd.width, cmd.height, cmd.r, cmd.g, cmd.b, cmd.a, fillRectCount);
                }
                // Use CGColor for proper alpha handling with premultiplied alpha context
                CGColorRef color = CGColorCreateGenericRGB(cmd.r, cmd.g, cmd.b, cmd.a);
                CGContextSetFillColorWithColor(m_context, color);
                CGContextFillRect(m_context, rect);
                CGColorRelease(color);
                break;
            }

            case DrawCommand::DrawRect: {
                CGRect rect = CGRectMake(cmd.x, cmd.y, cmd.width, cmd.height);
                // Use CGColor for proper alpha handling with premultiplied alpha context
                CGColorRef color = CGColorCreateGenericRGB(cmd.r, cmd.g, cmd.b, cmd.a);
                CGContextSetStrokeColorWithColor(m_context, color);
                CGContextSetLineWidth(m_context, cmd.lineWidth * m_contentScale);
                CGContextStrokeRect(m_context, rect);
                CGColorRelease(color);
                break;
            }

            case DrawCommand::FillCircle: {
                CGRect rect = CGRectMake(cmd.x - cmd.radius, cmd.y - cmd.radius,
                                       cmd.radius * 2, cmd.radius * 2);
                // Use CGColor for proper alpha handling with premultiplied alpha context
                CGColorRef color = CGColorCreateGenericRGB(cmd.r, cmd.g, cmd.b, cmd.a);
                CGContextSetFillColorWithColor(m_context, color);
                CGContextFillEllipseInRect(m_context, rect);
                CGColorRelease(color);
                break;
            }

            case DrawCommand::DrawCircle: {
                CGRect rect = CGRectMake(cmd.x - cmd.radius, cmd.y - cmd.radius,
                                       cmd.radius * 2, cmd.radius * 2);
                // Use CGColor for proper alpha handling with premultiplied alpha context
                CGColorRef color = CGColorCreateGenericRGB(cmd.r, cmd.g, cmd.b, cmd.a);
                CGContextSetStrokeColorWithColor(m_context, color);
                CGContextSetLineWidth(m_context, cmd.lineWidth * m_contentScale);
                CGContextStrokeEllipseInRect(m_context, rect);
                CGColorRelease(color);
                break;
            }

            case DrawCommand::DrawLine: {
                // Use CGColor for proper alpha handling with premultiplied alpha context
                CGColorRef color = CGColorCreateGenericRGB(cmd.r, cmd.g, cmd.b, cmd.a);
                CGContextSetStrokeColorWithColor(m_context, color);
                CGContextSetLineWidth(m_context, cmd.lineWidth * m_contentScale);
                CGContextMoveToPoint(m_context, cmd.x, cmd.y);
                CGContextAddLineToPoint(m_context, cmd.x2, cmd.y2);
                CGContextStrokePath(m_context);
                CGColorRelease(color);
                break;
            }

            case DrawCommand::DrawPixel: {
                CGRect rect = CGRectMake(cmd.x, cmd.y, 1, 1);
                // Use CGColor for proper alpha handling with premultiplied alpha context
                CGColorRef color = CGColorCreateGenericRGB(cmd.r, cmd.g, cmd.b, cmd.a);
                CGContextSetFillColorWithColor(m_context, color);
                CGContextFillRect(m_context, rect);
                CGColorRelease(color);
                break;
            }

            default:
                break;
        }
    }

private:
    id<MTLDevice> m_device;
    uint32_t m_width;
    uint32_t m_height;
    CGFloat m_contentScale;

    // Single buffer (retained mode)
    id<MTLTexture> m_texture;
    CGContextRef m_context;
    uint8_t* m_bitmapData;

    // Dirty tracking
    bool m_isDirty;
    bool m_hasContent;
    // Optimization: Partial texture updates
    CGRect m_dirtyRect;  // Bounding box of all rendered commands

    // Statistics
    int m_commandsRendered;
    int m_commandsCulled;
    uint64_t m_totalFrames;
    uint64_t m_dirtyFrames;
    double m_totalRenderTime;
    double m_totalUploadTime;
};

} // namespace SuperTerminal

// C interface for the GraphicsRenderer
struct STGraphicsRenderer {
    SuperTerminal::GraphicsRenderer* renderer;
};

STGraphicsRenderer* st_graphics_renderer_create(id<MTLDevice> device, uint32_t width, uint32_t height) {
    STGraphicsRenderer* wrapper = new STGraphicsRenderer;
    wrapper->renderer = new SuperTerminal::GraphicsRenderer(device, width, height, 1.0);
    return wrapper;
}

STGraphicsRenderer* st_graphics_renderer_create_with_scale(id<MTLDevice> device, uint32_t width, uint32_t height, CGFloat contentScale) {
    STGraphicsRenderer* wrapper = new STGraphicsRenderer;
    wrapper->renderer = new SuperTerminal::GraphicsRenderer(device, width, height, contentScale);
    return wrapper;
}

void st_graphics_renderer_destroy(STGraphicsRenderer* renderer) {
    if (renderer) {
        delete renderer->renderer;
        delete renderer;
    }
}

void st_graphics_renderer_render(STGraphicsRenderer* renderer, void* graphicsLayer) {
    if (renderer && renderer->renderer && graphicsLayer) {
        SuperTerminal::GraphicsLayer* layer = static_cast<SuperTerminal::GraphicsLayer*>(graphicsLayer);
        renderer->renderer->render(*layer);
    }
}

id<MTLTexture> st_graphics_renderer_get_texture(STGraphicsRenderer* renderer) {
    if (renderer && renderer->renderer) {
        return renderer->renderer->getTexture();
    }
    return nil;
}

bool st_graphics_renderer_is_dirty(STGraphicsRenderer* renderer) {
    if (renderer && renderer->renderer) {
        return renderer->renderer->isDirty();
    }
    return false;
}

bool st_graphics_renderer_has_content(STGraphicsRenderer* renderer) {
    if (renderer && renderer->renderer) {
        return renderer->renderer->hasContent();
    }
    return false;
}

void st_graphics_renderer_mark_clean(STGraphicsRenderer* renderer) {
    if (renderer && renderer->renderer) {
        renderer->renderer->markClean();
    }
}

void st_graphics_renderer_clear(STGraphicsRenderer* renderer) {
    if (renderer && renderer->renderer) {
        renderer->renderer->clear();
    }
}

void st_graphics_renderer_resize(STGraphicsRenderer* renderer, uint32_t width, uint32_t height) {
    if (renderer && renderer->renderer) {
        renderer->renderer->resize(width, height);
    }
}

void st_graphics_renderer_swap(STGraphicsRenderer* renderer) {
    if (renderer && renderer->renderer) {
        renderer->renderer->swapBuffersExplicit();
    }
}

void st_graphics_renderer_get_metrics(STGraphicsRenderer* renderer, STGraphicsMetrics* metrics) {
    if (renderer && renderer->renderer && metrics) {
        renderer->renderer->getMetrics(metrics);
    }
}
