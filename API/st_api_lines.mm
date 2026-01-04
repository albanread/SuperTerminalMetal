//
//  st_api_lines.mm
//  SuperTerminal Framework - Line Management C API Implementation
//
//  Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "st_api_lines.h"
#include "st_api_context.h"
#include "../Display/LineManager.h"
#include "../Display/DisplayManager.h"

using namespace SuperTerminal;

// =============================================================================
// Line Creation Functions
// =============================================================================

ST_API int st_line_create(float x1, float y1, float x2, float y2, uint32_t color, float thickness) {
    ST_LOCK;

    auto lineMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getLineManager() : nullptr;
    if (!lineMgr) {
        ST_SET_ERROR("Line manager not initialized");
        return -1;
    }

    return lineMgr->createLine(x1, y1, x2, y2, color, thickness);
}

ST_API int st_line_create_gradient(float x1, float y1, float x2, float y2,
                                    uint32_t color1, uint32_t color2, float thickness) {
    ST_LOCK;

    auto lineMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getLineManager() : nullptr;
    if (!lineMgr) {
        ST_SET_ERROR("Line manager not initialized");
        return -1;
    }

    return lineMgr->createGradientLine(x1, y1, x2, y2, color1, color2, thickness);
}

ST_API int st_line_create_dashed(float x1, float y1, float x2, float y2,
                                  uint32_t color, float thickness,
                                  float dashLength, float gapLength) {
    ST_LOCK;

    auto lineMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getLineManager() : nullptr;
    if (!lineMgr) {
        ST_SET_ERROR("Line manager not initialized");
        return -1;
    }

    return lineMgr->createDashedLine(x1, y1, x2, y2, color, thickness, dashLength, gapLength);
}

ST_API int st_line_create_dotted(float x1, float y1, float x2, float y2,
                                  uint32_t color, float thickness,
                                  float dotSpacing) {
    ST_LOCK;

    auto lineMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getLineManager() : nullptr;
    if (!lineMgr) {
        ST_SET_ERROR("Line manager not initialized");
        return -1;
    }

    return lineMgr->createDottedLine(x1, y1, x2, y2, color, thickness, dotSpacing);
}

// =============================================================================
// Line Update Functions
// =============================================================================

ST_API bool st_line_set_endpoints(int id, float x1, float y1, float x2, float y2) {
    ST_LOCK;

    auto lineMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getLineManager() : nullptr;
    if (!lineMgr) {
        ST_SET_ERROR("Line manager not initialized");
        return false;
    }

    return lineMgr->setEndpoints(id, x1, y1, x2, y2);
}

ST_API bool st_line_set_thickness(int id, float thickness) {
    ST_LOCK;

    auto lineMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getLineManager() : nullptr;
    if (!lineMgr) {
        ST_SET_ERROR("Line manager not initialized");
        return false;
    }

    return lineMgr->setThickness(id, thickness);
}

ST_API bool st_line_set_color(int id, uint32_t color) {
    ST_LOCK;

    auto lineMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getLineManager() : nullptr;
    if (!lineMgr) {
        ST_SET_ERROR("Line manager not initialized");
        return false;
    }

    return lineMgr->setColor(id, color);
}

ST_API bool st_line_set_colors(int id, uint32_t color1, uint32_t color2) {
    ST_LOCK;

    auto lineMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getLineManager() : nullptr;
    if (!lineMgr) {
        ST_SET_ERROR("Line manager not initialized");
        return false;
    }

    return lineMgr->setColors(id, color1, color2);
}

ST_API bool st_line_set_dash_pattern(int id, float dashLength, float gapLength) {
    ST_LOCK;

    auto lineMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getLineManager() : nullptr;
    if (!lineMgr) {
        ST_SET_ERROR("Line manager not initialized");
        return false;
    }

    return lineMgr->setDashPattern(id, dashLength, gapLength);
}

ST_API bool st_line_set_visible(int id, bool visible) {
    ST_LOCK;

    auto lineMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getLineManager() : nullptr;
    if (!lineMgr) {
        ST_SET_ERROR("Line manager not initialized");
        return false;
    }

    return lineMgr->setVisible(id, visible);
}

// =============================================================================
// Line Query Functions
// =============================================================================

ST_API bool st_line_exists(int id) {
    ST_LOCK;

    auto lineMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getLineManager() : nullptr;
    if (!lineMgr) {
        return false;
    }

    return lineMgr->exists(id);
}

ST_API bool st_line_is_visible(int id) {
    ST_LOCK;

    auto lineMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getLineManager() : nullptr;
    if (!lineMgr) {
        return false;
    }

    return lineMgr->isVisible(id);
}

// =============================================================================
// Line Management Functions
// =============================================================================

ST_API bool st_line_delete(int id) {
    ST_LOCK;

    auto lineMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getLineManager() : nullptr;
    if (!lineMgr) {
        ST_SET_ERROR("Line manager not initialized");
        return false;
    }

    return lineMgr->deleteLine(id);
}

ST_API void st_line_delete_all(void) {
    ST_LOCK;

    auto lineMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getLineManager() : nullptr;
    if (!lineMgr) {
        ST_SET_ERROR("Line manager not initialized");
        return;
    }

    lineMgr->deleteAll();
}

ST_API size_t st_line_count(void) {
    ST_LOCK;

    auto lineMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getLineManager() : nullptr;
    if (!lineMgr) {
        return 0;
    }

    return lineMgr->getLineCount();
}

ST_API bool st_line_is_empty(void) {
    ST_LOCK;

    auto lineMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getLineManager() : nullptr;
    if (!lineMgr) {
        return true;
    }

    return lineMgr->isEmpty();
}

ST_API size_t st_line_get_max(void) {
    ST_LOCK;

    auto lineMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getLineManager() : nullptr;
    if (!lineMgr) {
        return 0;
    }

    return lineMgr->getMaxLines();
}

ST_API void st_line_set_max(size_t max) {
    ST_LOCK;

    auto lineMgr = ST_CONTEXT.display() ? ST_CONTEXT.display()->getLineManager() : nullptr;
    if (!lineMgr) {
        ST_SET_ERROR("Line manager not initialized");
        return;
    }

    lineMgr->setMaxLines(max);
}
