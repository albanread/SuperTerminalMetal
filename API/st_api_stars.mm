//
// st_api_stars.mm
// SuperTerminal v2 - Star Rendering API Implementation
//
// High-performance GPU-accelerated star rendering with gradients, rotation, and outlines
//

#include "st_api_stars.h"
#include "st_api_context.h"
#include "superterminal_api.h"
#include "../Display/StarManager.h"
#include "../Display/DisplayManager.h"

using namespace SuperTerminal;

// =============================================================================
// ID-Based Star Management API Implementation
// =============================================================================

ST_API int st_star_create(float x, float y, float outerRadius, int numPoints, uint32_t color) {
    ST_LOCK;

    auto starMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getStarManager() : nullptr;
    if (!starMgr) {
        ST_SET_ERROR("Star manager not initialized");
        return -1;
    }

    return starMgr->createStar(x, y, outerRadius, numPoints, color);
}

ST_API int st_star_create_custom(float x, float y, float outerRadius, float innerRadius,
                                  int numPoints, uint32_t color) {
    ST_LOCK;

    auto starMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getStarManager() : nullptr;
    if (!starMgr) {
        ST_SET_ERROR("Star manager not initialized");
        return -1;
    }

    return starMgr->createCustomStar(x, y, outerRadius, innerRadius, numPoints, color);
}

ST_API int st_star_create_gradient(float x, float y, float outerRadius, int numPoints,
                                    uint32_t color1, uint32_t color2,
                                    STStarGradientMode mode) {
    ST_LOCK;

    auto starMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getStarManager() : nullptr;
    if (!starMgr) {
        ST_SET_ERROR("Star manager not initialized");
        return -1;
    }

    return starMgr->createGradient(x, y, outerRadius, numPoints, color1, color2, (StarGradientMode)mode);
}

ST_API int st_star_create_outline(float x, float y, float outerRadius, int numPoints,
                                   uint32_t fillColor, uint32_t outlineColor,
                                   float lineWidth) {
    ST_LOCK;

    auto starMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getStarManager() : nullptr;
    if (!starMgr) {
        ST_SET_ERROR("Star manager not initialized");
        return -1;
    }

    return starMgr->createOutline(x, y, outerRadius, numPoints, fillColor, outlineColor, lineWidth);
}

// =============================================================================
// Star Update API Implementation
// =============================================================================

ST_API bool st_star_set_position(int id, float x, float y) {
    ST_LOCK;

    auto starMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getStarManager() : nullptr;
    if (!starMgr) {
        ST_SET_ERROR("Star manager not initialized");
        return false;
    }

    return starMgr->setPosition(id, x, y);
}

ST_API bool st_star_set_radius(int id, float outerRadius) {
    ST_LOCK;

    auto starMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getStarManager() : nullptr;
    if (!starMgr) {
        ST_SET_ERROR("Star manager not initialized");
        return false;
    }

    return starMgr->setRadius(id, outerRadius);
}

ST_API bool st_star_set_radii(int id, float outerRadius, float innerRadius) {
    ST_LOCK;

    auto starMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getStarManager() : nullptr;
    if (!starMgr) {
        ST_SET_ERROR("Star manager not initialized");
        return false;
    }

    return starMgr->setRadii(id, outerRadius, innerRadius);
}

ST_API bool st_star_set_points(int id, int numPoints) {
    ST_LOCK;

    auto starMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getStarManager() : nullptr;
    if (!starMgr) {
        ST_SET_ERROR("Star manager not initialized");
        return false;
    }

    return starMgr->setPoints(id, numPoints);
}

ST_API bool st_star_set_color(int id, uint32_t color) {
    ST_LOCK;

    auto starMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getStarManager() : nullptr;
    if (!starMgr) {
        ST_SET_ERROR("Star manager not initialized");
        return false;
    }

    return starMgr->setColor(id, color);
}

ST_API bool st_star_set_colors(int id, uint32_t color1, uint32_t color2) {
    ST_LOCK;

    auto starMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getStarManager() : nullptr;
    if (!starMgr) {
        ST_SET_ERROR("Star manager not initialized");
        return false;
    }

    return starMgr->setColors(id, color1, color2);
}

ST_API bool st_star_set_rotation(int id, float angleDegrees) {
    ST_LOCK;

    auto starMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getStarManager() : nullptr;
    if (!starMgr) {
        ST_SET_ERROR("Star manager not initialized");
        return false;
    }

    return starMgr->setRotation(id, angleDegrees);
}

ST_API bool st_star_set_visible(int id, bool visible) {
    ST_LOCK;

    auto starMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getStarManager() : nullptr;
    if (!starMgr) {
        ST_SET_ERROR("Star manager not initialized");
        return false;
    }

    return starMgr->setVisible(id, visible);
}

// =============================================================================
// Star Query/Management API Implementation
// =============================================================================

ST_API bool st_star_exists(int id) {
    ST_LOCK;

    auto starMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getStarManager() : nullptr;
    if (!starMgr) {
        ST_SET_ERROR("Star manager not initialized");
        return false;
    }

    return starMgr->exists(id);
}

ST_API bool st_star_is_visible(int id) {
    ST_LOCK;

    auto starMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getStarManager() : nullptr;
    if (!starMgr) {
        ST_SET_ERROR("Star manager not initialized");
        return false;
    }

    return starMgr->isVisible(id);
}

ST_API bool st_star_delete(int id) {
    ST_LOCK;

    auto starMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getStarManager() : nullptr;
    if (!starMgr) {
        ST_SET_ERROR("Star manager not initialized");
        return false;
    }

    return starMgr->deleteStar(id);
}

ST_API void st_star_delete_all(void) {
    ST_LOCK;

    auto starMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getStarManager() : nullptr;
    if (!starMgr) {
        ST_SET_ERROR("Star manager not initialized");
        return;
    }

    starMgr->deleteAll();
}

ST_API size_t st_star_count(void) {
    ST_LOCK;

    auto starMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getStarManager() : nullptr;
    if (!starMgr) {
        return 0;
    }

    return starMgr->getStarCount();
}

ST_API bool st_star_is_empty(void) {
    ST_LOCK;

    auto starMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getStarManager() : nullptr;
    if (!starMgr) {
        return true;
    }

    return starMgr->isEmpty();
}
