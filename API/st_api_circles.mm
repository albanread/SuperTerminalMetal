//
// st_api_circles.mm
// SuperTerminal v2 - Circle Rendering API Implementation
//
// High-performance GPU-accelerated circle rendering with gradients
//

#include "st_api_circles.h"
#include "st_api_context.h"
#include "../Display/CircleManager.h"
#include "../Display/DisplayManager.h"

using namespace SuperTerminal;

// =============================================================================
// ID-Based Circle Management API Implementation
// =============================================================================

ST_API int st_circle_create(float x, float y, float radius, uint32_t color) {
    ST_LOCK;

    auto circleMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getCircleManager() : nullptr;
    if (!circleMgr) {
        ST_SET_ERROR("Circle manager not initialized");
        return -1;
    }

    return circleMgr->createCircle(x, y, radius, color);
}

ST_API int st_circle_create_radial(float x, float y, float radius,
                                    uint32_t centerColor, uint32_t edgeColor) {
    ST_LOCK;

    auto circleMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getCircleManager() : nullptr;
    if (!circleMgr) {
        ST_SET_ERROR("Circle manager not initialized");
        return -1;
    }

    return circleMgr->createRadialGradient(x, y, radius, centerColor, edgeColor);
}

ST_API int st_circle_create_radial_3(float x, float y, float radius,
                                      uint32_t color1, uint32_t color2, uint32_t color3) {
    ST_LOCK;

    auto circleMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getCircleManager() : nullptr;
    if (!circleMgr) {
        ST_SET_ERROR("Circle manager not initialized");
        return -1;
    }

    return circleMgr->createRadialGradient3(x, y, radius, color1, color2, color3);
}

ST_API int st_circle_create_radial_4(float x, float y, float radius,
                                      uint32_t color1, uint32_t color2,
                                      uint32_t color3, uint32_t color4) {
    ST_LOCK;

    auto circleMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getCircleManager() : nullptr;
    if (!circleMgr) {
        ST_SET_ERROR("Circle manager not initialized");
        return -1;
    }

    return circleMgr->createRadialGradient4(x, y, radius, color1, color2, color3, color4);
}

// =============================================================================
// Procedural Pattern Creation API Implementation
// =============================================================================

ST_API int st_circle_create_outline(float x, float y, float radius,
                                     uint32_t fillColor, uint32_t outlineColor, float lineWidth) {
    ST_LOCK;

    auto circleMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getCircleManager() : nullptr;
    if (!circleMgr) {
        ST_SET_ERROR("Circle manager not initialized");
        return -1;
    }

    return circleMgr->createOutline(x, y, radius, fillColor, outlineColor, lineWidth);
}

ST_API int st_circle_create_dashed_outline(float x, float y, float radius,
                                            uint32_t fillColor, uint32_t outlineColor,
                                            float lineWidth, float dashLength) {
    ST_LOCK;

    auto circleMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getCircleManager() : nullptr;
    if (!circleMgr) {
        ST_SET_ERROR("Circle manager not initialized");
        return -1;
    }

    return circleMgr->createDashedOutline(x, y, radius, fillColor, outlineColor, lineWidth, dashLength);
}

ST_API int st_circle_create_ring(float x, float y, float outerRadius, float innerRadius,
                                  uint32_t color) {
    ST_LOCK;

    auto circleMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getCircleManager() : nullptr;
    if (!circleMgr) {
        ST_SET_ERROR("Circle manager not initialized");
        return -1;
    }

    return circleMgr->createRing(x, y, outerRadius, innerRadius, color);
}

ST_API int st_circle_create_pie_slice(float x, float y, float radius,
                                       float startAngle, float endAngle, uint32_t color) {
    ST_LOCK;

    auto circleMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getCircleManager() : nullptr;
    if (!circleMgr) {
        ST_SET_ERROR("Circle manager not initialized");
        return -1;
    }

    return circleMgr->createPieSlice(x, y, radius, startAngle, endAngle, color);
}

ST_API int st_circle_create_arc(float x, float y, float radius,
                                 float startAngle, float endAngle,
                                 uint32_t color, float lineWidth) {
    ST_LOCK;

    auto circleMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getCircleManager() : nullptr;
    if (!circleMgr) {
        ST_SET_ERROR("Circle manager not initialized");
        return -1;
    }

    return circleMgr->createArc(x, y, radius, startAngle, endAngle, color, lineWidth);
}

ST_API int st_circle_create_dots_ring(float x, float y, float radius,
                                       uint32_t dotColor, uint32_t backgroundColor,
                                       float dotRadius, int numDots) {
    ST_LOCK;

    auto circleMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getCircleManager() : nullptr;
    if (!circleMgr) {
        ST_SET_ERROR("Circle manager not initialized");
        return -1;
    }

    return circleMgr->createDotsRing(x, y, radius, dotColor, backgroundColor, dotRadius, numDots);
}

ST_API int st_circle_create_star_burst(float x, float y, float radius,
                                        uint32_t color1, uint32_t color2, int numRays) {
    ST_LOCK;

    auto circleMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getCircleManager() : nullptr;
    if (!circleMgr) {
        ST_SET_ERROR("Circle manager not initialized");
        return -1;
    }

    return circleMgr->createStarBurst(x, y, radius, color1, color2, numRays);
}

// =============================================================================
// Circle Update API Implementation
// =============================================================================

ST_API bool st_circle_set_position(int id, float x, float y) {
    ST_LOCK;

    auto circleMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getCircleManager() : nullptr;
    if (!circleMgr) {
        ST_SET_ERROR("Circle manager not initialized");
        return false;
    }

    return circleMgr->updatePosition(id, x, y);
}

ST_API bool st_circle_set_radius(int id, float radius) {
    ST_LOCK;

    auto circleMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getCircleManager() : nullptr;
    if (!circleMgr) {
        ST_SET_ERROR("Circle manager not initialized");
        return false;
    }

    return circleMgr->updateRadius(id, radius);
}

ST_API bool st_circle_set_color(int id, uint32_t color) {
    ST_LOCK;

    auto circleMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getCircleManager() : nullptr;
    if (!circleMgr) {
        ST_SET_ERROR("Circle manager not initialized");
        return false;
    }

    return circleMgr->updateColor(id, color);
}

ST_API bool st_circle_set_colors(int id, uint32_t color1, uint32_t color2,
                                  uint32_t color3, uint32_t color4) {
    ST_LOCK;

    auto circleMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getCircleManager() : nullptr;
    if (!circleMgr) {
        ST_SET_ERROR("Circle manager not initialized");
        return false;
    }

    return circleMgr->updateColors(id, color1, color2, color3, color4);
}

ST_API bool st_circle_set_parameters(int id, float param1, float param2, float param3) {
    ST_LOCK;

    auto circleMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getCircleManager() : nullptr;
    if (!circleMgr) {
        ST_SET_ERROR("Circle manager not initialized");
        return false;
    }

    return circleMgr->updateParameters(id, param1, param2, param3);
}

ST_API bool st_circle_set_visible(int id, bool visible) {
    ST_LOCK;

    auto circleMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getCircleManager() : nullptr;
    if (!circleMgr) {
        ST_SET_ERROR("Circle manager not initialized");
        return false;
    }

    return circleMgr->setVisible(id, visible);
}

ST_API bool st_circle_exists(int id) {
    ST_LOCK;

    auto circleMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getCircleManager() : nullptr;
    if (!circleMgr) {
        ST_SET_ERROR("Circle manager not initialized");
        return false;
    }

    return circleMgr->exists(id);
}

ST_API bool st_circle_is_visible(int id) {
    ST_LOCK;

    auto circleMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getCircleManager() : nullptr;
    if (!circleMgr) {
        ST_SET_ERROR("Circle manager not initialized");
        return false;
    }

    return circleMgr->isVisible(id);
}

ST_API bool st_circle_delete(int id) {
    ST_LOCK;

    auto circleMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getCircleManager() : nullptr;
    if (!circleMgr) {
        ST_SET_ERROR("Circle manager not initialized");
        return false;
    }

    return circleMgr->deleteCircle(id);
}

ST_API void st_circle_delete_all(void) {
    ST_LOCK;

    auto circleMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getCircleManager() : nullptr;
    if (!circleMgr) {
        ST_SET_ERROR("Circle manager not initialized");
        return;
    }

    circleMgr->deleteAll();
}

// =============================================================================
// General Circle Statistics and Management Implementation
// =============================================================================

ST_API size_t st_circle_count(void) {
    ST_LOCK;

    auto circleMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getCircleManager() : nullptr;
    if (!circleMgr) {
        return 0;
    }

    return circleMgr->getCircleCount();
}

ST_API bool st_circle_is_empty(void) {
    ST_LOCK;

    auto circleMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getCircleManager() : nullptr;
    if (!circleMgr) {
        return true;
    }

    return circleMgr->isEmpty();
}

ST_API void st_circle_set_max(size_t max) {
    ST_LOCK;

    auto circleMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getCircleManager() : nullptr;
    if (!circleMgr) {
        ST_SET_ERROR("Circle manager not initialized");
        return;
    }

    circleMgr->setMaxCircles(max);
}

ST_API size_t st_circle_get_max(void) {
    ST_LOCK;

    auto circleMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getCircleManager() : nullptr;
    if (!circleMgr) {
        return 0;
    }

    return circleMgr->getMaxCircles();
}
