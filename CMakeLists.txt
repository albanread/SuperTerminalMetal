# Framework/CMakeLists.txt
# SuperTerminal v2.0 Framework Core

project(SuperTerminalFramework
    VERSION 2.0.0
    LANGUAGES CXX OBJCXX
)

# Find libsidplayfp (prefer Homebrew installation)
set(SIDPLAYFP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../sidplayfp_bundle")
find_library(SIDPLAYFP_LIBRARY
    NAMES sidplayfp.6 libsidplayfp.6 sidplayfp libsidplayfp
    PATHS
        "/opt/homebrew/lib"
        "/opt/homebrew/Cellar/libsidplayfp/*/lib"
        "/usr/local/lib"
        "/usr/local/Cellar/libsidplayfp/*/lib"
        "${SIDPLAYFP_ROOT}/lib"
    NO_DEFAULT_PATH
)
find_path(SIDPLAYFP_INCLUDE_DIR
    NAMES sidplayfp/sidplayfp.h
    PATHS
        "/opt/homebrew/include"
        "/opt/homebrew/Cellar/libsidplayfp/*/include"
        "/usr/local/include"
        "/usr/local/Cellar/libsidplayfp/*/include"
    NO_DEFAULT_PATH
)

if(NOT SIDPLAYFP_LIBRARY)
    message(FATAL_ERROR "libsidplayfp not found! Please ensure sidplayfp is built.")
endif()

message(STATUS "Found libsidplayfp: ${SIDPLAYFP_LIBRARY}")
message(STATUS "  Include dir: ${SIDPLAYFP_INCLUDE_DIR}")

# Framework library
add_library(SuperTerminalFramework STATIC
    # Base Runner (common runner application base class)
    BaseRunner.h
    BaseRunner.mm

    # Lua Base Runner (runner without BASIC dependencies)
    LuaBaseRunner.h
    LuaBaseRunner.mm

    # Startup state machine
    Startup/AppStartupStateMachine.h
    Startup/AppStartupStateMachine.mm

    # API headers (C interface)
    API/superterminal_api.h
    API/superterminal_api.cpp
    API/st_api_context.h
    API/st_api_context.cpp
    API/st_api_display.cpp
    API/st_api_sprites.mm
    API/st_sprite_drawing_utils.h
    API/st_sprite_drawing_utils.mm
    API/st_api_file_drawing.mm
    API/st_api_tileset_drawing.mm
    API/st_api_audio.cpp
    API/st_api_input.cpp
    API/st_api_assets.cpp
    API/st_api_tilemap.mm
    API/st_api_utils.cpp
    API/st_api_rectangles.h
    API/st_api_rectangles.mm
    API/st_api_circles.h
    API/st_api_circles.mm
    API/st_api_lines.h
    API/st_api_lines.mm
    API/st_api_polygons.h
    API/st_api_polygons.mm
    API/st_api_stars.h
    API/st_api_stars.mm
    API/st_api_collision.h
    API/st_api_collision.cpp
    API/st_api_video_mode.h
    API/st_api_video_mode.cpp
    API/st_api_video_palette.h
    API/st_api_video_palette.cpp
    API/st_api_video_image.mm

    # Display subsystem
    Display/TextGrid.cpp
    Display/TextGrid.h
    Display/CharacterMapping.cpp
    Display/CharacterMapping.h
    Display/DisplayManager.h
    Display/DisplayManager.mm
    Display/GraphicsLayer.cpp
    Display/GraphicsLayer.h
    Display/SpriteManager.h
    Display/SpriteManager.mm
    Display/SpriteCompression.h
    Display/SpriteCompression.cpp

    # Data subsystem
    Data/PaletteLibrary.h
    Data/PaletteLibrary.cpp

    Display/RectangleManager.h
    Display/RectangleManager.mm
    Display/CircleManager.h
    Display/CircleManager.mm
    Display/LineManager.h
    Display/LineManager.mm
    Display/PolygonManager.h
    Display/PolygonManager.mm
    Display/StarManager.h
    Display/StarManager.mm
    Display/LoResPaletteManager.h
    Display/LoResPaletteManager.cpp
    Display/XResPaletteManager.h
    Display/XResPaletteManager.cpp
    Display/WResPaletteManager.h
    Display/WResPaletteManager.cpp
    Display/PResPaletteManager.h
    Display/PResPaletteManager.cpp
    Display/LoResBuffer.h
    Display/LoResBuffer.cpp
    Display/UResBuffer.h
    Display/UResBuffer.cpp
    Display/XResBuffer.h
    Display/XResBuffer.cpp
    Display/WResBuffer.h
    Display/WResBuffer.cpp
    Display/PResBuffer.h
    Display/PResBuffer.cpp
    Display/TextDisplayManager.h
    Display/TextDisplayManager.mm
    Display/GraphicsRenderer.h
    Display/GraphicsRenderer.mm
    Display/VideoMode/VideoMode.h
    Display/VideoMode/VideoModeManager.h
    Display/VideoMode/VideoModeManager.cpp
    # Display/Compositor.cpp
    # Display/Compositor.h

    # Audio subsystem
    Audio/AudioManager.h
    Audio/AudioManager.mm
    Audio/SynthEngine.h
    Audio/SynthEngine.mm
    Audio/SoundBank.h
    Audio/SoundBank.cpp
    Audio/MusicBank.h
    Audio/MusicBank.cpp
    Audio/SIDPlayer.h
    Audio/SIDPlayer.cpp
    Audio/SIDBank.h
    Audio/SIDBank.cpp
    Audio/MidiEngine.h
    Audio/MidiEngine.mm
    Audio/ABCParser.h
    Audio/ABCParser.mm
    Audio/CoreAudioEngine.h
    Audio/CoreAudioEngine.mm
    Audio/Voice/VoiceController.h
    Audio/Voice/VoiceController.cpp
    Audio/Voice/VoiceScript.h
    Audio/Voice/VoiceScript.cpp
    Audio/Voice/VoiceScriptPlayer.h
    Audio/Voice/VoiceScriptPlayer.cpp

    # Input subsystem
    Input/InputManager.cpp
    Input/InputManager.h
    Input/SimpleLineEditor.cpp
    Input/SimpleLineEditor.h

    # Asset subsystem
    Assets/AssetMetadata.cpp
    Assets/AssetMetadata.h
    Assets/AssetDatabase.cpp
    Assets/AssetDatabase.h
    Assets/AssetManager.cpp
    Assets/AssetManager.h
    Assets/AssetLoader.cpp
    Assets/AssetLoader.h

    # Cart subsystem
    Cart/CartLoader.cpp
    Cart/CartLoader.h
    Cart/CartAssetProvider.cpp
    Cart/CartAssetProvider.h
    Cart/CartManager.cpp
    Cart/CartManager.h

    # Debug subsystem
    Debug/Logger.cpp
    Debug/Logger.h

    # Tilemap subsystem
    Tilemap/TileData.cpp
    Tilemap/TileData.h
    Tilemap/Tilemap.cpp
    Tilemap/Tilemap.h
    Tilemap/Camera.cpp
    Tilemap/Camera.h
    Tilemap/Tileset.mm
    Tilemap/Tileset.h
    Tilemap/TilemapLayer.cpp
    Tilemap/TilemapLayer.h
    Tilemap/TilemapManager.cpp
    Tilemap/TilemapManager.h
    Tilemap/TilemapFormat.cpp
    Tilemap/TilemapFormat.h
    Tilemap/TilemapRenderer.h
    Tilemap/TilemapRenderer.mm
    Tilemap/TileDataEx.h
    Tilemap/PaletteBank.cpp
    Tilemap/PaletteBank.h
    Tilemap/TilesetIndexed.cpp
    Tilemap/TilesetIndexed.h
    Tilemap/TilemapEx.cpp
    Tilemap/TilemapEx.h
    Tilemap/TilemapIndexedAPI.cpp
    Tilemap/TilemapIndexedAPI.h
    Tilemap/TilesetLoader.h
    Tilemap/TilesetLoader.mm

    # Editor subsystem
    Editor/ScriptDatabase.h
    Editor/ScriptDatabase.mm
    Editor/Document.h
    Editor/Document.cpp
    Editor/TextBuffer.h
    Editor/TextBuffer.cpp
    Editor/Cursor.h
    Editor/Cursor.cpp
    Editor/ScreenState.h
    Editor/ScreenState.cpp
    Editor/ScreenMode.h
    Editor/ScreenMode.cpp
    Editor/ExportImportManager.h
    Editor/ExportImportManager.cpp
    Editor/EditorAPI.h
    Editor/EditorAPI.mm

    # UI subsystem (interactive components)
    UI/InputLineEditor.h
    UI/InputLineEditor.cpp
    UI/TextGridOutputStream.h
    UI/TextGridOutputStream.cpp
    Editor/EditorRenderer.h
    Editor/EditorRenderer.cpp
    Editor/EditorStatusBar.h
    Editor/EditorStatusBar.mm
    Editor/InputHandler.h
    Editor/InputHandler.cpp
    Editor/TextEditor.h
    Editor/TextEditor.cpp
    Editor/ScriptBrowserDialog.h
    Editor/ScriptBrowserDialog.mm
    Editor/FindDialogs.h
    Editor/FindDialogs.mm
    Editor/LuaFormatterWrapper.h
    Editor/LuaFormatterWrapper.cpp

    # Particle system
    Particles/ParticleSystem.h
    Particles/ParticleSystem.cpp

    # Metal utilities
    Metal/FontAtlas.h
    Metal/FontAtlas.mm
    Metal/MetalRenderer.h
    Metal/MetalRenderer.mm
    Metal/TextRenderer.metal
    Metal/TilemapRenderer.metal
    Display/Shaders/rectangle_shaders.metal
    # Metal/MetalDevice.mm
    # Metal/MetalDevice.h
)

# Set target properties
set_target_properties(SuperTerminalFramework PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    OBJCXX_STANDARD 17
    OBJCXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# Include directories
target_include_directories(SuperTerminalFramework
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/API
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Display
        ${CMAKE_CURRENT_SOURCE_DIR}/Audio
        ${CMAKE_CURRENT_SOURCE_DIR}/Input
        ${CMAKE_CURRENT_SOURCE_DIR}/Assets
        ${CMAKE_CURRENT_SOURCE_DIR}/Cart
        ${CMAKE_CURRENT_SOURCE_DIR}/Tilemap
        ${CMAKE_CURRENT_SOURCE_DIR}/Particles
        ${CMAKE_CURRENT_SOURCE_DIR}/Metal
        ${CMAKE_CURRENT_SOURCE_DIR}/Editor
        ${CMAKE_CURRENT_SOURCE_DIR}/Debug
        ${SIDPLAYFP_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/simple_lua_formatter
)

# Find SQLite3 (system library on macOS)
find_library(SQLITE3_LIBRARY sqlite3 REQUIRED)
message(STATUS "Found SQLite3: ${SQLITE3_LIBRARY}")

# Link frameworks
target_link_libraries(SuperTerminalFramework
    PUBLIC
        ${FOUNDATION_FRAMEWORK}
        ${APPKIT_FRAMEWORK}
        ${METAL_FRAMEWORK}
        ${METALKIT_FRAMEWORK}
        ${QUARTZCORE_FRAMEWORK}
        ${COREAUDIO_FRAMEWORK}
        ${AUDIOUNIT_FRAMEWORK}
        ${AUDIOTOOLBOX_FRAMEWORK}
        ${SQLITE3_LIBRARY}
        ${SIDPLAYFP_LIBRARY}
        simple-lua-format-lib
        z
    PRIVATE
        basic_formatter_lib
)

# Compiler definitions
target_compile_definitions(SuperTerminalFramework
    PRIVATE
        $<$<CONFIG:Debug>:ST_DEBUG>
        $<$<CONFIG:Release>:ST_RELEASE>
)

# Enable ARC for Objective-C++ files
set_source_files_properties(
    BaseRunner.mm
    LuaBaseRunner.mm
    Metal/FontAtlas.mm
    Metal/MetalRenderer.mm
    Display/DisplayManager.mm
    Display/SpriteManager.mm
    Display/TextDisplayManager.mm
    Display/GraphicsRenderer.mm
    Audio/AudioManager.mm
    Audio/SynthEngine.mm
    Audio/MidiEngine.mm
    Audio/ABCParser.mm
    Audio/CoreAudioEngine.mm
    Tilemap/TilemapRenderer.mm
    Tilemap/TilesetLoader.mm
    Tilemap/Tileset.mm
    Editor/ScriptDatabase.mm
    Editor/EditorStatusBar.mm
    Editor/ScriptBrowserDialog.mm
    Editor/FindDialogs.mm
    Editor/EditorAPI.mm
    API/st_api_tilemap.mm
    API/st_api_sprites.mm
    API/st_sprite_drawing_utils.mm
    API/st_api_file_drawing.mm
    API/st_api_tileset_drawing.mm
    API/st_api_rectangles.mm
    API/st_api_circles.mm
    API/st_api_lines.mm
    API/st_api_polygons.mm
    API/st_api_stars.mm
    Display/RectangleManager.mm
    Display/CircleManager.mm
    Display/LineManager.mm
    Display/DiamondManager.mm
    Display/StarManager.mm
    # Metal/MetalDevice.mm
    PROPERTIES
        COMPILE_FLAGS "-fobjc-arc"
)

# Metal shader compilation
set_target_properties(SuperTerminalFramework PROPERTIES
    RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/Metal/TextRenderer.metal;${CMAKE_CURRENT_SOURCE_DIR}/Metal/TilemapRenderer.metal;${CMAKE_CURRENT_SOURCE_DIR}/Display/Shaders/rectangle_shaders.metal;${CMAKE_CURRENT_SOURCE_DIR}/Display/Shaders/diamond_shaders.metal;${CMAKE_CURRENT_SOURCE_DIR}/Display/Shaders/star_shaders.metal"
)

# Export headers for other targets
target_include_directories(SuperTerminalFramework
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

message(STATUS "Framework library configured")
