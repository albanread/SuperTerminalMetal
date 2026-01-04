//
// st_api_polygons.mm
// SuperTerminal v2 - Polygon Rendering API Implementation
//
// High-performance GPU-accelerated polygon rendering with gradients
//

#include "st_api_polygons.h"
#include "st_api_context.h"
#include "../Display/PolygonManager.h"
#include "../Display/DisplayManager.h"

using namespace SuperTerminal;

// =============================================================================
// ID-Based Polygon Management API Implementation
// =============================================================================

ST_API int st_polygon_create(float x, float y, float radius, int numSides, uint32_t color) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return -1;
    }

    return polyMgr->createPolygon(x, y, radius, numSides, color);
}

ST_API int st_polygon_create_gradient(float x, float y, float radius, int numSides,
                                       uint32_t color1, uint32_t color2,
                                       STPolygonGradientMode mode) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return -1;
    }

    return polyMgr->createGradient(x, y, radius, numSides, color1, color2,
                                   static_cast<PolygonGradientMode>(mode));
}

ST_API int st_polygon_create_three_point(float x, float y, float radius, int numSides,
                                          uint32_t color1, uint32_t color2, uint32_t color3,
                                          STPolygonGradientMode mode) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return -1;
    }

    return polyMgr->createThreePointGradient(x, y, radius, numSides, color1, color2, color3,
                                              static_cast<PolygonGradientMode>(mode));
}

ST_API int st_polygon_create_four_corner(float x, float y, float radius, int numSides,
                                          uint32_t topLeft, uint32_t topRight,
                                          uint32_t bottomRight, uint32_t bottomLeft) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return -1;
    }

    return polyMgr->createFourCornerGradient(x, y, radius, numSides,
                                              topLeft, topRight, bottomRight, bottomLeft);
}

// =============================================================================
// Procedural Pattern Creation API Implementation
// =============================================================================

ST_API int st_polygon_create_outline(float x, float y, float radius, int numSides,
                                      uint32_t fillColor, uint32_t outlineColor, float lineWidth) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return -1;
    }

    return polyMgr->createOutline(x, y, radius, numSides, fillColor, outlineColor, lineWidth);
}

ST_API int st_polygon_create_dashed_outline(float x, float y, float radius, int numSides,
                                             uint32_t fillColor, uint32_t outlineColor,
                                             float lineWidth, float dashLength) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return -1;
    }

    return polyMgr->createDashedOutline(x, y, radius, numSides, fillColor, outlineColor, lineWidth, dashLength);
}

ST_API int st_polygon_create_horizontal_stripes(float x, float y, float radius, int numSides,
                                                 uint32_t color1, uint32_t color2, float stripeHeight) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return -1;
    }

    return polyMgr->createHorizontalStripes(x, y, radius, numSides, color1, color2, stripeHeight);
}

ST_API int st_polygon_create_vertical_stripes(float x, float y, float radius, int numSides,
                                               uint32_t color1, uint32_t color2, float stripeWidth) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return -1;
    }

    return polyMgr->createVerticalStripes(x, y, radius, numSides, color1, color2, stripeWidth);
}

ST_API int st_polygon_create_diagonal_stripes(float x, float y, float radius, int numSides,
                                               uint32_t color1, uint32_t color2,
                                               float stripeWidth, float angle) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return -1;
    }

    return polyMgr->createDiagonalStripes(x, y, radius, numSides, color1, color2, stripeWidth, angle);
}

ST_API int st_polygon_create_checkerboard(float x, float y, float radius, int numSides,
                                           uint32_t color1, uint32_t color2, float cellSize) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return -1;
    }

    return polyMgr->createCheckerboard(x, y, radius, numSides, color1, color2, cellSize);
}

ST_API int st_polygon_create_dots(float x, float y, float radius, int numSides,
                                   uint32_t dotColor, uint32_t backgroundColor,
                                   float dotRadius, float spacing) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return -1;
    }

    return polyMgr->createDots(x, y, radius, numSides, dotColor, backgroundColor, dotRadius, spacing);
}

ST_API int st_polygon_create_crosshatch(float x, float y, float radius, int numSides,
                                         uint32_t lineColor, uint32_t backgroundColor,
                                         float lineWidth, float spacing) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return -1;
    }

    return polyMgr->createCrosshatch(x, y, radius, numSides, lineColor, backgroundColor, lineWidth, spacing);
}

ST_API int st_polygon_create_grid(float x, float y, float radius, int numSides,
                                   uint32_t lineColor, uint32_t backgroundColor,
                                   float lineWidth, float cellSize) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return -1;
    }

    return polyMgr->createGrid(x, y, radius, numSides, lineColor, backgroundColor, lineWidth, cellSize);
}

// =============================================================================
// Polygon Update API Implementation
// =============================================================================

ST_API bool st_polygon_set_position(int id, float x, float y) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return false;
    }

    return polyMgr->updatePosition(id, x, y);
}

ST_API bool st_polygon_set_radius(int id, float radius) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return false;
    }

    return polyMgr->updateRadius(id, radius);
}

ST_API bool st_polygon_set_sides(int id, int numSides) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return false;
    }

    return polyMgr->updateSides(id, numSides);
}

ST_API bool st_polygon_set_color(int id, uint32_t color) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return false;
    }

    return polyMgr->updateColor(id, color);
}

ST_API bool st_polygon_set_colors(int id, uint32_t color1, uint32_t color2,
                                   uint32_t color3, uint32_t color4) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return false;
    }

    return polyMgr->updateColors(id, color1, color2, color3, color4);
}

ST_API bool st_polygon_set_mode(int id, STPolygonGradientMode mode) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return false;
    }

    return polyMgr->updateMode(id, static_cast<PolygonGradientMode>(mode));
}

ST_API bool st_polygon_set_parameters(int id, float param1, float param2, float param3) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return false;
    }

    return polyMgr->updateParameters(id, param1, param2, param3);
}

ST_API bool st_polygon_set_rotation(int id, float angleDegrees) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return false;
    }

    return polyMgr->setRotation(id, angleDegrees);
}

ST_API bool st_polygon_set_visible(int id, bool visible) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return false;
    }

    return polyMgr->setVisible(id, visible);
}

ST_API bool st_polygon_exists(int id) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return false;
    }

    return polyMgr->exists(id);
}

ST_API bool st_polygon_is_visible(int id) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return false;
    }

    return polyMgr->isVisible(id);
}

ST_API bool st_polygon_delete(int id) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return false;
    }

    return polyMgr->deletePolygon(id);
}

ST_API void st_polygon_delete_all(void) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return;
    }

    polyMgr->deleteAll();
}

// =============================================================================
// General Polygon Statistics and Management Implementation
// =============================================================================

ST_API size_t st_polygon_count(void) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return 0;
    }

    return polyMgr->count();
}

ST_API bool st_polygon_is_empty(void) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return true;
    }

    return polyMgr->isEmpty();
}

ST_API void st_polygon_set_max(size_t max) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return;
    }

    polyMgr->setMaxPolygons(max);
}

ST_API size_t st_polygon_get_max(void) {
    ST_LOCK;

    auto polyMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getPolygonManager() : nullptr;
    if (!polyMgr) {
        ST_SET_ERROR("Polygon manager not initialized");
        return 0;
    }

    return polyMgr->getMaxPolygons();
}
