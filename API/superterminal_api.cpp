//
// superterminal_api.cpp
// SuperTerminal v2.0 - C API Main Entry Point
//
// This file serves as the main entry point for the C API.
// All actual implementations are in modular files:
// - st_api_display.cpp: Text grid, graphics, and screen functions
// - st_api_sprites.cpp: Sprite loading and manipulation
// - st_api_audio.cpp: Sound effects, music, and synthesis
// - st_api_input.cpp: Keyboard and mouse input
// - st_api_assets.cpp: Asset loading and management
// - st_api_utils.cpp: Utility functions, frame control, versioning
//
// This design allows for easier maintenance and compilation unit separation.
//

#include "superterminal_api.h"
#include "st_api_context.h"

// =============================================================================
// API Implementation Note
// =============================================================================
//
// All st_* functions are implemented in their respective modular files.
// This file only contains the shared initialization and context management
// that ties the API to the framework components.
//
// The modular structure is:
//
// superterminal_api.h          - Public C API header (all function declarations)
// st_api_context.h/cpp         - Internal API context manager (singleton)
// st_api_display.cpp           - Text, graphics, layers, screen
// st_api_sprites.cpp           - Sprite management
// st_api_audio.cpp             - Audio (SFX, music, synth)
// st_api_input.cpp             - Input (keyboard, mouse)
// st_api_assets.cpp            - Asset loading (future)
// st_api_utils.cpp             - Utilities (color, frame control, version, debug)
//
// =============================================================================

// The API context is managed as a singleton in st_api_context.cpp
// Framework components (TextGrid, AudioManager, etc.) are registered with
// the context during application initialization, typically in the App layer.

// All API functions use the ST_CONTEXT macro to access the singleton instance
// and ST_LOCK for thread-safe access to shared state.

// No additional implementation needed here - all functions are in modular files