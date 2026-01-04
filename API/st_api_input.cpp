//
// st_api_input.cpp
// SuperTerminal v2 - C API Input Implementation
//

#include "superterminal_api.h"
#include "st_api_context.h"
#include "../Input/InputManager.h"

using namespace SuperTerminal;
using namespace STApi;

// Map STKeyCode to InputManager KeyCode
static KeyCode mapKeyCode(STKeyCode key) {
    return static_cast<KeyCode>(key);
}

// Map STMouseButton to InputManager MouseButton
static MouseButton mapMouseButton(STMouseButton button) {
    return static_cast<MouseButton>(button);
}

// =============================================================================
// Input API - Keyboard
// =============================================================================

ST_API bool st_key_pressed(STKeyCode key) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.input(), "InputManager", false);
    return ST_CONTEXT.input()->isKeyPressed(mapKeyCode(key));
}

ST_API bool st_key_just_pressed(STKeyCode key) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.input(), "InputManager", false);
    return ST_CONTEXT.input()->isKeyJustPressed(mapKeyCode(key));
}

ST_API bool st_key_just_released(STKeyCode key) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.input(), "InputManager", false);
    return ST_CONTEXT.input()->isKeyJustReleased(mapKeyCode(key));
}

ST_API uint32_t st_key_get_char(void) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.input(), "InputManager", 0);
    return ST_CONTEXT.input()->getNextCharacter();
}

ST_API void st_key_clear_buffer(void) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.input(), "InputManager");
    ST_CONTEXT.input()->clearCharacterBuffer();
}

ST_API void st_key_clear_all(void) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.input(), "InputManager");
    
    // Clear all input state (keys, mouse, character buffer)
    ST_CONTEXT.input()->clearAll();
}

// =============================================================================
// Input API - Mouse
// =============================================================================

ST_API void st_mouse_position(int* x, int* y) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.input(), "InputManager");
    ST_CONTEXT.input()->getMousePosition(x, y);
}

ST_API void st_mouse_grid_position(int* x, int* y) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.input(), "InputManager");
    ST_CONTEXT.input()->getMouseGridPosition(x, y);
}

ST_API bool st_mouse_button(STMouseButton button) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.input(), "InputManager", false);
    return ST_CONTEXT.input()->isMouseButtonPressed(mapMouseButton(button));
}

ST_API bool st_mouse_button_just_pressed(STMouseButton button) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.input(), "InputManager", false);
    return ST_CONTEXT.input()->isMouseButtonJustPressed(mapMouseButton(button));
}

ST_API bool st_mouse_button_just_released(STMouseButton button) {
    ST_LOCK;
    ST_CHECK_PTR_RET(ST_CONTEXT.input(), "InputManager", false);
    return ST_CONTEXT.input()->isMouseButtonJustReleased(mapMouseButton(button));
}

ST_API void st_mouse_wheel(float* dx, float* dy) {
    ST_LOCK;
    ST_CHECK_PTR(ST_CONTEXT.input(), "InputManager");
    ST_CONTEXT.input()->getMouseWheel(dx, dy);
}