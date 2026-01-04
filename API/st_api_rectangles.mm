//
// st_api_rectangles.cpp
// SuperTerminal v2 - Rectangle Rendering API Implementation
//
// High-performance GPU-accelerated rectangle rendering with gradients
//

#include "st_api_rectangles.h"
#include "st_api_context.h"
#include "../Display/RectangleManager.h"
#include "../Display/DisplayManager.h"

using namespace SuperTerminal;

// =============================================================================
// ID-Based Rectangle Management API Implementation
// =============================================================================

ST_API int st_rect_create(float x, float y, float width, float height, uint32_t color) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        ST_SET_ERROR("Rectangle manager not initialized");
        return -1;
    }

    return rectMgr->createRectangle(x, y, width, height, color);
}

ST_API int st_rect_create_gradient(float x, float y, float width, float height,
                                     uint32_t color1, uint32_t color2,
                                     STRectangleGradientMode mode) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        ST_SET_ERROR("Rectangle manager not initialized");
        return -1;
    }

    return rectMgr->createGradient(x, y, width, height, color1, color2,
                                   static_cast<RectangleGradientMode>(mode));
}

ST_API int st_rect_create_three_point(float x, float y, float width, float height,
                                        uint32_t color1, uint32_t color2, uint32_t color3,
                                        STRectangleGradientMode mode) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        ST_SET_ERROR("Rectangle manager not initialized");
        return -1;
    }

    return rectMgr->createThreePointGradient(x, y, width, height, color1, color2, color3,
                                              static_cast<RectangleGradientMode>(mode));
}

ST_API int st_rect_create_four_corner(float x, float y, float width, float height,
                                        uint32_t topLeft, uint32_t topRight,
                                        uint32_t bottomRight, uint32_t bottomLeft) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        ST_SET_ERROR("Rectangle manager not initialized");
        return -1;
    }

    return rectMgr->createFourCornerGradient(x, y, width, height,
                                              topLeft, topRight, bottomRight, bottomLeft);
}

// =============================================================================
// Procedural Pattern Creation API Implementation
// =============================================================================

ST_API int st_rect_create_outline(float x, float y, float width, float height,
                                   uint32_t fillColor, uint32_t outlineColor, float lineWidth) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        ST_SET_ERROR("Rectangle manager not initialized");
        return -1;
    }

    return rectMgr->createOutline(x, y, width, height, fillColor, outlineColor, lineWidth);
}

ST_API int st_rect_create_dashed_outline(float x, float y, float width, float height,
                                          uint32_t fillColor, uint32_t outlineColor,
                                          float lineWidth, float dashLength) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        ST_SET_ERROR("Rectangle manager not initialized");
        return -1;
    }

    return rectMgr->createDashedOutline(x, y, width, height, fillColor, outlineColor,
                                        lineWidth, dashLength);
}

ST_API int st_rect_create_horizontal_stripes(float x, float y, float width, float height,
                                              uint32_t color1, uint32_t color2, float stripeHeight) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        ST_SET_ERROR("Rectangle manager not initialized");
        return -1;
    }

    return rectMgr->createHorizontalStripes(x, y, width, height, color1, color2, stripeHeight);
}

ST_API int st_rect_create_vertical_stripes(float x, float y, float width, float height,
                                            uint32_t color1, uint32_t color2, float stripeWidth) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        ST_SET_ERROR("Rectangle manager not initialized");
        return -1;
    }

    return rectMgr->createVerticalStripes(x, y, width, height, color1, color2, stripeWidth);
}

ST_API int st_rect_create_diagonal_stripes(float x, float y, float width, float height,
                                            uint32_t color1, uint32_t color2,
                                            float stripeWidth, float angle) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        ST_SET_ERROR("Rectangle manager not initialized");
        return -1;
    }

    return rectMgr->createDiagonalStripes(x, y, width, height, color1, color2,
                                          stripeWidth, angle);
}

ST_API int st_rect_create_checkerboard(float x, float y, float width, float height,
                                        uint32_t color1, uint32_t color2, float cellSize) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        ST_SET_ERROR("Rectangle manager not initialized");
        return -1;
    }

    return rectMgr->createCheckerboard(x, y, width, height, color1, color2, cellSize);
}

ST_API int st_rect_create_dots(float x, float y, float width, float height,
                                uint32_t dotColor, uint32_t backgroundColor,
                                float dotRadius, float spacing) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        ST_SET_ERROR("Rectangle manager not initialized");
        return -1;
    }

    return rectMgr->createDots(x, y, width, height, dotColor, backgroundColor,
                               dotRadius, spacing);
}

ST_API int st_rect_create_crosshatch(float x, float y, float width, float height,
                                      uint32_t lineColor, uint32_t backgroundColor,
                                      float lineWidth, float spacing) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        ST_SET_ERROR("Rectangle manager not initialized");
        return -1;
    }

    return rectMgr->createCrosshatch(x, y, width, height, lineColor, backgroundColor,
                                     lineWidth, spacing);
}

ST_API int st_rect_create_rounded_corners(float x, float y, float width, float height,
                                           uint32_t color, float cornerRadius) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        ST_SET_ERROR("Rectangle manager not initialized");
        return -1;
    }

    return rectMgr->createRoundedCorners(x, y, width, height, color, cornerRadius);
}

ST_API int st_rect_create_grid(float x, float y, float width, float height,
                                uint32_t lineColor, uint32_t backgroundColor,
                                float lineWidth, float cellSize) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        ST_SET_ERROR("Rectangle manager not initialized");
        return -1;
    }

    return rectMgr->createGrid(x, y, width, height, lineColor, backgroundColor,
                               lineWidth, cellSize);
}

// =============================================================================
// Rectangle Update API Implementation
// =============================================================================

ST_API bool st_rect_set_position(int id, float x, float y) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        ST_SET_ERROR("Rectangle manager not initialized");
        return false;
    }

    return rectMgr->updatePosition(id, x, y);
}

ST_API bool st_rect_set_size(int id, float width, float height) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        ST_SET_ERROR("Rectangle manager not initialized");
        return false;
    }

    return rectMgr->updateSize(id, width, height);
}

ST_API bool st_rect_set_color(int id, uint32_t color) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        ST_SET_ERROR("Rectangle manager not initialized");
        return false;
    }

    return rectMgr->updateColor(id, color);
}

ST_API bool st_rect_set_colors(int id, uint32_t color1, uint32_t color2,
                                 uint32_t color3, uint32_t color4) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        ST_SET_ERROR("Rectangle manager not initialized");
        return false;
    }

    return rectMgr->updateColors(id, color1, color2, color3, color4);
}

ST_API bool st_rect_set_mode(int id, STRectangleGradientMode mode) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        ST_SET_ERROR("Rectangle manager not initialized");
        return false;
    }

    return rectMgr->updateMode(id, static_cast<RectangleGradientMode>(mode));
}

ST_API bool st_rect_set_parameters(int id, float param1, float param2, float param3) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        ST_SET_ERROR("Rectangle manager not initialized");
        return false;
    }

    return rectMgr->updateParameters(id, param1, param2, param3);
}

ST_API bool st_rect_set_rotation(int id, float angleDegrees) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        ST_SET_ERROR("Rectangle manager not initialized");
        return false;
    }

    return rectMgr->setRotation(id, angleDegrees);
}

ST_API bool st_rect_set_visible(int id, bool visible) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        ST_SET_ERROR("Rectangle manager not initialized");
        return false;
    }

    return rectMgr->setVisible(id, visible);
}

ST_API bool st_rect_exists(int id) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        return false;
    }

    return rectMgr->exists(id);
}

ST_API bool st_rect_is_visible(int id) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        return false;
    }

    return rectMgr->isVisible(id);
}

ST_API bool st_rect_delete(int id) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        ST_SET_ERROR("Rectangle manager not initialized");
        return false;
    }

    return rectMgr->deleteRectangle(id);
}

ST_API void st_rect_delete_all(void) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        return; // Silently ignore if not initialized
    }

    rectMgr->deleteAll();
}

// =============================================================================
// General Rectangle Statistics and Management
// =============================================================================

ST_API size_t st_rect_count(void) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        return 0;
    }

    return rectMgr->getRectangleCount();
}

ST_API bool st_rect_is_empty(void) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        return true;
    }

    return rectMgr->isEmpty();
}

ST_API void st_rect_set_max(size_t max) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        ST_SET_ERROR("Rectangle manager not initialized");
        return;
    }

    rectMgr->setMaxRectangles(max);
}

ST_API size_t st_rect_get_max(void) {
    ST_LOCK;

    auto rectMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getRectangleManager() : nullptr;
    if (!rectMgr) {
        return 0;
    }

    return rectMgr->getMaxRectangles();
}
