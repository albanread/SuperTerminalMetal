//
// TextDisplayManager.mm
// SuperTerminal Framework - Free-form Text Display System Implementation
//
// Manages overlay text rendering with transformations (scale, rotation, shear)
// at arbitrary pixel positions. Renders on top of all other content.
// For game messages etc, in their own layer.

#import "TextDisplayManager.h"
#import "../Metal/FontAtlas.h"
#import <Metal/Metal.h>
#import <simd/simd.h>
#import <cmath>
#import <algorithm>

namespace SuperTerminal {

// =============================================================================
// Private Implementation Structure
// =============================================================================

struct TextDisplayManager::Impl {
    MTLDevicePtr device;
    std::shared_ptr<FontAtlas> fontAtlas;

    // Text items storage
    std::vector<TextDisplayItem> textItems;
    std::vector<int> textItemIds;
    int nextItemId;

    // Vertex data
    std::vector<TransformedTextVertex> vertices;
    std::vector<TextEffectVertex> effectVertices;
    MTLBufferPtr vertexBuffer;
    MTLBufferPtr effectVertexBuffer;
    size_t vertexBufferSize;
    size_t effectVertexBufferSize;
    bool needsVertexRebuild;
    bool hasEffects;

    // Viewport info for coordinate conversion
    uint32_t viewportWidth;
    uint32_t viewportHeight;

    Impl(MTLDevicePtr dev)
        : device(dev)
        , nextItemId(1)
        , vertexBuffer(nil)
        , effectVertexBuffer(nil)
        , vertexBufferSize(0)
        , effectVertexBufferSize(0)
        , needsVertexRebuild(true)
        , hasEffects(false)
        , viewportWidth(1920)
        , viewportHeight(1080)
    {}
};

// =============================================================================
// Constructor / Destructor
// =============================================================================

TextDisplayManager::TextDisplayManager(MTLDevicePtr device)
    : m_impl(std::make_unique<Impl>(device))
{
    NSLog(@"[TextDisplayManager] Initialized");
}

TextDisplayManager::~TextDisplayManager() {
    if (m_impl) {
        if (m_impl->vertexBuffer) {
            m_impl->vertexBuffer = nil;
        }
        if (m_impl->effectVertexBuffer) {
            m_impl->effectVertexBuffer = nil;
        }
    }
}

TextDisplayManager::TextDisplayManager(TextDisplayManager&&) noexcept = default;
TextDisplayManager& TextDisplayManager::operator=(TextDisplayManager&&) noexcept = default;

// =============================================================================
// Text Display API
// =============================================================================

int TextDisplayManager::displayTextAt(float x, float y, const std::string& text,
                                      float scaleX, float scaleY, float rotation,
                                      uint32_t color, TextAlignment alignment, int layer) {
    return displayTextAtWithShear(x, y, text, scaleX, scaleY, rotation, 0.0f, 0.0f,
                                  color, alignment, layer);
}

int TextDisplayManager::displayTextWithEffects(float x, float y, const std::string& text,
                                               float scaleX, float scaleY, float rotation,
                                               uint32_t color, TextAlignment alignment, int layer,
                                               TextEffect effect, uint32_t effectColor,
                                               float effectIntensity, float effectSize) {
    if (!m_impl || text.empty()) {
        return -1;
    }

    TextDisplayItem item;
    item.text = text;
    item.x = x;
    item.y = y;
    item.scaleX = scaleX;
    item.scaleY = scaleY;
    item.rotation = rotation;
    item.shearX = 0.0f;
    item.shearY = 0.0f;
    item.color = color;
    item.alignment = alignment;
    item.layer = layer;
    item.visible = true;
    item.effect = effect;
    item.effectColor = effectColor;
    item.effectIntensity = effectIntensity;
    item.effectSize = effectSize;
    item.animationTime = 0.0f;

    int itemId = generateItemId();

    m_impl->textItems.push_back(item);
    m_impl->textItemIds.push_back(itemId);
    m_impl->needsVertexRebuild = true;

    NSLog(@"[TextDisplayManager] Added text item %d with effect %d: \"%s\" at (%.1f,%.1f)",
          itemId, (int)effect, text.c_str(), x, y);

    return itemId;
}

int TextDisplayManager::displayTextAtWithShear(float x, float y, const std::string& text,
                                               float scaleX, float scaleY, float rotation,
                                               float shearX, float shearY,
                                               uint32_t color, TextAlignment alignment, int layer) {
    if (!m_impl || text.empty()) {
        return -1;
    }

    TextDisplayItem item;
    item.text = text;
    item.x = x;
    item.y = y;
    item.scaleX = scaleX;
    item.scaleY = scaleY;
    item.rotation = rotation;
    item.shearX = shearX;
    item.shearY = shearY;
    item.color = color;
    item.alignment = alignment;
    item.layer = layer;
    item.visible = true;

    int itemId = generateItemId();

    m_impl->textItems.push_back(item);
    m_impl->textItemIds.push_back(itemId);
    m_impl->needsVertexRebuild = true;

    NSLog(@"[TextDisplayManager] Added text item %d: \"%s\" at (%.1f,%.1f)",
          itemId, text.c_str(), x, y);

    return itemId;
}

bool TextDisplayManager::updateTextItem(int itemId, const std::string& text, float x, float y) {
    if (!m_impl) return false;

    for (size_t i = 0; i < m_impl->textItemIds.size(); ++i) {
        if (m_impl->textItemIds[i] == itemId) {
            TextDisplayItem& item = m_impl->textItems[i];

            if (!text.empty()) {
                item.text = text;
            }
            if (x >= 0) {
                item.x = x;
            }
            if (y >= 0) {
                item.y = y;
            }

            m_impl->needsVertexRebuild = true;
            return true;
        }
    }
    return false;
}

bool TextDisplayManager::removeTextItem(int itemId) {
    if (!m_impl) return false;

    for (size_t i = 0; i < m_impl->textItemIds.size(); ++i) {
        if (m_impl->textItemIds[i] == itemId) {
            m_impl->textItems.erase(m_impl->textItems.begin() + i);
            m_impl->textItemIds.erase(m_impl->textItemIds.begin() + i);
            m_impl->needsVertexRebuild = true;
            return true;
        }
    }
    return false;
}

void TextDisplayManager::clearDisplayedText() {
    if (!m_impl) return;

    m_impl->textItems.clear();
    m_impl->textItemIds.clear();
    m_impl->vertices.clear();
    m_impl->effectVertices.clear();
    m_impl->needsVertexRebuild = true;

    NSLog(@"[TextDisplayManager] Cleared all displayed text");
}

bool TextDisplayManager::setTextItemVisible(int itemId, bool visible) {
    if (!m_impl) return false;

    for (size_t i = 0; i < m_impl->textItemIds.size(); ++i) {
        if (m_impl->textItemIds[i] == itemId) {
            m_impl->textItems[i].visible = visible;
            m_impl->needsVertexRebuild = true;
            return true;
        }
    }
    return false;
}

bool TextDisplayManager::setTextItemLayer(int itemId, int layer) {
    if (!m_impl) return false;

    for (size_t i = 0; i < m_impl->textItemIds.size(); ++i) {
        if (m_impl->textItemIds[i] == itemId) {
            m_impl->textItems[i].layer = layer;
            m_impl->needsVertexRebuild = true;
            return true;
        }
    }
    return false;
}

bool TextDisplayManager::setTextItemColor(int itemId, uint32_t color) {
    if (!m_impl) return false;

    for (size_t i = 0; i < m_impl->textItemIds.size(); ++i) {
        if (m_impl->textItemIds[i] == itemId) {
            m_impl->textItems[i].color = color;
            m_impl->needsVertexRebuild = true;
            return true;
        }
    }
    return false;
}

// =============================================================================
// Rendering Integration
// =============================================================================

void TextDisplayManager::setFontAtlas(std::shared_ptr<FontAtlas> fontAtlas) {
    if (!m_impl) return;

    m_impl->fontAtlas = fontAtlas;
    m_impl->needsVertexRebuild = true;
}

void TextDisplayManager::buildVertexBuffers(uint32_t viewportWidth, uint32_t viewportHeight) {
    if (!m_impl || !m_impl->fontAtlas || !m_impl->needsVertexRebuild) {
        return;
    }

    m_impl->viewportWidth = viewportWidth;
    m_impl->viewportHeight = viewportHeight;
    m_impl->vertices.clear();
    m_impl->effectVertices.clear();
    m_impl->hasEffects = false;

    // Sort items by layer for proper z-ordering
    sortTextItemsByLayer();

    // Build vertices for all visible text items
    for (const auto& item : m_impl->textItems) {
        if (!item.visible || item.text.empty()) {
            continue;
        }

        // Check if this item has effects
        bool hasEffect = (item.effect != TextEffect::NONE);
        if (hasEffect) {
            m_impl->hasEffects = true;
        }

        // Calculate text dimensions and alignment offset
        float textWidth, textHeight;
        calculateTextDimensions(item.text, textWidth, textHeight);

        float alignOffsetX, alignOffsetY;
        calculateAlignmentOffset(item.text, item.alignment, alignOffsetX, alignOffsetY);

        // Calculate transformation matrix
        float transformMatrix[16];
        calculateTransformMatrix(item, transformMatrix);

        // Extract color components
        float r = ((item.color >> 24) & 0xFF) / 255.0f;
        float g = ((item.color >> 16) & 0xFF) / 255.0f;
        float b = ((item.color >> 8) & 0xFF) / 255.0f;
        float a = (item.color & 0xFF) / 255.0f;

        // Extract effect color components
        float er = ((item.effectColor >> 24) & 0xFF) / 255.0f;
        float eg = ((item.effectColor >> 16) & 0xFF) / 255.0f;
        float eb = ((item.effectColor >> 8) & 0xFF) / 255.0f;
        float ea = (item.effectColor & 0xFF) / 255.0f;

        // Generate vertices for each character
        float currentX = alignOffsetX;

        for (size_t charIdx = 0; charIdx < item.text.length(); ++charIdx) {
            uint32_t codepoint = static_cast<uint32_t>(item.text[charIdx]);

            if (!m_impl->fontAtlas->hasGlyph(codepoint)) {
                // Skip unavailable characters
                currentX += m_impl->fontAtlas->getGlyphWidth();
                continue;
            }

            // Get glyph metrics and texture coordinates
            auto metrics = m_impl->fontAtlas->getGlyphMetrics(codepoint);

            // Character quad vertices (before transformation)
            float x1 = currentX;
            float y1 = alignOffsetY;
            float x2 = currentX + metrics.width;
            float y2 = alignOffsetY + metrics.height;

            // Transform vertices
            float tx1, ty1, tx2, ty2, tx3, ty3, tx4, ty4;
            transformVertex(x1, y1, transformMatrix, tx1, ty1);
            transformVertex(x2, y1, transformMatrix, tx2, ty2);
            transformVertex(x2, y2, transformMatrix, tx3, ty3);
            transformVertex(x1, y2, transformMatrix, tx4, ty4);

            // Keep coordinates in pixel space - the vertex shader will transform them
            // No conversion to NDC needed here since the vertex shader applies projection matrix

            if (hasEffect) {
                // Create effect vertices (2 triangles, 6 vertices)
                TextEffectVertex ev1 = {{tx1, ty1}, {metrics.atlasX, metrics.atlasY},
                                       {r, g, b, a}, {er, eg, eb, ea},
                                       item.effectIntensity, item.effectSize, item.animationTime,
                                       static_cast<uint32_t>(item.effect)};
                TextEffectVertex ev2 = {{tx2, ty2}, {metrics.atlasX + metrics.atlasWidth, metrics.atlasY},
                                       {r, g, b, a}, {er, eg, eb, ea},
                                       item.effectIntensity, item.effectSize, item.animationTime,
                                       static_cast<uint32_t>(item.effect)};
                TextEffectVertex ev3 = {{tx3, ty3}, {metrics.atlasX + metrics.atlasWidth, metrics.atlasY + metrics.atlasHeight},
                                       {r, g, b, a}, {er, eg, eb, ea},
                                       item.effectIntensity, item.effectSize, item.animationTime,
                                       static_cast<uint32_t>(item.effect)};
                TextEffectVertex ev4 = {{tx4, ty4}, {metrics.atlasX, metrics.atlasY + metrics.atlasHeight},
                                       {r, g, b, a}, {er, eg, eb, ea},
                                       item.effectIntensity, item.effectSize, item.animationTime,
                                       static_cast<uint32_t>(item.effect)};

                // First triangle: ev1, ev2, ev3
                m_impl->effectVertices.push_back(ev1);
                m_impl->effectVertices.push_back(ev2);
                m_impl->effectVertices.push_back(ev3);

                // Second triangle: ev1, ev3, ev4
                m_impl->effectVertices.push_back(ev1);
                m_impl->effectVertices.push_back(ev3);
                m_impl->effectVertices.push_back(ev4);
            } else {
                // Create regular vertices (2 triangles, 6 vertices)
                TransformedTextVertex v1 = {{tx1, ty1}, {metrics.atlasX, metrics.atlasY}, {r, g, b, a}};
                TransformedTextVertex v2 = {{tx2, ty2}, {metrics.atlasX + metrics.atlasWidth, metrics.atlasY}, {r, g, b, a}};
                TransformedTextVertex v3 = {{tx3, ty3}, {metrics.atlasX + metrics.atlasWidth, metrics.atlasY + metrics.atlasHeight}, {r, g, b, a}};
                TransformedTextVertex v4 = {{tx4, ty4}, {metrics.atlasX, metrics.atlasY + metrics.atlasHeight}, {r, g, b, a}};

                // First triangle: v1, v2, v3
                m_impl->vertices.push_back(v1);
                m_impl->vertices.push_back(v2);
                m_impl->vertices.push_back(v3);

                // Second triangle: v1, v3, v4
                m_impl->vertices.push_back(v1);
                m_impl->vertices.push_back(v3);
                m_impl->vertices.push_back(v4);
            }

            currentX += metrics.advance;
        }
    }

    // Create/update Metal vertex buffers

    // Regular vertices buffer
    size_t vertexDataSize = m_impl->vertices.size() * sizeof(TransformedTextVertex);
    if (vertexDataSize > 0) {
        if (!m_impl->vertexBuffer || vertexDataSize > m_impl->vertexBufferSize) {
            // Create new buffer
            m_impl->vertexBuffer = [m_impl->device newBufferWithLength:vertexDataSize
                                                               options:MTLResourceStorageModeShared];
            m_impl->vertexBufferSize = vertexDataSize;
        }

        // Copy vertex data
        memcpy([m_impl->vertexBuffer contents], m_impl->vertices.data(), vertexDataSize);
    }

    // Effect vertices buffer
    size_t effectVertexDataSize = m_impl->effectVertices.size() * sizeof(TextEffectVertex);
    if (effectVertexDataSize > 0) {
        if (!m_impl->effectVertexBuffer || effectVertexDataSize > m_impl->effectVertexBufferSize) {
            // Create new buffer
            m_impl->effectVertexBuffer = [m_impl->device newBufferWithLength:effectVertexDataSize
                                                                     options:MTLResourceStorageModeShared];
            m_impl->effectVertexBufferSize = effectVertexDataSize;
        }

        // Copy effect vertex data
        memcpy([m_impl->effectVertexBuffer contents], m_impl->effectVertices.data(), effectVertexDataSize);
    }

    m_impl->needsVertexRebuild = false;
}

void TextDisplayManager::render(MTLRenderCommandEncoderPtr encoder,
                                uint32_t viewportWidth, uint32_t viewportHeight) {
    if (!m_impl || !m_impl->fontAtlas) {
        return;
    }

    // Ensure vertex buffer is up to date
    buildVertexBuffers(viewportWidth, viewportHeight);

    // Render regular vertices (no effects)
    if (!m_impl->vertices.empty() && m_impl->vertexBuffer) {
        [encoder setVertexBuffer:m_impl->vertexBuffer offset:0 atIndex:0];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                    vertexStart:0
                    vertexCount:m_impl->vertices.size()];
    }

    // Render effect vertices (with effects)
    if (!m_impl->effectVertices.empty() && m_impl->effectVertexBuffer) {
        [encoder setVertexBuffer:m_impl->effectVertexBuffer offset:0 atIndex:0];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                    vertexStart:0
                    vertexCount:m_impl->effectVertices.size()];
    }
}

// =============================================================================
// Status and Debug
// =============================================================================

size_t TextDisplayManager::getTextItemCount() const {
    return m_impl ? m_impl->textItems.size() : 0;
}

size_t TextDisplayManager::getVisibleTextItemCount() const {
    if (!m_impl) return 0;

    size_t count = 0;
    for (const auto& item : m_impl->textItems) {
        if (item.visible) ++count;
    }
    return count;
}

bool TextDisplayManager::hasContent() const {
    return getVisibleTextItemCount() > 0;
}

size_t TextDisplayManager::getTotalVertexCount() const {
    return m_impl ? m_impl->vertices.size() : 0;
}

bool TextDisplayManager::hasEffects() const {
    return m_impl ? m_impl->hasEffects : false;
}

// =============================================================================
// Private Implementation
// =============================================================================

int TextDisplayManager::generateItemId() {
    return m_impl->nextItemId++;
}

void TextDisplayManager::calculateTransformMatrix(const TextDisplayItem& item, float* matrix) {
    // Initialize as identity matrix (column-major order)
    for (int i = 0; i < 16; ++i) {
        matrix[i] = 0.0f;
    }
    matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.0f;

    // Translation matrix
    float tx = item.x;
    float ty = item.y;

    // Convert rotation to radians
    float rotRad = item.rotation * M_PI / 180.0f;
    float cosR = cosf(rotRad);
    float sinR = sinf(rotRad);

    // Combined transformation matrix: T * R * S * Shear
    // Where T = translation, R = rotation, S = scale, Shear = shear

    float scaleX = item.scaleX;
    float scaleY = item.scaleY;
    float shearX = item.shearX;
    float shearY = item.shearY;

    // Build composite matrix manually for better control
    matrix[0] = scaleX * cosR + shearX * scaleY * sinR;    // m00
    matrix[1] = scaleX * sinR - shearX * scaleY * cosR;    // m01
    matrix[4] = shearY * scaleX * cosR - scaleY * sinR;    // m10
    matrix[5] = shearY * scaleX * sinR + scaleY * cosR;    // m11
    matrix[12] = tx;                                        // m30 (translation X)
    matrix[13] = ty;                                        // m31 (translation Y)
}

void TextDisplayManager::transformVertex(float x, float y, const float* matrix,
                                        float& outX, float& outY) {
    // Apply 2D transformation: [x', y'] = [x, y, 1] * Matrix
    outX = x * matrix[0] + y * matrix[4] + matrix[12];
    outY = x * matrix[1] + y * matrix[5] + matrix[13];
}

void TextDisplayManager::calculateTextDimensions(const std::string& text,
                                                float& outWidth, float& outHeight) {
    if (!m_impl->fontAtlas || text.empty()) {
        outWidth = outHeight = 0.0f;
        return;
    }

    outWidth = 0.0f;
    outHeight = static_cast<float>(m_impl->fontAtlas->getGlyphHeight());

    for (char c : text) {
        uint32_t codepoint = static_cast<uint32_t>(c);
        if (m_impl->fontAtlas->hasGlyph(codepoint)) {
            auto metrics = m_impl->fontAtlas->getGlyphMetrics(codepoint);
            outWidth += metrics.advance;
        } else {
            outWidth += m_impl->fontAtlas->getGlyphWidth();
        }
    }
}

void TextDisplayManager::calculateAlignmentOffset(const std::string& text, TextAlignment alignment,
                                                 float& outOffsetX, float& outOffsetY) {
    outOffsetY = 0.0f; // Always align to top for now

    if (alignment == TextAlignment::LEFT) {
        outOffsetX = 0.0f;
        return;
    }

    float textWidth, textHeight;
    calculateTextDimensions(text, textWidth, textHeight);

    switch (alignment) {
        case TextAlignment::CENTER:
            outOffsetX = -textWidth * 0.5f;
            break;
        case TextAlignment::RIGHT:
            outOffsetX = -textWidth;
            break;
        default:
            outOffsetX = 0.0f;
            break;
    }
}

void TextDisplayManager::sortTextItemsByLayer() {
    if (!m_impl) return;

    // Create indices and sort by layer
    std::vector<size_t> indices(m_impl->textItems.size());
    for (size_t i = 0; i < indices.size(); ++i) {
        indices[i] = i;
    }

    std::sort(indices.begin(), indices.end(), [this](size_t a, size_t b) {
        return m_impl->textItems[a].layer < m_impl->textItems[b].layer;
    });

    // Reorder both vectors
    std::vector<TextDisplayItem> sortedItems;
    std::vector<int> sortedIds;

    for (size_t idx : indices) {
        sortedItems.push_back(m_impl->textItems[idx]);
        sortedIds.push_back(m_impl->textItemIds[idx]);
    }

    m_impl->textItems = std::move(sortedItems);
    m_impl->textItemIds = std::move(sortedIds);
}

} // namespace SuperTerminal
