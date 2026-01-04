//
// InputManager.cpp
// SuperTerminal v2
//
// Input system for keyboard and mouse events.
//

#include "InputManager.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>  // For atoi
#include <unordered_map>
#include <string>
#include <Carbon/Carbon.h>  // For macOS key codes

namespace SuperTerminal {

// =============================================================================
// Internal Implementation
// =============================================================================

struct InputManager::Impl {
    // Keyboard state
    std::array<bool, MAX_KEYS> keyState{};           // Current state
    std::array<bool, MAX_KEYS> keyPressedThisFrame{};  // Just pressed
    std::array<bool, MAX_KEYS> keyReleasedThisFrame{}; // Just released
    std::array<bool, MAX_KEYS> keyStatePrevFrame{};    // Previous frame state
    
    // Character input buffer
    std::vector<uint32_t> characterBuffer;
    
    // Compose mode state
    bool composeMode = false;
    std::string composeSequence;
    
    // Mouse state
    int mouseX = 0;
    int mouseY = 0;
    float cellWidth = 8.0f;
    float cellHeight = 16.0f;
    
    std::array<bool, 3> mouseButtonState{};
    std::array<bool, 3> mouseButtonPressedThisFrame{};
    std::array<bool, 3> mouseButtonReleasedThisFrame{};
    std::array<bool, 3> mouseButtonStatePrevFrame{};
    
    float mouseWheelDx = 0.0f;
    float mouseWheelDy = 0.0f;
    
    // Double-click detection
    double lastClickTime = 0.0;
    int lastClickX = -1;
    int lastClickY = -1;
    bool doubleClickDetected = false;
    static constexpr double DOUBLE_CLICK_TIME = 0.5;  // 500ms
    static constexpr int DOUBLE_CLICK_DISTANCE = 5;   // 5 pixels
    
    // Thread safety
    mutable std::mutex mutex;
    
    // Helper: get array index from KeyCode
    static size_t keyIndex(KeyCode keyCode) {
        int code = static_cast<int>(keyCode);
        return (code >= 0 && code < MAX_KEYS) ? code : 0;
    }
    
    // Helper: get array index from MouseButton
    static size_t buttonIndex(MouseButton button) {
        return static_cast<size_t>(button);
    }
};

// =============================================================================
// Constructor / Destructor
// =============================================================================

InputManager::InputManager()
    : m_impl(std::make_unique<Impl>())
{
}

InputManager::~InputManager() = default;

InputManager::InputManager(InputManager&&) noexcept = default;
InputManager& InputManager::operator=(InputManager&&) noexcept = default;

// =============================================================================
// Frame Management
// =============================================================================

void InputManager::beginFrame() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    
    // Note: Wheel deltas are now cleared in endFrame() so they persist for the entire frame
    // This allows the editor to read them in updateEditor() which happens after beginFrame()
}

void InputManager::endFrame() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    
    // Clear "just pressed/released" flags at end of frame
    // (after game logic has had a chance to read them)
    m_impl->keyPressedThisFrame.fill(false);
    m_impl->keyReleasedThisFrame.fill(false);
    m_impl->mouseButtonPressedThisFrame.fill(false);
    m_impl->mouseButtonReleasedThisFrame.fill(false);
    
    // Clear wheel deltas at end of frame (after editor has read them)
    m_impl->mouseWheelDx = 0.0f;
    m_impl->mouseWheelDy = 0.0f;
    
    // Clear double-click flag at end of frame
    m_impl->doubleClickDetected = false;
    
    // Update previous frame state for next frame's edge detection
    m_impl->keyStatePrevFrame = m_impl->keyState;
    m_impl->mouseButtonStatePrevFrame = m_impl->mouseButtonState;
}

// =============================================================================
// Event Processing
// =============================================================================

void InputManager::handleKeyDown(KeyCode keyCode) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    
    size_t idx = Impl::keyIndex(keyCode);
    
    // If key wasn't pressed before, mark as "just pressed"
    if (!m_impl->keyState[idx]) {
        m_impl->keyPressedThisFrame[idx] = true;
    }
    
    m_impl->keyState[idx] = true;
}

void InputManager::handleKeyUp(KeyCode keyCode) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    
    size_t idx = Impl::keyIndex(keyCode);
    
    // If key was pressed before, mark as "just released"
    if (m_impl->keyState[idx]) {
        m_impl->keyReleasedThisFrame[idx] = true;
    }
    
    m_impl->keyState[idx] = false;
}

void InputManager::handleCharacterInput(uint32_t character) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    
    // Handle compose mode
    if (m_impl->composeMode) {
        // Only accept printable ASCII for compose sequences
        if (character >= 32 && character < 127) {
            m_impl->composeSequence += static_cast<char>(character);
            
            // Check if sequence is complete
            bool complete = false;
            uint8_t result = 0;
            
            // Check if we have digits - if so, wait for 3 digits
            bool firstIsDigit = (m_impl->composeSequence[0] >= '0' && m_impl->composeSequence[0] <= '9');
            
            if (firstIsDigit) {
                // Wait for 3 digits
                if (m_impl->composeSequence.length() == 3) {
                    bool allDigits = true;
                    for (char c : m_impl->composeSequence) {
                        if (c < '0' || c > '9') {
                            allDigits = false;
                            break;
                        }
                    }
                    if (allDigits) {
                        int code = std::atoi(m_impl->composeSequence.c_str());
                        if (code >= 128 && code <= 255) {
                            result = static_cast<uint8_t>(code);
                            complete = true;
                        } else {
                            // Invalid code - cancel
                            complete = false;
                            m_impl->composeMode = false;
                            m_impl->composeSequence.clear();
                            return;
                        }
                    }
                }
                // Not 3 digits yet - keep waiting
            } else {
                // Try 2-character digraph
                if (m_impl->composeSequence.length() == 2) {
                    result = handleComposeSequence(m_impl->composeSequence[0], m_impl->composeSequence[1]);
                    if (result >= 128) {
                        complete = true;
                    }
                }
            }
            
            if (complete) {
                // Insert the composed character
                m_impl->characterBuffer.push_back(result);
                // Add a right arrow key press to move cursor after the inserted character
                m_impl->characterBuffer.push_back(0xF703); // Right arrow special code
                m_impl->composeMode = false;
                m_impl->composeSequence.clear();
            } else if (m_impl->composeSequence.length() >= 3) {
                // Sequence failed - cancel compose mode
                m_impl->composeMode = false;
                m_impl->composeSequence.clear();
            }
        } else {
            // Non-printable character cancels compose mode
            m_impl->composeMode = false;
            m_impl->composeSequence.clear();
        }
        
        return; // Don't add to normal character buffer
    }
    
    // Add to character buffer (with limit)
    if (m_impl->characterBuffer.size() < MAX_CHAR_BUFFER) {
        m_impl->characterBuffer.push_back(character);
    }
}

void InputManager::handleCharacterInputWithModifiers(uint32_t character, bool shift, bool ctrl, bool alt, bool cmd) {
    // Just pass through to regular character input
    handleCharacterInput(character);
}

void InputManager::handleMouseMove(int x, int y) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    
    m_impl->mouseX = x;
    m_impl->mouseY = y;
}

void InputManager::handleMouseButtonDown(MouseButton button) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    
    size_t idx = Impl::buttonIndex(button);
    
    // If button wasn't pressed before, mark as "just pressed"
    if (!m_impl->mouseButtonState[idx]) {
        m_impl->mouseButtonPressedThisFrame[idx] = true;
    }
    
    m_impl->mouseButtonState[idx] = true;
}

void InputManager::handleMouseButtonUp(MouseButton button) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    
    size_t idx = Impl::buttonIndex(button);
    
    // Mark as "just released"
    if (m_impl->mouseButtonState[idx]) {
        m_impl->mouseButtonReleasedThisFrame[idx] = true;
    }
    
    m_impl->mouseButtonState[idx] = false;
}

void InputManager::handleMouseButtonDownWithTime(MouseButton button, double timestamp) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    
    size_t idx = Impl::buttonIndex(button);
    
    // Check for double-click (only for left button)
    if (button == MouseButton::Left) {
        double timeSinceLastClick = timestamp - m_impl->lastClickTime;
        int dx = m_impl->mouseX - m_impl->lastClickX;
        int dy = m_impl->mouseY - m_impl->lastClickY;
        int distSquared = dx * dx + dy * dy;
        
        if (timeSinceLastClick > 0 && 
            timeSinceLastClick <= Impl::DOUBLE_CLICK_TIME &&
            distSquared <= (Impl::DOUBLE_CLICK_DISTANCE * Impl::DOUBLE_CLICK_DISTANCE)) {
            m_impl->doubleClickDetected = true;
        }
        
        // Update last click info
        m_impl->lastClickTime = timestamp;
        m_impl->lastClickX = m_impl->mouseX;
        m_impl->lastClickY = m_impl->mouseY;
    }
    
    // If button wasn't pressed before, mark as "just pressed"
    if (!m_impl->mouseButtonState[idx]) {
        m_impl->mouseButtonPressedThisFrame[idx] = true;
    }
    
    m_impl->mouseButtonState[idx] = true;
}

void InputManager::handleMouseWheel(float dx, float dy) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    
    m_impl->mouseWheelDx += dx;
    m_impl->mouseWheelDy += dy;
}

// =============================================================================
// Keyboard State Query
// =============================================================================

bool InputManager::isKeyPressed(KeyCode keyCode) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    
    size_t idx = Impl::keyIndex(keyCode);
    return m_impl->keyState[idx];
}

bool InputManager::isKeyJustPressed(KeyCode keyCode) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    

    size_t idx = Impl::keyIndex(keyCode);
    bool result = m_impl->keyPressedThisFrame[idx];
    
    return result;
}

bool InputManager::isKeyJustReleased(KeyCode keyCode) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    
    size_t idx = Impl::keyIndex(keyCode);
    return m_impl->keyReleasedThisFrame[idx];
}

bool InputManager::isAnyModifierPressed() const {
    return isShiftPressed() || isControlPressed() || 
           isAltPressed() || isCommandPressed();
}

bool InputManager::isShiftPressed() const {
    return isKeyPressed(KeyCode::LeftShift) || isKeyPressed(KeyCode::RightShift);
}

bool InputManager::isControlPressed() const {
    return isKeyPressed(KeyCode::LeftControl) || isKeyPressed(KeyCode::RightControl);
}

bool InputManager::isAltPressed() const {
    return isKeyPressed(KeyCode::LeftAlt) || isKeyPressed(KeyCode::RightAlt);
}

bool InputManager::isCommandPressed() const {
    return isKeyPressed(KeyCode::LeftCommand) || isKeyPressed(KeyCode::RightCommand);
}

// =============================================================================
// Character Input Buffer
// =============================================================================

uint32_t InputManager::getNextCharacter() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    
    if (m_impl->characterBuffer.empty()) {
        return 0;
    }
    
    uint32_t ch = m_impl->characterBuffer.front();
    m_impl->characterBuffer.erase(m_impl->characterBuffer.begin());
    return ch;
}

uint32_t InputManager::peekNextCharacter() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    
    if (m_impl->characterBuffer.empty()) {
        return 0;
    }
    
    return m_impl->characterBuffer.front();
}

bool InputManager::hasCharacters() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    
    return !m_impl->characterBuffer.empty();
}

void InputManager::clearCharacterBuffer() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    
    m_impl->characterBuffer.clear();
}

size_t InputManager::getCharacterBufferSize() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    
    return m_impl->characterBuffer.size();
}

// =============================================================================
// Mouse State Query
// =============================================================================

void InputManager::getMousePosition(int* outX, int* outY) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    
    if (outX) *outX = m_impl->mouseX;
    if (outY) *outY = m_impl->mouseY;
}

void InputManager::getMouseGridPosition(int* outGridX, int* outGridY) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    
    if (outGridX) {
        *outGridX = static_cast<int>(m_impl->mouseX / m_impl->cellWidth);
    }
    if (outGridY) {
        *outGridY = static_cast<int>(m_impl->mouseY / m_impl->cellHeight);
    }
}

void InputManager::setCellSize(float cellWidth, float cellHeight) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    
    m_impl->cellWidth = cellWidth > 0.0f ? cellWidth : 8.0f;
    m_impl->cellHeight = cellHeight > 0.0f ? cellHeight : 16.0f;
}

bool InputManager::isMouseButtonPressed(MouseButton button) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    
    size_t idx = Impl::buttonIndex(button);
    return m_impl->mouseButtonState[idx];
}

bool InputManager::isMouseButtonJustPressed(MouseButton button) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    
    size_t idx = Impl::buttonIndex(button);
    return m_impl->mouseButtonPressedThisFrame[idx];
}

bool InputManager::isMouseButtonJustReleased(MouseButton button) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    
    size_t idx = Impl::buttonIndex(button);
    return m_impl->mouseButtonReleasedThisFrame[idx];
}

bool InputManager::isDoubleClick() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->doubleClickDetected;
}

void InputManager::clearDoubleClick() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->doubleClickDetected = false;
}

void InputManager::getMouseWheel(float* outDx, float* outDy) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    
    if (outDx) *outDx = m_impl->mouseWheelDx;
    if (outDy) *outDy = m_impl->mouseWheelDy;
}

// =============================================================================
// Box Drawing Character Input (Compose Key / Digraph System)
// =============================================================================

// Compose key sequence mapping table
// Maps two-character sequences to ASCII codes 128-255
// Format: "xy" -> ASCII code
// Inspired by Vim digraphs for mnemonic input
static const std::unordered_map<std::string, uint8_t> COMPOSE_SEQUENCES = {
    // Single-line box drawing (horizontal/vertical)
    {"hh", 128}, {"--", 128},  // ─ horizontal line
    {"vv", 129}, {"||", 129},  // │ vertical line
    
    // Box corners
    {"dr", 130}, {"ul", 130},  // ┌ down-right / upper-left corner
    {"dl", 131}, {"ur", 131},  // ┐ down-left / upper-right corner
    {"ur", 132}, {"ll", 132},  // └ up-right / lower-left corner
    {"ul", 133}, {"lr", 133},  // ┘ up-left / lower-right corner
    
    // T-junctions
    {"vr", 134}, {"lt", 134},  // ├ vertical-right / left T
    {"vl", 135}, {"rt", 135},  // ┤ vertical-left / right T
    {"hd", 136}, {"tt", 136},  // ┬ horizontal-down / top T
    {"hu", 137}, {"bt", 137},  // ┴ horizontal-up / bottom T
    
    // Cross
    {"vh", 138}, {"++", 138},  // ┼ vertical-horizontal cross
    
    // Heavy lines
    {"HH", 139}, {"==", 139},  // ━ heavy horizontal
    {"VV", 140},               // ┃ heavy vertical
    {"DR", 141},               // ┏ heavy down-right
    {"DL", 142},               // ┓ heavy down-left
    {"UR", 143},               // ┗ heavy up-right
    {"UL", 144},               // ┛ heavy up-left
    
    // Double lines
    {"dh", 150}, {"d-", 150},  // ═ double horizontal
    {"dv", 151}, {"d|", 151},  // ║ double vertical
    {"DD", 154},               // ╔ double down-right
    {"Dd", 157},               // ╗ double down-left
    {"Uu", 160},               // ╚ double up-right
    {"UU", 163},               // ╝ double up-left
    
    // Block elements
    {"ub", 179},               // ▀ upper half block
    {"lb", 180},               // ▄ lower half block
    {"fb", 181}, {"##", 181},  // █ full block
    {"lh", 182},               // ▌ left half block
    {"rh", 183},               // ▐ right half block
    
    // Shading
    {"s1", 184}, {"..", 184},  // ░ light shade
    {"s2", 185}, {"::", 185},  // ▒ medium shade
    {"s3", 186}, {"%%", 186},  // ▓ dark shade
    
    // Geometric shapes
    {"sq", 200}, {"[]", 200},  // ■ black square
    {"ci", 210}, {"()", 210},  // ● black circle
    {"sm", 213}, {":)", 213},  // ☺ smiley
    
    // Card suits
    {"sp", 218},               // ♠ spade
    {"cl", 219},               // ♣ club
    {"he", 220}, {"<3", 220},  // ♥ heart
    {"di", 221},               // ♦ diamond
    
    // Music notes
    {"mu", 222}, {"m1", 222},  // ♪ eighth note
    {"mm", 223}, {"m2", 223},  // ♫ beamed eighth notes
    
    // Mathematical symbols
    {"de", 240}, {"DG", 240},  // ° degree
    {"+-", 241}, {"pm", 241},  // ± plus-minus
    {"12", 244}, {"hf", 244},  // ½ one half
    {"14", 252},               // ¼ one quarter
    {"34", 253},               // ¾ three quarters
};

void InputManager::setComposeKeyEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    if (!enabled && m_impl->composeMode) {
        // Cancel any active compose sequence
        m_impl->composeMode = false;
        m_impl->composeSequence.clear();
    }
}

bool InputManager::isComposeKeyEnabled() const {
    return false; // Always disabled - handled in editor
}

bool InputManager::isInComposeSequence() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->composeMode && !m_impl->composeSequence.empty();
}

std::string InputManager::getComposeSequenceDisplay() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    if (m_impl->composeMode && !m_impl->composeSequence.empty()) {
        return "Compose: " + m_impl->composeSequence;
    }
    return "";
}

void InputManager::enterComposeMode() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->composeMode = true;
    m_impl->composeSequence.clear();
}

uint8_t InputManager::handleComposeSequence(char key1, char key2) {
    // Build sequence string
    std::string seq;
    seq += key1;
    seq += key2;
    
    // Look up in table
    auto it = COMPOSE_SEQUENCES.find(seq);
    if (it != COMPOSE_SEQUENCES.end()) {
        return it->second;
    }
    
    return 0; // Invalid sequence
}

// =============================================================================
// Utility
// =============================================================================

void InputManager::clearAll() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    
    m_impl->keyState.fill(false);
    m_impl->keyPressedThisFrame.fill(false);
    m_impl->keyReleasedThisFrame.fill(false);
    m_impl->keyStatePrevFrame.fill(false);
    m_impl->characterBuffer.clear();
    m_impl->mouseButtonState.fill(false);
    m_impl->mouseButtonPressedThisFrame.fill(false);
    m_impl->mouseButtonReleasedThisFrame.fill(false);
    m_impl->mouseButtonStatePrevFrame.fill(false);
    m_impl->mouseWheelDx = 0.0f;
    m_impl->mouseWheelDy = 0.0f;
}

KeyCode InputManager::convertMacKeyCode(unsigned short macKeyCode) {
    // macOS virtual key codes to our KeyCode mapping
    // Reference: https://eastmanreference.com/complete-list-of-applescript-key-codes
    switch (macKeyCode) {
        // Letters
        case kVK_ANSI_A: return KeyCode::A;
        case kVK_ANSI_B: return KeyCode::B;
        case kVK_ANSI_C: return KeyCode::C;
        case kVK_ANSI_D: return KeyCode::D;
        case kVK_ANSI_E: return KeyCode::E;
        case kVK_ANSI_F: return KeyCode::F;
        case kVK_ANSI_G: return KeyCode::G;
        case kVK_ANSI_H: return KeyCode::H;
        case kVK_ANSI_I: return KeyCode::I;
        case kVK_ANSI_J: return KeyCode::J;
        case kVK_ANSI_K: return KeyCode::K;
        case kVK_ANSI_L: return KeyCode::L;
        case kVK_ANSI_M: return KeyCode::M;
        case kVK_ANSI_N: return KeyCode::N;
        case kVK_ANSI_O: return KeyCode::O;
        case kVK_ANSI_P: return KeyCode::P;
        case kVK_ANSI_Q: return KeyCode::Q;
        case kVK_ANSI_R: return KeyCode::R;
        case kVK_ANSI_S: return KeyCode::S;
        case kVK_ANSI_T: return KeyCode::T;
        case kVK_ANSI_U: return KeyCode::U;
        case kVK_ANSI_V: return KeyCode::V;
        case kVK_ANSI_W: return KeyCode::W;
        case kVK_ANSI_X: return KeyCode::X;
        case kVK_ANSI_Y: return KeyCode::Y;
        case kVK_ANSI_Z: return KeyCode::Z;
        
        // Numbers
        case kVK_ANSI_1: return KeyCode::Num1;
        case kVK_ANSI_2: return KeyCode::Num2;
        case kVK_ANSI_3: return KeyCode::Num3;
        case kVK_ANSI_4: return KeyCode::Num4;
        case kVK_ANSI_5: return KeyCode::Num5;
        case kVK_ANSI_6: return KeyCode::Num6;
        case kVK_ANSI_7: return KeyCode::Num7;
        case kVK_ANSI_8: return KeyCode::Num8;
        case kVK_ANSI_9: return KeyCode::Num9;
        case kVK_ANSI_0: return KeyCode::Num0;
        
        // Special keys
        case kVK_Return: return KeyCode::Enter;
        case kVK_Escape: return KeyCode::Escape;
        case kVK_Delete: return KeyCode::Backspace;
        case kVK_Tab: return KeyCode::Tab;
        case kVK_Space: return KeyCode::Space;
        
        // Arrow keys
        case kVK_RightArrow: return KeyCode::Right;
        case kVK_LeftArrow: return KeyCode::Left;
        case kVK_DownArrow: return KeyCode::Down;
        case kVK_UpArrow: return KeyCode::Up;
        
        // Page navigation keys
        case 116: return KeyCode::PageUp;    // kVK_PageUp
        case 121: return KeyCode::PageDown;  // kVK_PageDown
        
        // Function keys
        case kVK_F1: return KeyCode::F1;
        case kVK_F2: return KeyCode::F2;
        case kVK_F3: return KeyCode::F3;
        case kVK_F4: return KeyCode::F4;
        case kVK_F5: return KeyCode::F5;
        case kVK_F6: return KeyCode::F6;
        case kVK_F7: return KeyCode::F7;
        case kVK_F8: return KeyCode::F8;
        case kVK_F9: return KeyCode::F9;
        case kVK_F10: return KeyCode::F10;
        case kVK_F11: return KeyCode::F11;
        case kVK_F12: return KeyCode::F12;
        
        // Modifiers
        case kVK_Shift: return KeyCode::LeftShift;
        case kVK_Control: return KeyCode::LeftControl;
        case kVK_Option: return KeyCode::LeftAlt;
        case kVK_Command: return KeyCode::LeftCommand;
        case kVK_RightShift: return KeyCode::RightShift;
        case kVK_RightControl: return KeyCode::RightControl;
        case kVK_RightOption: return KeyCode::RightAlt;
        // Note: macOS doesn't distinguish left/right Command in virtual key codes
        
        default:
            return KeyCode::Unknown;
    }
}

const char* InputManager::keyCodeToString(KeyCode keyCode) {
    switch (keyCode) {
        case KeyCode::Unknown: return "Unknown";
        
        // Letters
        case KeyCode::A: return "A";
        case KeyCode::B: return "B";
        case KeyCode::C: return "C";
        case KeyCode::D: return "D";
        case KeyCode::E: return "E";
        case KeyCode::F: return "F";
        case KeyCode::G: return "G";
        case KeyCode::H: return "H";
        case KeyCode::I: return "I";
        case KeyCode::J: return "J";
        case KeyCode::K: return "K";
        case KeyCode::L: return "L";
        case KeyCode::M: return "M";
        case KeyCode::N: return "N";
        case KeyCode::O: return "O";
        case KeyCode::P: return "P";
        case KeyCode::Q: return "Q";
        case KeyCode::R: return "R";
        case KeyCode::S: return "S";
        case KeyCode::T: return "T";
        case KeyCode::U: return "U";
        case KeyCode::V: return "V";
        case KeyCode::W: return "W";
        case KeyCode::X: return "X";
        case KeyCode::Y: return "Y";
        case KeyCode::Z: return "Z";
        
        // Numbers
        case KeyCode::Num1: return "1";
        case KeyCode::Num2: return "2";
        case KeyCode::Num3: return "3";
        case KeyCode::Num4: return "4";
        case KeyCode::Num5: return "5";
        case KeyCode::Num6: return "6";
        case KeyCode::Num7: return "7";
        case KeyCode::Num8: return "8";
        case KeyCode::Num9: return "9";
        case KeyCode::Num0: return "0";
        
        // Special keys
        case KeyCode::Enter: return "Enter";
        case KeyCode::Escape: return "Escape";
        case KeyCode::Backspace: return "Backspace";
        case KeyCode::Tab: return "Tab";
        case KeyCode::Space: return "Space";
        
        // Arrow keys
        case KeyCode::Right: return "Right";
        case KeyCode::Left: return "Left";
        case KeyCode::Down: return "Down";
        case KeyCode::Up: return "Up";
        
        // Page navigation
        case KeyCode::PageUp: return "PageUp";
        case KeyCode::PageDown: return "PageDown";
        
        // Function keys
        case KeyCode::F1: return "F1";
        case KeyCode::F2: return "F2";
        case KeyCode::F3: return "F3";
        case KeyCode::F4: return "F4";
        case KeyCode::F5: return "F5";
        case KeyCode::F6: return "F6";
        case KeyCode::F7: return "F7";
        case KeyCode::F8: return "F8";
        case KeyCode::F9: return "F9";
        case KeyCode::F10: return "F10";
        case KeyCode::F11: return "F11";
        case KeyCode::F12: return "F12";
        
        // Modifiers
        case KeyCode::LeftShift: return "LeftShift";
        case KeyCode::LeftControl: return "LeftControl";
        case KeyCode::LeftAlt: return "LeftAlt";
        case KeyCode::LeftCommand: return "LeftCommand";
        case KeyCode::RightShift: return "RightShift";
        case KeyCode::RightControl: return "RightControl";
        case KeyCode::RightAlt: return "RightAlt";
        case KeyCode::RightCommand: return "RightCommand";
        
        default:
            return "Unknown";
    }
}

} // namespace SuperTerminal