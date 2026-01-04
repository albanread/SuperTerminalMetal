//
// TileData.cpp
// SuperTerminal Framework - Tilemap System
//
// TileData implementation
// Copyright © 2024 SuperTerminal. All rights reserved.
//

#include "TileData.h"
#include <cstdio>

namespace SuperTerminal {

// Thread-local buffer for toString (avoid allocations)
thread_local char g_tileDataStringBuffer[128];

const char* TileData::toString() const {
    if (isEmpty()) {
        snprintf(g_tileDataStringBuffer, sizeof(g_tileDataStringBuffer), 
                 "TileData(EMPTY)");
    } else {
        const char* rotStr = "";
        switch (getRotation()) {
            case TILE_ROTATION_0:   rotStr = "0°"; break;
            case TILE_ROTATION_90:  rotStr = "90°"; break;
            case TILE_ROTATION_180: rotStr = "180°"; break;
            case TILE_ROTATION_270: rotStr = "270°"; break;
        }
        
        snprintf(g_tileDataStringBuffer, sizeof(g_tileDataStringBuffer),
                 "TileData(id=%u, flipX=%d, flipY=%d, rot=%s, col=%d)",
                 getTileID(),
                 getFlipX() ? 1 : 0,
                 getFlipY() ? 1 : 0,
                 rotStr,
                 getCollision() ? 1 : 0);
    }
    
    return g_tileDataStringBuffer;
}

} // namespace SuperTerminal